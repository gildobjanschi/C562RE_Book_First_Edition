/*******************************************************************************
 * file           : i2c1.c
 * brief          : I2C1 implementation
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/queue.h"
#include "../../Shared/Debug/swd_printf.h"
#include "i2c1.h"

// The Tx buffer
#define TX_BUFFER_SIZE 4
static uint8_t I2C1_TxBuffer[TX_BUFFER_SIZE];

// The Rx buffer
#define RX_BUFFER_SIZE 4
static uint8_t I2C1_RxBuffer[RX_BUFFER_SIZE];
static uint32_t I2C1_ulRxBytes;

// Rx state machines
typedef enum {
  RX_IDLE,
  RX_ADDRESS_SENT,
  RX_READ
} RX_STATE;

// The Rx state
static RX_STATE I2C1_RxState;

// The application state machine function pointer
static hal_i2c1_sm_cb_t I2C1_pAppStateMachine;

// The application I2C device address
static uint32_t I2C1_ulDeviceAddress;

// The interrupt notification queue
static volatile QueueHandle_t sI2C1Queue;

// Interrupt callbacks
static void I2C1_MasterTxTransferCpltCallback(hal_i2c_handle_t *hI2C);
static void I2C1_MasterRxTransferCpltCallback(hal_i2c_handle_t *hI2C);
static void I2C1_TransferErrorCallback(hal_i2c_handle_t *hI2C);

/*
 * @brief  Initialize I2C1
 *
 * @param I2cQueue The queue used to notify of interrupts
 * @param pCallback Pointer to the application state machine
 * @param ulDeviceAddress The I2C devices shifted address
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t I2C1_Init(QueueHandle_t I2cQueue, hal_i2c1_sm_cb_t pCallback,
    uint32_t ulDeviceAddress) {
  hal_status_t status;
  hal_i2c_handle_t *hI2C = mx_i2c1_i2c_gethandle();

  // Register the Tx complete I2C master callback (ACK was received)
  status = HAL_I2C_MASTER_RegisterTxCpltCallback(hI2C,
      I2C1_MasterTxTransferCpltCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_I2C_MASTER_RegisterTxCpltCallback failed.\n");
    return status;
  }

  // Register the Rx complete I2C master callback
  status = HAL_I2C_MASTER_RegisterRxCpltCallback(hI2C,
      I2C1_MasterRxTransferCpltCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_I2C_MASTER_RegisterRxCpltCallback failed.\n");
    return status;
  }

  // Register the error I2C master callback
  status = HAL_I2C_RegisterErrorCallback(hI2C, I2C1_TransferErrorCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_I2C_RegisterErrorCallback failed.\n");
    return status;
  }

  // Store the device I2C address
  I2C1_ulDeviceAddress = ulDeviceAddress;

  // Clear the Rx state
  I2C1_RxState = RX_IDLE;
  I2C1_ulRxBytes = 0;

  // Store the pointer to the application state machine
  I2C1_pAppStateMachine = pCallback;

  sI2C1Queue = I2cQueue;
  return HAL_OK;
}

/*
 * @brief  Send I2C data
 *
 * @param uwRegisterAddr The register address
 * @param pDataBuffer The pointer to the data to send
 * @param ulDataLength The number of bytes to send
 *
 * @retval HAL_OK if it succeeded
 */
hal_status_t I2C1_Send(uint16_t uwRegisterAddr, uint8_t *pDataBuffer,
    uint32_t ulDataLength) {

  if (ulDataLength + 2 > TX_BUFFER_SIZE) {
    SWD_printf("I2C1_Send: Tx buffer too small: %d, requested + 2: %d.\n",
        TX_BUFFER_SIZE, ulDataLength + 2);
    return HAL_ERROR;
  }

  // Send the 16-bit register address and the data
  I2C1_TxBuffer[0] = uwRegisterAddr >> 8;
  I2C1_TxBuffer[1] = uwRegisterAddr & 0xFF;
  for (uint32_t i = 0; i < ulDataLength; i++) {
    I2C1_TxBuffer[i + 2] = pDataBuffer[i];
  }

  hal_status_t status;
  hal_i2c_handle_t *hI2C = mx_i2c1_i2c_gethandle();
  status = HAL_I2C_MASTER_Transmit_IT(hI2C, I2C1_ulDeviceAddress, I2C1_TxBuffer,
      ulDataLength + 2);
  if (status != HAL_OK) {
    SWD_printf("I2C1_Send: HAL_I2C_MASTER_Transmit_IT failed\n");
    return status;
  }

  return HAL_OK;
}

/*
 * @brief: Master transmission end of transfer callback
 *
 * @param hI2C The handle to the I2C peripheral
 */
static void I2C1_MasterTxTransferCpltCallback(hal_i2c_handle_t *hI2C) {
  if (hI2C == mx_i2c1_i2c_gethandle()) {
    uint8_t ucEvent = EVENT_TX_COMPLETE;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sI2C1Queue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * @brief  I2C1 TxComplete
 *
 * @retval HAL_OK if the function succeeds
 */
hal_status_t I2C1_TxComplete() {
  if (I2C1_RxState == RX_ADDRESS_SENT) {
    // Read the data
    hal_i2c_handle_t *hI2C = mx_i2c1_i2c_gethandle();
    hal_status_t status;
    status = HAL_I2C_MASTER_Receive_IT(hI2C, I2C1_ulDeviceAddress,
        I2C1_RxBuffer, I2C1_ulRxBytes);
    if (status != HAL_OK) {
      SWD_printf("I2C1_TxComplete: HAL_I2C_MASTER_Receive_IT failed\n");
      return status;
    }

    I2C1_RxState = RX_READ;
  } else if (I2C1_RxState == RX_READ) {
    SWD_printf("I2C1_TxComplete: Bad Rx_State == RX_READ!.\n");
  } else {
    I2C1_pAppStateMachine(NULL, 0, NULL);
  }

  return HAL_OK;
}

/*
 * @brief  Receive I2C1 data
 *
 * @param uwRegisterAddr The register address
 * @param ulDataLength The length of data to receive
 *
 * @retval HAL_OK if it succeeded
 */
hal_status_t I2C1_Recv(uint16_t uwRegisterAddr, uint32_t ulDataLength) {
  if (ulDataLength > RX_BUFFER_SIZE) {
    SWD_printf("I2C1_Recv: Rx buffer too small: %d, requested: %d.\n",
        RX_BUFFER_SIZE, ulDataLength);
    return HAL_ERROR;
  }

  // Send the 16-bit address.
  I2C1_TxBuffer[0] = uwRegisterAddr >> 8;
  I2C1_TxBuffer[1] = uwRegisterAddr & 0xFF;

  hal_i2c_handle_t *hI2C = mx_i2c1_i2c_gethandle();
  hal_status_t status;
  status = HAL_I2C_MASTER_Transmit_IT(hI2C, I2C1_ulDeviceAddress, I2C1_TxBuffer,
      2);
  if (status != HAL_OK) {
    SWD_printf("I2C1_Recv: HAL_I2C_MASTER_Transmit_IT failed\n");
    return status;
  }

  // The read will occur after the address sending completes.
  I2C1_RxState = RX_ADDRESS_SENT;
  I2C1_ulRxBytes = ulDataLength;

  return HAL_OK;
}

/*
 * @brief: Master reception end of transfer callback
 *
 * @param hI2C The handle to the I2C peripheral
 */
static void I2C1_MasterRxTransferCpltCallback(hal_i2c_handle_t *hI2C) {
  if (hI2C == mx_i2c1_i2c_gethandle()) {
    uint8_t ucEvent = EVENT_RX_COMPLETE;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sI2C1Queue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * @brief  I2C1 RxComplete
 *
 * @param puwDistance Return value for the distance when the return value
 *      is HAL_DISTANCE_AVAIL
 *
 * @retval HAL_OK if the function succeeds
 */
hal_status_t I2C1_RxComplete(uint16_t *puwDistance) {
  // We are done reading
  I2C1_RxState = RX_IDLE;

  return I2C1_pAppStateMachine(I2C1_RxBuffer, I2C1_ulRxBytes, puwDistance);
}

/*
 * @brief: Error callback
 *
 * @param hI2C The handle to the I2C peripheral
 */
static void I2C1_TransferErrorCallback(hal_i2c_handle_t *hI2C) {
  if (hI2C == mx_i2c1_i2c_gethandle()) {
    uint8_t ucEvent = EVENT_ERROR;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sI2C1Queue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * @brief  An I2C1 error occurred
 *
 * @retval HAL_OK if the function succeeded
 */
hal_status_t I2C1_Error() {
  SWD_printf("I2C1_Error.\n");
  return HAL_OK;
}
