/*******************************************************************************
 * file           : spi2_dma.c
 * brief          : SPI 2 DMA implementation
 ******************************************************************************/
#include <string.h>
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/queue.h"
#include "../../Shared/Debug/swd_printf.h"
#include "spi2_dma.h"

// Interrupt callbacks
static void SPI2_TxRxTransferCpltCallback(hal_spi_handle_t *hSPI);
static void SPI2_ErrorCallback(hal_spi_handle_t *hSPI);

// FRAM SPI commands
#define SPI_COMMAND_WREN  0x06
#define SPI_COMMAND_WRITE 0x02
#define SPI_COMMAND_RDSR  0x05
#define SPI_COMMAND_WRSR  0x01
#define SPI_COMMAND_WRDI  0x04
#define SPI_COMMAND_READ  0x03
#define SPI_COMMAND_RDID  0x9f

// The buffer size
#define BUFFER_SIZE 128
// The Rx DMA buffer
static uint8_t SPI2_DMA_RxBuffer[BUFFER_SIZE];

// The Tx DMA buffer (allow space for a one byte command and 2 bytes address)
static uint8_t SPI2_DMA_TxBuffer[BUFFER_SIZE + 3];

typedef enum {
  SM_IDLE,
  SM_READ,
  SM_WREN,
  SM_WRITE,
  SM_WRDI
} SM_STATE;
static volatile SM_STATE SPI2_State;

// The SPI interrupts queue
static volatile QueueHandle_t sSPI2Queue;

// The application data buffer and data length
static uint16_t uwAppAddress;
static uint8_t *pAppDataBuffer;
static uint32_t ulAppDataLength;

/*
 * @brief  Initialize SPI2 DMA
 *
 * @param SPIQueue The queue for notifying the task of interrupt events
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t SPI2_Init(QueueHandle_t SPIQueue) {
  hal_status_t status;
  hal_spi_handle_t *hSPI = mx_spi2_gethandle();

  // Register the Tx/Rx complete callback
  status = HAL_SPI_RegisterTxRxCpltCallback(hSPI,
      SPI2_TxRxTransferCpltCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_SPI_RegisterTxRxCpltCallback failed.\n");
    return status;
  }

  // Register the error SPI callback
  status = HAL_SPI_RegisterErrorCallback(hSPI, SPI2_ErrorCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_SPI_RegisterErrorCallback failed.\n");
    return status;
  }

  SPI2_State = SM_IDLE;

  sSPI2Queue = SPIQueue;

  return HAL_OK;
}

/*
 * @brief  The write state machine
 *
 * @retval HAL_OK if it succeeded
 */
static hal_status_t SPI2_WriteStateMachine() {
  uint32_t ulBytesToTransact;
  switch (SPI2_State) {
  case SM_IDLE: {
    // One byte command
    SPI2_DMA_TxBuffer[0] = SPI_COMMAND_WREN;
    ulBytesToTransact = 1;

    SWD_printf("SM_IDLE -> SM_WREN.\n");
    SPI2_State = SM_WREN;
    break;
  }

  case SM_READ: {
    return HAL_ERROR;
  }

  case SM_WREN: {
    // One byte command
    SPI2_DMA_TxBuffer[0] = SPI_COMMAND_WRITE;
    // Two bytes address
    SPI2_DMA_TxBuffer[1] = uwAppAddress >> 8;
    SPI2_DMA_TxBuffer[2] = uwAppAddress & 0xFF;
    // Data
    memcpy(SPI2_DMA_TxBuffer + 3, pAppDataBuffer, ulAppDataLength);
    ulBytesToTransact = sizeof(uint8_t) + sizeof(uint16_t) + ulAppDataLength;

    SWD_printf("SM_WREN -> SM_WRITE.\n");
    SPI2_State = SM_WRITE;
    break;
  }

  case SM_WRITE: {
    // One byte command
    SPI2_DMA_TxBuffer[0] = SPI_COMMAND_WRDI;
    ulBytesToTransact = 1;

    SWD_printf("SM_WRITE -> SM_WRDI.\n");
    SPI2_State = SM_WRDI;
    break;
  }

  case SM_WRDI: {
    SWD_printf("SM_WRDI -> SM_IDLE.\n");
    SPI2_State = SM_IDLE;
    ulBytesToTransact = 0;
    break;
  }
  }

  if (ulBytesToTransact > 0) {
    hal_spi_handle_t *hSPI = mx_spi2_gethandle();
    hal_status_t status = HAL_SPI_TransmitReceive_DMA(hSPI, SPI2_DMA_TxBuffer,
        SPI2_DMA_RxBuffer, ulBytesToTransact);
    if (status != HAL_OK) {
      SWD_printf("HAL_SPI_TransmitReceive_DMA failed.\n");
    }

    return status;
  } else {
    return HAL_WRITE_COMPLETE;
  }
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
hal_status_t SPI2_Write(uint16_t uwAddress, uint8_t *pDataBuffer,
    uint32_t ulDataLength) {
  if (ulDataLength > BUFFER_SIZE) {
    return HAL_ERROR;
  }

  if (SPI2_State != SM_IDLE) {
    SWD_printf("SPI2_Write is invalid in state: %ld.\n", SPI2_State);
    return HAL_ERROR;
  }

  // Store the app data since we need in the state machine
  uwAppAddress = uwAddress;
  pAppDataBuffer = pDataBuffer;
  ulAppDataLength = ulDataLength;

  return SPI2_WriteStateMachine();
}

/*
 * @brief  The Tx completed
 *
 * @retval HAL_OK if it succeeded
 */
hal_status_t SPI2_TxComplete() {
  return SPI2_WriteStateMachine();
}

/*
 * @brief  Read SPI data sequentially starting from the specified address
 *
 * @param uwAddress The address
 * @param ulDataLength The length of data to read
 *
 * @retval HAL_OK if it succeeded
 */
hal_status_t SPI2_Read(uint16_t uwAddress, uint32_t ulDataLength) {
  if (SPI2_State != SM_IDLE) {
    SWD_printf("SPI2_Read is invalid in state: %ld.\n", SPI2_State);
    return HAL_ERROR;
  }

  // One byte command
  SPI2_DMA_TxBuffer[0] = SPI_COMMAND_READ;
  // Two bytes address
  SPI2_DMA_TxBuffer[1] = uwAddress >> 8;
  SPI2_DMA_TxBuffer[2] = uwAddress & 0xFF;

  SPI2_State = SM_READ;
  ulAppDataLength = ulDataLength;

  hal_spi_handle_t *hSPI = mx_spi2_gethandle();
  hal_status_t status;
  status = HAL_SPI_TransmitReceive_DMA(hSPI, SPI2_DMA_TxBuffer,
      SPI2_DMA_RxBuffer, sizeof(uint8_t) + sizeof(uint16_t) + ulDataLength);
  if (status != HAL_OK) {
    SWD_printf("HAL_SPI_TransmitReceive_DMA failed.\n");
    return status;
  }

  return HAL_OK;
}

/*
 * @brief  The Rx completed
 *
 * @param ppRxBuffer Returns the pointer to the read buffer
 * @param pulBytesRead Returns the number of bytes read
 *
 * @retval HAL_OK if it succeeded
 */
hal_status_t SPI2_RxComplete(uint8_t **ppRxBuffer, uint32_t *pulBytesRead) {
  if (SPI2_State == SM_READ) {
    SPI2_State = SM_IDLE;
    *ppRxBuffer = SPI2_DMA_RxBuffer + 3;
    *pulBytesRead = ulAppDataLength;
    return HAL_OK;
  } else {
    SWD_printf("SPI2_RxComplete invalid state: %ld.\n", SPI2_State);
    return HAL_ERROR;
  }
}

/*
 * @brief  Get the buffer size
 *
 * @retval The size of the buffer
 */
uint32_t SPI2_GetBufferSize() {
  return BUFFER_SIZE;
}

/*
 * brief: Master transmission end of transfer callback
 *
 * @param hSPI The handle to the SPI peripheral
 */
static void SPI2_TxRxTransferCpltCallback(hal_spi_handle_t *hSPI) {
  if (hSPI == mx_spi2_gethandle()) {
    uint8_t ucEvent = SPI2_State == SM_READ ? EVENT_RX_COMPLETE :
        EVENT_TX_COMPLETE;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sSPI2Queue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * brief: Error callback
 *
 * @param hSPI The handle to the hSPI peripheral
 */
static void SPI2_ErrorCallback(hal_spi_handle_t *hSPI) {
  if (hSPI == mx_spi2_gethandle()) {
    uint8_t ucEvent = EVENT_ERROR;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sSPI2Queue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * @brief  An spi 2 error occurred
 *
 * @retval HAL_OK if the function succeeded
 */
hal_status_t SPI2_Error() {
  SWD_printf("SPI2_Error.\n");

  return HAL_OK;
}

