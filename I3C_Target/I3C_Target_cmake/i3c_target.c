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

/* Custom Command codes are in the range 0xC0 to 0xDF.
 * These commands must match the defines with the same name on the Controller
 * side.
 */
#define I3C_STORE_CMD             0xC0
#define I3C_LOAD_CMD              0xC1

// Interrupt callback
static void I3C_NotifyCallback(hal_i3c_handle_t *hi3c, uint32_t ulNotifyId);
static void I3C_ErrorCallback(hal_i3c_handle_t *hi3c);
static void I3C_RxCompleteCallback(hal_i3c_handle_t *hi3c);
static void I3C_TxCompleteCallback(hal_i3c_handle_t *hi3c);

// The maximum Rx length
static uint32_t I3C_Max_Rx_Buffer_Length = 0;
static uint8_t* I3C_RxBuffer = NULL;
// The maximum Tx length
static uint32_t I3C_Max_Tx_Buffer_Length = 0;

// The I3C state flags
typedef enum {
  STATE_INIT              = 0x00000000,
  STATE_IDLE              = 0x00000001,
  NOTIFICATION_DAU_FLAG   = 0x00000002,
  NOTIFICATION_MRL_FLAG   = 0x00000004,
  NOTIFICATION_MWL_FLAG   = 0x00000008,
  STATE_RX_COMMAND_PENDING = 0x00000010,
  STATE_RX_PAYLOAD_PENDING = 0x00000020,
  STATE_TX_PAYLOAD_PENDING = 0x00000040
} I3C_STATE;

static I3C_STATE I3C_State;

// The memory block that is being accessed over I3C
#define MEM_SIZE 0x4000U
static uint8_t pMem[MEM_SIZE];

static uint8_t I3C_ucLastCommand;
static uint16_t I3C_uwLastAddress;

static volatile QueueHandle_t sI3CNotifyQueue;
static volatile QueueHandle_t sI3CIntQueue;

static void I3C_Reset();
static hal_status_t I3C_ReadCommand();
static hal_status_t I3C_ReadPayload(uint32_t ulPayloadLength);
static hal_status_t I3C_WritePayload(uint32_t ulPayloadLength);

/*
 * @brief  Initialize I3C
 *
 * @param I3CNotifyQueue The queue for notifying the task of events
 * @param I3cIntQueue The queue for notifying of interrupt events
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t I3C_Init(QueueHandle_t I3CNotifyQueue, QueueHandle_t I3CIntQueue) {
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

  sI3CNotifyQueue = I3CNotifyQueue;
  sI3CIntQueue = I3CIntQueue;

  I3C_Reset();

  // The initialization was successful
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

    SWD_printf("Dynamic address: %02xh, ", CCCInfo.dynamic_addr);
    // DAA completed
    if (CCCInfo.dynamic_addr != 0) {
      I3C_State |= NOTIFICATION_DAU_FLAG;
    }
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

    SWD_printf("SETMWL: %02xh, ", CCCInfo.max_write_data_size_byte);
    // Allocate the receive buffer
    I3C_Max_Rx_Buffer_Length = CCCInfo.max_write_data_size_byte;
    if (I3C_RxBuffer != NULL) {
      vPortFree(I3C_RxBuffer);
      I3C_RxBuffer = NULL;
    }
    I3C_RxBuffer = pvPortMalloc(I3C_Max_Rx_Buffer_Length);

    // SETMWL completed
    I3C_State |= NOTIFICATION_MWL_FLAG;
  }

  // SETMRL: Dictates the maximum number of bytes a Target can return to an
  // I3C Controller in a single target read/controller write transfer.
  if ((ulNotifyId & HAL_I3C_TGT_NOTIFICATION_SETMRL) ==
      HAL_I3C_TGT_NOTIFICATION_SETMRL) {
    status = HAL_I3C_GetCCCInfo(hI3C, HAL_I3C_TGT_NOTIFICATION_SETMRL,
        &CCCInfo);
    if (status != HAL_OK) {
      // Error occurred while retrieving CCC info.
      SWD_printf("HAL_I3C_GetCCCInfo SETMRL failed.\n");
      return status;
    }

    SWD_printf("SETMRL: %02xh.\n", CCCInfo.max_read_data_size_byte);
    I3C_Max_Tx_Buffer_Length = CCCInfo.max_read_data_size_byte;

    // SETMRL completed
    I3C_State |= NOTIFICATION_MRL_FLAG;
  }

  if (I3C_State == (NOTIFICATION_DAU_FLAG | NOTIFICATION_MRL_FLAG |
      NOTIFICATION_MWL_FLAG)) {
    // DAA is complete and we received MWL and MRL
    I3C_State = STATE_IDLE;

    // Wait for a command from the controller
    I3C_ReadCommand();
  }

  return HAL_OK;
}

/*
 * @brief: Rx complete handler
 */
hal_status_t I3C_RxComplete() {
  if (I3C_State == STATE_RX_COMMAND_PENDING) {
    I3C_State = STATE_IDLE;
    // Get the command
    I3C_ucLastCommand = I3C_RxBuffer[0];

    // Get the address
    I3C_uwLastAddress = I3C_RxBuffer[1];
    I3C_uwLastAddress <<= 8;
    I3C_uwLastAddress |= I3C_RxBuffer[2];
    // The address can only have 14 bits (the addressed memory is 16KB)
    I3C_uwLastAddress &= 0x3fff;

    // Get the length of the payload
    uint32_t ulLength = I3C_RxBuffer[3];
    switch(I3C_ucLastCommand) {
    case I3C_STORE_CMD: {
      I3C_ReadPayload(ulLength);

      SWD_printf("STORE CMD: %02xh, address: %02xh, length: %d\n",
          I3C_ucLastCommand, I3C_uwLastAddress, ulLength);
      break;
    }

    case I3C_LOAD_CMD: {
      I3C_WritePayload(ulLength);
      break;
    }

    default: {
      SWD_printf(
          "I3C_RxComplete: Unhandled CMD: %02x, address: %02x, length: %d\n",
          I3C_ucLastCommand, I3C_uwLastAddress, ulLength);
      break;
    }
    }
  } else if (I3C_State == STATE_RX_PAYLOAD_PENDING) {
    I3C_State = STATE_IDLE;

    switch (I3C_ucLastCommand) {
    case I3C_STORE_CMD: {
      hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();

      // Check if we would end up writing beyond the end of the buffer.
      uint32_t ulBytesReceived = hI3C->data_size_byte;
      uint32_t ulBytesCopy = (I3C_uwLastAddress + ulBytesReceived < MEM_SIZE) ?
              ulBytesReceived : MEM_SIZE - I3C_uwLastAddress;
      memcpy(pMem + I3C_uwLastAddress, I3C_RxBuffer, ulBytesCopy);

      SWD_printf("STORE PAYLOAD [Copied: %d bytes] @%02x: ",
          ulBytesCopy, I3C_uwLastAddress);
      for (uint32_t i = 0; i < ulBytesCopy; i++) {
        SWD_printf("%02x ", I3C_RxBuffer[i]);
      }
      SWD_printf("\n");

      // Wait for the next command from the controller
      I3C_ReadCommand();
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

  return HAL_OK;
}

/*
 * @brief: Tx complete handler
 */
hal_status_t I3C_TxComplete() {
  if (I3C_State == STATE_TX_PAYLOAD_PENDING) {
    I3C_State = STATE_IDLE;
    // Load payload was sent. Wait for the next command from the controller.
    I3C_ReadCommand();
  }

  return HAL_OK;
}

/*
 * brief: I3C target notification callback.
 *
 * @param hi3c The I3C handle
 * @param ulNotifyId The event that occurred
 */
static void I3C_NotifyCallback(hal_i3c_handle_t *hi3c, uint32_t ulNotifyId) {
  //SWD_printf("-- I3C_NotifyCallback --: %lx\n", ulNotifyId);
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sI3CNotifyQueue, &ulNotifyId,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * @brief: Rx complete callback
 *
 * @param hi3c The I3C handle
 */
static void I3C_RxCompleteCallback(hal_i3c_handle_t *hi3c) {
  //SWD_printf("-- I3C_RxCompleteCallback [%d bytes]--\n", hi3c->data_size_byte);
  uint8_t ucEvent = EVENT_RX_COMPLETE;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sI3CIntQueue, &ucEvent,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * @brief: Tx complete callback
 *
 * @param hi3c The I3C handle
 */
static void I3C_TxCompleteCallback(hal_i3c_handle_t *hi3c) {
  //SWD_printf("-- I3C_TxCompleteCallback --\n");
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
  SWD_printf("-- Error callback codes: %01x\n", hi3c->last_error_codes);
  uint8_t ucEvent = EVENT_ERROR;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sI3CIntQueue, &ucEvent,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * @brief: I3C read command
 */
static hal_status_t I3C_ReadCommand() {
  if (I3C_State != STATE_IDLE) {
    SWD_printf("I3C_ReadCommand: [Error] state not idle.\n");
    return HAL_BUSY;
  }

  hal_status_t status;
  hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();
  status = HAL_I3C_TGT_Receive_IT(hI3C, I3C_RxBuffer, 4);
  if (status != HAL_OK) {
    SWD_printf("I3C_ReadCommand: HAL_I3C_TGT_Receive_IT: %lx\n", status);
    return status;
  }

  I3C_State = STATE_RX_COMMAND_PENDING;

  SWD_printf("Waiting for CMD...\n");
  return HAL_OK;
}

/*
 * brief: I3C read payload
 *
 * @param ulPayloadLength The Rx payload length
 */
static hal_status_t I3C_ReadPayload(uint32_t ulPayloadLength) {
  if (I3C_State != STATE_IDLE) {
    SWD_printf("I3C_ReadPayload: [Error] state is not idle.\n");
    return HAL_BUSY;
  }

  if (ulPayloadLength > I3C_Max_Rx_Buffer_Length) {
    SWD_printf("I3C_ReadPayload: ulPayloadLength (%lx) > "
        "I3C_Max_Rx_Buffer_Length (%lx)\n",
        ulPayloadLength, I3C_Max_Rx_Buffer_Length);
    return HAL_ERROR;
  }

  hal_status_t status;
  hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();
  status = HAL_I3C_TGT_Receive_IT(hI3C, I3C_RxBuffer, ulPayloadLength);
  if (status != HAL_OK) {
    SWD_printf("I3C_ReadPayload: HAL_I3C_TGT_Receive_IT: %lx\n", status);
    return status;
  }

  I3C_State = STATE_RX_PAYLOAD_PENDING;
  return HAL_OK;
}

/*
 * brief: I3C write payload
 *
 * @param ulPayloadLength The Tx payload length
 */
static hal_status_t I3C_WritePayload(uint32_t ulPayloadLength) {
  if (I3C_State != STATE_IDLE) {
    SWD_printf("I3C_WritePayload: [Error] state not idle.\n");
    return HAL_BUSY;
  }

  if (ulPayloadLength > I3C_Max_Tx_Buffer_Length) {
    SWD_printf("I3C_WritePayload: ulLength (%lx) > "
        "I3C_Max_Tx_Buffer_Length (%lx)\n",
        ulPayloadLength, I3C_Max_Tx_Buffer_Length);
    return HAL_ERROR;
  }

  // Check if we would end up reading beyond the end of the buffer.
  uint32_t ulBytesCopy = (I3C_uwLastAddress + ulPayloadLength < MEM_SIZE) ?
      ulPayloadLength : MEM_SIZE - I3C_uwLastAddress;

  hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();

  hal_status_t status;
  status = HAL_I3C_TGT_Transmit_IT(hI3C, pMem + I3C_uwLastAddress, ulBytesCopy);
  if (status != HAL_OK) {
    SWD_printf("I3C_WritePayload: HAL_I3C_TGT_Transmit_IT: %lx\n", status);
    return status;
  }

  I3C_State = STATE_TX_PAYLOAD_PENDING;

  SWD_printf("LOAD CMD [%d bytes] @%02x: ", ulBytesCopy, I3C_uwLastAddress);
  for (uint32_t i = 0; i < ulBytesCopy; i++) {
    SWD_printf("%02x ", (pMem + I3C_uwLastAddress)[i]);
  }
  SWD_printf("\n");

  return status;
}

/*
 * @brief: Reset the target
 */
static void I3C_Reset() {
  // Reset the state
  I3C_State = STATE_INIT;
  I3C_ucLastCommand = 0;
  I3C_uwLastAddress = 0;
}
