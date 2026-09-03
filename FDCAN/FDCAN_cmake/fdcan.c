/*******************************************************************************
 * file           : fdcan_controller.c
 * brief          : FDCAN controller implementation
 ******************************************************************************/
#include <string.h>
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/queue.h"
#include "../../Shared/Debug/swd_printf.h"
#include "fdcan.h"

/* @user: define the controller requested Frame ID in Standard Mode (11bits) */
#define FDCAN_CONTROLLER_FRAME_ID 0x101

typedef enum {
  STATE_TX_IDLE               = 0,
  STATE_TX_CONTROLLER_PENDING = 0x1,
  STATE_TX_RESPONDER_PENDING  = 0x2,
} FDCAN_TX_STATE;

static FDCAN_TX_STATE FDCAN_Tx_State;

typedef enum {
  STATE_RX_IDLE               = 0,
  STATE_RX_CONTROLLER_WAITING = 0x1,
  STATE_RX_RESPONDER_WAITING  = 0x2,
} FDCAN_RX_STATE;

static FDCAN_RX_STATE FDCAN_Rx_State;

static hal_fdcan_rx_header_t rx_element_header;

// The maximum size of the RX FDCAN buffer in bytes.
#define MAX_RX_BUFFER_SIZE       64U
static uint8_t FDCAN_RxBuffer[MAX_RX_BUFFER_SIZE];

// The maximum size of the TX FDCAN buffer in bytes.
#define MAX_TX_BUFFER_SIZE       64U
static uint8_t FDCAN_TxBuffer[MAX_TX_BUFFER_SIZE];

static volatile QueueHandle_t sxControllerQueue;

/*
 * Functions allowing the user to configure dynamically the FDCAN callbacks
 * instead of weak functions.
 */
static void TxCompleteCallback(hal_fdcan_handle_t *hfdcan,
    uint32_t buffer_indexes);
static void RxFifo0EventCallback(hal_fdcan_handle_t *hfdcan,
    uint32_t buffer_indexes);
static void TransferErrorCallback(hal_fdcan_handle_t *hfdcan);

/*
 * @brief:  Initialize FDCAN controller
 *
 * @param xFDCANQueue The queue for notifying of interrupt events
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t FDCAN_Init(QueueHandle_t xFDCANQueue) {
  hal_status_t status;

  hal_fdcan_handle_t * hfdcan = mx_fdcan1_gethandle();
  // Register the Tx complete FDCAN callbacks
  status = HAL_FDCAN_RegisterTxBufferCompleteCallback(hfdcan,
      TxCompleteCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_FDCAN_RegisterTxBufferCompleteCallback failed.\n");
    return status;
  }

  // Register the Rx complete FDCAN callbacks
  status = HAL_FDCAN_RegisterRxFifo0Callback(hfdcan, RxFifo0EventCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_FDCAN_RegisterRxFifoEventCallback failed.\n");
    return status;
  }

  // Register the error FDCAN callbacks
  status = HAL_FDCAN_RegisterErrorCallback(hfdcan, TransferErrorCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_FDCAN_RegisterErrorCallback failed.\n");
    return status;
  }

  // Start FDCAN
  status = HAL_FDCAN_Start(hfdcan);
  if (status != HAL_OK) {
    SWD_printf("HAL_FDCAN_Start failed.\n");
    return status;
  }

  sxControllerQueue = xFDCANQueue;

  FDCAN_Tx_State = STATE_TX_IDLE;
  FDCAN_Rx_State = STATE_RX_IDLE;

  return status;
}

/*
 * @brief:  Send a data buffer. The length must be one of the accepted
 *  CAN lengths.
 *
 * @param pTxData The Tx data buffer
 * @param ulTxData The number of bytes sent
 */
static hal_status_t FDCAN_Send(uint8_t* pTxData, uint32_t ulTxData) {
  /* Prepare FDCAN transmit message header */
  hal_fdcan_tx_header_t tx_element_header = {
    .b.identifier = FDCAN_CONTROLLER_FRAME_ID,
    .b.frame_type = HAL_FDCAN_FRAME_DATA,
    .b.identifier_type = HAL_FDCAN_ID_STANDARD,
    .b.error_state_indicator = HAL_FDCAN_ERROR_STATE_IND_ACTIVE,
    .b.message_marker = 52U,
    .b.frame_format = HAL_FDCAN_HEADER_FRAME_FORMAT_FD_CAN,
    .b.bit_rate_switch = HAL_FDCAN_BIT_RATE_SWITCH_ON,
    .b.event_fifo_control = HAL_FDCAN_TX_EVENTS_FIFO_STORE,
    .b.data_length = ulTxData,
  };

  // Send the message
  hal_status_t status;
  hal_fdcan_handle_t * hfdcan = mx_fdcan1_gethandle();
  status = HAL_FDCAN_ReqTransmitMsgFromFIFOQ(hfdcan, &tx_element_header,
      pTxData);
  if (status != HAL_OK) {
    SWD_printf("HAL_FDCAN_ReqTransmitMsgFromFIFOQ failed.\n");
    return status;
  }

  return status;
}

/*
 * @brief:  Controller sends a data buffer.
 *
 * @param pTxData The Tx data buffer
 * @param ulTxData The number of bytes sent
 */
hal_status_t FDCAN_Controller_Send(uint8_t* pTxData, uint32_t ulTxData) {
  hal_status_t status = FDCAN_Send(pTxData, ulTxData);
  if (status == HAL_OK) {
    FDCAN_Tx_State |= STATE_TX_CONTROLLER_PENDING;
    FDCAN_Rx_State |= STATE_RX_RESPONDER_WAITING;

    HAL_GPIO_WritePin(HAL_GPIOC, PC0_PIN, HAL_GPIO_PIN_SET);
  }

  return status;
}

/*
 * @brief:  Controller Tx complete from the application task.
 */
void FDCAN_Controller_Tx_Complete() {
  FDCAN_Tx_State &= ~STATE_TX_CONTROLLER_PENDING;

  HAL_GPIO_WritePin(HAL_GPIOC, PC0_PIN, HAL_GPIO_PIN_RESET);
  //SWD_printf("Controller Tx complete\n");
}

/*
 * @brief:  Controller Rx complete from the application task.
 */
void FDCAN_Controller_Rx_Complete() {
  FDCAN_Rx_State &= ~STATE_RX_CONTROLLER_WAITING;

  hal_status_t status;
  // Receive data
  hal_fdcan_handle_t * hfdcan = mx_fdcan1_gethandle();
  status = HAL_FDCAN_GetReceivedMessage(hfdcan, HAL_FDCAN_RX_FIFO_0,
      &rx_element_header, FDCAN_RxBuffer);
  if (status != HAL_OK) {
    SWD_printf("HAL_FDCAN_GetReceivedMessage failed %x.\n",
        hfdcan->last_error_codes);
    return;
  }
  HAL_GPIO_WritePin(HAL_GPIOC, PC3_PIN, HAL_GPIO_PIN_RESET);

  // Display the buffer received from the responder
  uint32_t ulTxData = rx_element_header.b.data_length;
  SWD_printf("Controller Rx [%d bytes]: ", ulTxData);
  for (uint32_t i = 0; i < ulTxData; i++) {
    SWD_printf("%02xh ", FDCAN_RxBuffer[i]);
  }
  SWD_printf("\n");
}

/*
 * @brief:  Responder Tx complete from the application task.
 */
void FDCAN_Responder_Tx_Complete() {
  FDCAN_Tx_State &= ~STATE_TX_RESPONDER_PENDING;

  HAL_GPIO_WritePin(HAL_GPIOC, PC2_PIN, HAL_GPIO_PIN_RESET);
  //SWD_printf("Responder Tx complete\n");
}

/*
 * @brief:  Responder Rx complete from the application task.
 */
void FDCAN_Responder_Rx_Complete(){
  FDCAN_Rx_State &= ~STATE_RX_RESPONDER_WAITING;

  hal_status_t status;
  // Receive data
  hal_fdcan_handle_t * hfdcan = mx_fdcan1_gethandle();
  status = HAL_FDCAN_GetReceivedMessage(hfdcan, HAL_FDCAN_RX_FIFO_0,
      &rx_element_header, FDCAN_RxBuffer);
  if (status != HAL_OK) {
    SWD_printf("HAL_FDCAN_GetReceivedMessage failed %x.\n",
        hfdcan->last_error_codes);
    return;
  }
  HAL_GPIO_WritePin(HAL_GPIOC, PC3_PIN, HAL_GPIO_PIN_RESET);

  // Display the buffer received and build the response buffer
  uint32_t ulTxData = rx_element_header.b.data_length;
  SWD_printf("Responder Rx [%d bytes]: ", ulTxData);
  for (uint32_t i = 0; i < ulTxData; i++) {
    SWD_printf("%02xh ", FDCAN_RxBuffer[i]);
    FDCAN_TxBuffer[i] = FDCAN_RxBuffer[i] + 1;
  }
  SWD_printf("\n");

  // Send the response
  status = FDCAN_Send(FDCAN_TxBuffer, ulTxData);
  if (status == HAL_OK) {
    FDCAN_Tx_State |= STATE_TX_RESPONDER_PENDING;
    FDCAN_Rx_State |= STATE_RX_CONTROLLER_WAITING;
    HAL_GPIO_WritePin(HAL_GPIOC, PC2_PIN, HAL_GPIO_PIN_SET);
  }
}

/*
 * @brief:  Tx complete callback
 *
 * @param hfdcan The FDCAN handle
 */
static void TxCompleteCallback(hal_fdcan_handle_t *hfdcan,
    uint32_t buffer_indexes) {
  uint8_t ucEvent = (FDCAN_Tx_State & STATE_TX_CONTROLLER_PENDING) ==
          STATE_TX_CONTROLLER_PENDING?
      EVENT_CONTROLLER_TX_COMPLETE : EVENT_RESPONDER_TX_COMPLETE;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sxControllerQueue, &ucEvent,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * @brief:  Rx FIFO 0 complete callback
 *
 * @param hfdcan The FDCAN handle
 */
static void RxFifo0EventCallback(hal_fdcan_handle_t *hfdcan,
    uint32_t buffer_indexes) {
  HAL_GPIO_WritePin(HAL_GPIOC, PC3_PIN, HAL_GPIO_PIN_SET);

  uint8_t ucEvent = (FDCAN_Rx_State & STATE_RX_RESPONDER_WAITING) ==
      STATE_RX_RESPONDER_WAITING?
      EVENT_RESPONDER_RX_0_COMPLETE : EVENT_CONTROLLER_RX_0_COMPLETE;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sxControllerQueue, &ucEvent,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * @brief:  Error callback
 *
 * @param hfdcan The FDCAN handle
 */
static void TransferErrorCallback(hal_fdcan_handle_t *hfdcan) {
  uint8_t ucEvent = EVENT_ERROR;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sxControllerQueue, &ucEvent,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}
