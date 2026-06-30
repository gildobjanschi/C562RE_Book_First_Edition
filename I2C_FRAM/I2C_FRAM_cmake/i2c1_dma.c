/*******************************************************************************
 * file           : i2c1_dma.c
 * brief          : I2C 1 DMA implementation
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/queue.h"
#include "../../Shared/Debug/swd_printf.h"
#include "i2c1_dma.h"

// The application I2C device address
static uint32_t I2C1_ulDeviceAddress;

// Interrupt callbacks
static void I2C1_MasterTxTransferCpltCallback(hal_i2c_handle_t *hI2C);
static void I2C1_MasterRxTransferCpltCallback(hal_i2c_handle_t *hI2C);
static void I2C1_TransferErrorCallback(hal_i2c_handle_t *hI2C);

// Rx state machines
typedef enum {
  RX_IDLE,
  RX_ADDRESS_SENT,
  RX_READ
} RX_STATE;

// The Rx state
static RX_STATE I2C1_RxState;

// Tx state machines
typedef enum {
  TX_IDLE,
  TX_PENDING
} TX_STATE;

// The Tx state
static TX_STATE I2C1_TxState;

// The Rx buffer
#define RX_DMA_BUFFER_SIZE 128
static uint8_t I2C1_RxBuffer[RX_DMA_BUFFER_SIZE];
static uint32_t I2C1_ulRxBytes;

// The APP_TX_BUFFER_SIZE is the maximum size that can be sent to the FRAM
#define APP_TX_BUFFER_SIZE 128
// The DMA buffer size must accommodate 2 bytes of address
#define TX_DMA_BUFFER_SIZE (APP_TX_BUFFER_SIZE + 2)

static uint8_t I2C1_TxBuffer[TX_DMA_BUFFER_SIZE];

static volatile QueueHandle_t sI2C1Queue;

/*
 * @brief  Initialize I2C1 DMA
 *
 * @param I2cQueue The queue for notifying the task of interrupt events
 * @param ulDeviceAddress The I2C devices shifted address
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t I2C1_Init(QueueHandle_t I2cQueue, uint32_t ulDeviceAddress) {
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

  // Clear the Rx & Tx states
  I2C1_RxState = RX_IDLE;
  I2C1_TxState = TX_IDLE;
  I2C1_ulRxBytes = 0;

  sI2C1Queue = I2cQueue;

  return HAL_OK;
}

/*
 * @brief  Get the write buffer size
 *
 * @retval The size of the Tx buffer
 */
uint32_t I2C1_GetWriteBufferSize() {
  return APP_TX_BUFFER_SIZE;
}

/*
 * @brief  Get the read buffer size
 *
 * @retval The size of the Rx buffer
 */
uint32_t I2C1_GetReadBufferSize() {
  return RX_DMA_BUFFER_SIZE;
}

/*
 * @brief  Write data in a sequential mode starting at the specified address
 *
 * @param uwAddress The address where to start writing the data
 * @param pDataBuffer The pointer to the data to send
 * @param ulDataLength The number of bytes to send
 *
 * @retval HAL_OK if it succeeded
 */
hal_status_t I2C1_Write(uint16_t uwAddress, uint8_t *pDataBuffer,
    uint32_t ulDataLength) {
  if (I2C1_TxState != TX_IDLE || I2C1_RxState != RX_IDLE) {
    SWD_printf("I2C1_Write: %d %d.\n", I2C1_TxState, I2C1_RxState);
    return HAL_BUSY;
  }

  if (ulDataLength + 2 > TX_DMA_BUFFER_SIZE) {
    SWD_printf("I2C1_Write: Tx buffer too small: %d, requested + 2: %d.\n",
        TX_DMA_BUFFER_SIZE, ulDataLength + 2);
    return HAL_ERROR;
  }

  // Send the 16-bit register address and the data
  I2C1_TxBuffer[0] = uwAddress >> 8;
  I2C1_TxBuffer[1] = uwAddress & 0xFF;
  for (uint32_t i = 0; i < ulDataLength; i++) {
    I2C1_TxBuffer[i + 2] = pDataBuffer[i];
  }

  hal_status_t status;
  hal_i2c_handle_t *hI2C = mx_i2c1_i2c_gethandle();
  status = HAL_I2C_MASTER_Transmit_DMA(hI2C, I2C1_ulDeviceAddress,
      I2C1_TxBuffer, ulDataLength + 2);
  if (status != HAL_OK) {
    SWD_printf("I2C1_Write: HAL_I2C_MASTER_Transmit_DMA failed: %lx.\n",
        status);
    return status;
  }

  I2C1_TxState = TX_PENDING;
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
 * @retval HAL_OK if Tx has completed HAL_ADDR_SENT if the address sent
 *  completed.
 */
hal_status_t I2C1_TxComplete() {
  hal_status_t status;
  if (I2C1_RxState == RX_ADDRESS_SENT) {
    // Read the data
    hal_i2c_handle_t *hI2C = mx_i2c1_i2c_gethandle();
    status = HAL_I2C_MASTER_Receive_DMA(hI2C, I2C1_ulDeviceAddress | 1,
        I2C1_RxBuffer, I2C1_ulRxBytes);
    if (status != HAL_OK) {
      SWD_printf("I2C1_TxComplete: HAL_I2C_MASTER_Receive_DMA: %lx\n",
          status);
      return status;
    }

    I2C1_RxState = RX_READ;
    return HAL_ADDR_SENT;
  } else if (I2C1_RxState == RX_READ) {
    SWD_printf("I2C1_TxComplete: Rx_State == RX_READ!.\n");
    return HAL_ERROR;
  } else {
    I2C1_TxState = TX_IDLE;
    return HAL_OK;
  }
}

/*
 * @brief  Read I2C data sequentially starting from the specified address
 *
 * @param uwAddress The address
 * @param ulDataLength The length of data to read
 *
 * @retval HAL_OK if it succeeded
 */
hal_status_t I2C1_Read(uint16_t uwAddress, uint32_t ulDataLength) {
  if (I2C1_TxState != TX_IDLE || I2C1_RxState != RX_IDLE) {
    SWD_printf("I2C1_Read: %d %d.\n", I2C1_TxState, I2C1_RxState);
    return HAL_BUSY;
  }

  if (ulDataLength > RX_DMA_BUFFER_SIZE) {
    SWD_printf("I2C1_Read: Rx buffer too small: %d, requested: %d.\n",
        RX_DMA_BUFFER_SIZE, ulDataLength);
    return HAL_ERROR;
  }

  // Send the 16-bit address.
  I2C1_TxBuffer[0] = uwAddress >> 8;
  I2C1_TxBuffer[1] = uwAddress & 0xFF;

  hal_status_t status;
  hal_i2c_handle_t *hI2C = mx_i2c1_i2c_gethandle();
  status = HAL_I2C_MASTER_Transmit_DMA(hI2C, I2C1_ulDeviceAddress,
      I2C1_TxBuffer, 2);
  if (status != HAL_OK) {
    SWD_printf("I2C1_Read: HAL_I2C_MASTER_Transmit_DMA: %lx\n", status);
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
 * @param pRxBuffer Returns the read pointer
 * @param pulRxBytes Returns the number of read bytes
 *
 * @retval HAL_OK if the function succeeds
 */
hal_status_t I2C1_RxComplete(uint8_t **pRxBuffer, uint32_t* pulRxBytes) {
  // We are done reading
  I2C1_RxState = RX_IDLE;

  *pRxBuffer = I2C1_RxBuffer;
  *pulRxBytes = I2C1_ulRxBytes;
  return HAL_OK;
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

