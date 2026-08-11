/*******************************************************************************
 * file           : i3c_target.c
 * brief          : I3C target implementation
 ******************************************************************************/
#include <string.h>
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/queue.h"
#include "../../Shared/Debug/swd_printf.h"
#include "i3c_target.h"

/* Custom Command codes are in the range 0xC0 to 0xDF */
/*
#define I3C_STORE_CMD             0xC0
#define I3C_LOAD_CMD              0xC1
*/
// Interrupt callback
static void I3C_NotifyCallback(hal_i3c_handle_t *hi3c, uint32_t ulNotifyId);
static void I3C_ErrorCallback(hal_i3c_handle_t *hi3c);
static void I3C_RxCompleteCallback(hal_i3c_handle_t *hi3c);
static void I3C_TxCompleteCallback(hal_i3c_handle_t *hi3c);

//#define RX_BUFFER_SIZE 16
//static uint8_t I3C_RxBuffer[RX_BUFFER_SIZE];
//#define TX_BUFFER_SIZE 16
//static uint8_t I3C_TxBuffer[TX_BUFFER_SIZE];

static volatile QueueHandle_t sI3CNotifyQueue;
static volatile QueueHandle_t sI3CIntQueue;

// The I3C state
static uint32_t I3C_NotificationsReceived;
static uint8_t I3C_ucLastCommand;
static uint16_t I3C_uwLastAddress;

typedef enum {
  RX_IDLE,
  RX_COMMAND_PENDING,
  RX_PAYLOAD_PENDING,
} RX_STATE;

static RX_STATE I3C_RxState;

typedef enum {
  TX_IDLE,
  TX_PENDING
} TX_STATE;

static TX_STATE I3C_TxState;

#define I3C_TGT_READY (HAL_I3C_TGT_NOTIFICATION_DAU | \
                        HAL_I3C_TGT_NOTIFICATION_SETMRL | \
                        HAL_I3C_TGT_NOTIFICATION_SETMWL)

static void I3C_Reset();
//static hal_status_t I3C_ReadCommand();
//static hal_status_t I3C_ReadPayload(uint32_t ulPayloadLength);

/*
 * @brief  Initialize I3C
 *
 * @param I3cNotifyQueue The queue for notifying the task of events
 * @param I3cIntQueue The queue for notifying of interrupt events
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t I3C_Init(QueueHandle_t I3cNotifyQueue, QueueHandle_t I3cIntQueue) {
  hal_status_t status;
  hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();

  status = HAL_I3C_RegisterNotifyCallback(hI3C, I3C_NotifyCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_RegisterNotifyCallback failed.\n");
    return status;
  }

  status = HAL_I3C_TGT_RegisterRxCpltCallback(hI3C, I3C_RxCompleteCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_TGT_RegisterRxCpltCallback failed.\n");
    return status;
  }

  status = HAL_I3C_TGT_RegisterTxCpltCallback(hI3C, I3C_TxCompleteCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_TGT_RegisterTxCpltCallback failed.\n");
    return status;
  }

  status = HAL_I3C_RegisterErrorCallback(hI3C, I3C_ErrorCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_RegisterErrorCallback failed.\n");
    return status;
  }

  // Activate target notifications for DAA (Dynamic Address Assignment)
  status = HAL_I3C_TGT_ActivateNotification(hI3C, (uint8_t *)NULL, 0U,
                        HAL_I3C_TGT_NOTIFICATION_DAU |
                        HAL_I3C_TGT_NOTIFICATION_SETMRL |
                        HAL_I3C_TGT_NOTIFICATION_SETMWL);

  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_TGT_ActivateNotification failed.\n");
    return status;
  }

  sI3CNotifyQueue = I3cNotifyQueue;
  sI3CIntQueue = I3cIntQueue;

  I3C_Reset();
  HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, HAL_GPIO_PIN_SET);

  return HAL_OK;
}

/*
 * @brief  I3C notify handler
 *
 * @param ulNotifyId The notification
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t I3C_Notify(uint32_t ulNotifyId) {
  hal_status_t status;
  hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();
  hal_i3c_ccc_info_t CCCInfo;

  if ((ulNotifyId & HAL_I3C_TGT_NOTIFICATION_DAU) ==
      HAL_I3C_TGT_NOTIFICATION_DAU) {
    // Retrieve Common Command Code info to check dynamic address assignment
    status = HAL_I3C_GetCCCInfo(hI3C, HAL_I3C_TGT_NOTIFICATION_DAU, &CCCInfo);
    if (status != HAL_OK) {
      SWD_printf("HAL_I3C_GetCCCInfo DAU failed.\n");
      return status;
    }

    // DAA completed
    if (CCCInfo.dynamic_addr != 0) {
      I3C_NotificationsReceived |= HAL_I3C_TGT_NOTIFICATION_DAU;

      SWD_printf("Dynamic address complete: %02xh.\n", CCCInfo.dynamic_addr);
    } else {
      SWD_printf("Dynamic address cleared.\n");
    }
  }

  // SETMRL: Dictates the maximum number of bytes a Target can return to an
  // I3C Controller in a single read transfer.
  if ((ulNotifyId & HAL_I3C_TGT_NOTIFICATION_SETMRL) ==
      HAL_I3C_TGT_NOTIFICATION_SETMRL) {
    status = HAL_I3C_GetCCCInfo(hI3C, HAL_I3C_TGT_NOTIFICATION_SETMRL,
        &CCCInfo);
    if (status != HAL_OK) {
      // Error occurred while retrieving CCC info.
      SWD_printf("HAL_I3C_GetCCCInfo SETMRL failed.\n");
      return status;
    }

    // SETMRL completed
    I3C_NotificationsReceived |= HAL_I3C_TGT_NOTIFICATION_SETMRL;

    SWD_printf("SETMRL complete: %d.\n", CCCInfo.max_read_data_size_byte);
  }

  // SETMWL: Dictates the maximum number of bytes an I3C Controller can send to
  // a Target in a single write transfer.
  if ((ulNotifyId & HAL_I3C_TGT_NOTIFICATION_SETMWL) ==
      HAL_I3C_TGT_NOTIFICATION_SETMWL) {
    status = HAL_I3C_GetCCCInfo(hI3C, HAL_I3C_TGT_NOTIFICATION_SETMWL,
        &CCCInfo);
    if (status != HAL_OK) {
      // Error occurred while retrieving CCC info.
      SWD_printf("HAL_I3C_GetCCCInfo SETMWL failed.\n");
      return status;
    }

    // SETMWL completed
    I3C_NotificationsReceived |= HAL_I3C_TGT_NOTIFICATION_SETMWL;

    SWD_printf("SETMWL complete: %d.\n", CCCInfo.max_write_data_size_byte);
  }

  /*
  if (I3C_NotificationsReceived == I3C_TGT_READY) {
    // Start reading data from the controller
    I3C_ReadCommand();
  }
 */
  return HAL_OK;
}

/*
 * @brief: Rx complete handler
 */
hal_status_t I3C_RxComplete() {
  I3C_RxState = RX_IDLE;
/*
  if (I3C_RxState == RX_COMMAND_PENDING) {
    I3C_RxState = RX_IDLE;
    // Get the command
    uint8_t ucCommand = I3C_RxBuffer[0];

    // Get the address
    uint16_t uwAddress = I3C_RxBuffer[1];
    uwAddress <<= 8;
    uwAddress |= I3C_RxBuffer[2];

    SWD_printf("I3C_RxComplete: cmd: %02xh, address: %04xh\n",
        ucCommand, uwAddress);

    I3C_ucLastCommand = ucCommand;
    I3C_uwLastAddress = uwAddress;
    switch(ucCommand) {
    case I3C_STORE_CMD: {
      I3C_ReadPayload(4);
      break;
    }

    case I3C_LOAD_CMD: {
      if (I3C_TxState != TX_IDLE) {
        SWD_printf("I3C_RxComplete: [Error] Tx is pending.\n");
        return HAL_BUSY;
      }

      // Send the response
      I3C_TxBuffer[0] = 0xaa;
      I3C_TxBuffer[1] = 0xbb;
      I3C_TxBuffer[2] = 0xcc;
      I3C_TxBuffer[3] = 0xdd;

      hal_status_t status;
      hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();
      status = HAL_I3C_TGT_Transmit_IT(hI3C, I3C_TxBuffer, 4);
      if (status != HAL_OK) {
        SWD_printf("I3C_RxComplete: HAL_I3C_TGT_Transmit_IT: %lx\n", status);
        return status;
      }

      I3C_TxState = TX_PENDING;
      break;
    }

    default: {
      SWD_printf("I3C_RxComplete: Unhandled cmd: %02x, address: %04x\n",
          ucCommand, uwAddress);
      break;
    }
    }
  } else if (I3C_RxState == RX_PAYLOAD_PENDING) {
    I3C_RxState = RX_IDLE;
    hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();

    switch (I3C_ucLastCommand) {
    case I3C_STORE_CMD: {
      SWD_printf("I3C_RxComplete: ");
      for (uint32_t i = 0; i < hI3C->data_size_byte; i++) {
        SWD_printf("%02x", I3C_RxBuffer[i]);
      }
      SWD_printf("\n");
      break;
    }

    case I3C_LOAD_CMD: {
      SWD_printf("I3C_RxComplete: Invalid command for payload.\n");
      break;
    }

    default: {
      SWD_printf("I3C_RxComplete: Unhandled cmd: %x\n", I3C_ucLastCommand);
      break;
    }
    }
  }
  */
  return HAL_OK;
}

/*
 * @brief: Tx complete handler
 */
hal_status_t I3C_TxComplete() {
  I3C_TxState = TX_IDLE;
  return HAL_OK;
}

/*
 * brief: I3C target notification callback.
 *
 * @param hi3c The I3C handle
 * @param ulNotifyId The event that occurred
 */
static void I3C_NotifyCallback(hal_i3c_handle_t *hi3c, uint32_t ulNotifyId) {
  SWD_printf("-- I3C_NotifyCallback --: %lx\n", ulNotifyId);
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sI3CNotifyQueue, &ulNotifyId,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * @brief: Rx complete IRQ callback
 *
 * @param hi3c The I3C handle
 */
static void I3C_RxCompleteCallback(hal_i3c_handle_t *hi3c) {
  SWD_printf("-- I3C_RxCompleteCallback [%d bytes]--\n", hi3c->data_size_byte);
  uint8_t ucEvent = EVENT_RX_COMPLETE;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sI3CIntQueue, &ucEvent,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * @brief: Tx complete irq callback
 *
 * @param hi3c The I3C handle
 */
static void I3C_TxCompleteCallback(hal_i3c_handle_t *hi3c) {
  SWD_printf("-- I3C_TxCompleteCallback --\n");
  uint8_t ucEvent = EVENT_TX_COMPLETE;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sI3CIntQueue, &ucEvent,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * brief: I3C error callback.
 *
 * @param hi3c The I3C handle
 */
static void I3C_ErrorCallback(hal_i3c_handle_t *hi3c) {
  uint8_t ucEvent = EVENT_ERROR;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sI3CIntQueue, &ucEvent,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * brief: I3C read command
 */
/*
static hal_status_t I3C_ReadCommand() {
  if (I3C_RxState != RX_IDLE) {
    SWD_printf("I3C_ReadCommand: [Error] Rx is pending.\n");
    return HAL_BUSY;
  }

  hal_status_t status;
  hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();
  status = HAL_I3C_TGT_Receive_IT(hI3C, I3C_RxBuffer, 3);
  if (status != HAL_OK) {
    SWD_printf("I3C_ReadCommand: HAL_I3C_TGT_Receive_IT: %lx\n", status);
    return status;
  }

  I3C_RxState = RX_COMMAND_PENDING;
  return HAL_OK;
}
*/
/*
 * brief: I3C read payload
 *
 * @param ulPayloadLength The Rx payload length
 */
/*
static hal_status_t I3C_ReadPayload(uint32_t ulPayloadLength) {
  if (I3C_RxState != RX_IDLE) {
    SWD_printf("I3C_ReadPayload: [Error] Rx is pending.\n");
    return HAL_BUSY;
  }

  if (ulPayloadLength > RX_BUFFER_SIZE) {
    SWD_printf("I3C_ReadPayload: Rx payload to large: %ld.\n", ulPayloadLength);
    return HAL_ERROR;
  }

  hal_status_t status;
  hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();
  status = HAL_I3C_TGT_Receive_IT(hI3C, I3C_RxBuffer, ulPayloadLength);
  if (status != HAL_OK) {
    SWD_printf("I3C_ReadPayload: HAL_I3C_TGT_Receive_IT: %lx\n", status);
    return status;
  }

  I3C_RxState = RX_PAYLOAD_PENDING;
  return HAL_OK;
}
*/
/*
 * @brief: Reset the target
 */
static void I3C_Reset() {
  // Reset the state
  I3C_NotificationsReceived = 0;
  I3C_RxState = RX_IDLE;
  I3C_TxState = TX_IDLE;
  I3C_ucLastCommand = 0;
  I3C_uwLastAddress = 0;
}
