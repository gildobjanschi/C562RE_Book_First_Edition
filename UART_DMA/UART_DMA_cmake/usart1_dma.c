/*******************************************************************************
 * file           : usart1_dma.c
 * brief          : USART1 implementation
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/queue.h"
#include "middleware/freertos/include/stream_buffer.h"
#include "../../Shared/Debug/swd_printf.h"
#include "usart1_dma.h"

// Interrupt callbacks
static void USART1_TxCpltCallback(hal_uart_handle_t *hUART);
static void USART1_RxCpltCallback(hal_uart_handle_t *hUART,
    uint32_t ulBytesReceived, hal_uart_rx_event_types_t rx_event);
static void USART1_ErrorCallback(hal_uart_handle_t *hUART);

// The Rx and Tx DMA buffers
static uint8_t USART1_RxDMABuffer[RX_DMA_BUFFER_SIZE];
static uint8_t USART1_TxDMABuffer[TX_DMA_BUFFER_SIZE];

typedef enum {
  RX_IDLE,
  RX_BUSY
} RX_STATE;

static volatile RX_STATE USART1_RxState;

typedef enum {
  TX_IDLE,
  TX_BUSY
} TX_STATE;

static volatile TX_STATE USART1_TxState;

// The queue used for interrupt events sent to the task
static volatile QueueHandle_t sInt_USART1_Queue;
static volatile StreamBufferHandle_t sRx_USART1_StreamBuffer;
static StreamBufferHandle_t sTx_USART1_StreamBuffer;

/*
 * @brief  Initialize the UART
 *
 * @param RxUARTStreamBuffer The Rx stream buffer
 * @param TxUARTStreamBuffer The Tx stream buffer
 * @param IntUARTQueue UART queue for reporting interrupts
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t USART1_Init(StreamBufferHandle_t RxUARTStreamBuffer,
    StreamBufferHandle_t TxUARTStreamBuffer, QueueHandle_t IntUARTQueue) {
  hal_status_t status;
  hal_uart_handle_t *hUART = mx_usart1_uart_gethandle();

  // Register the Tx complete callback
  status = HAL_UART_RegisterTxCpltCallback(hUART, USART1_TxCpltCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_UART_RegisterTxCpltCallback failed.\n");
    return status;
  }

  // Register the Rx complete callback
  status = HAL_UART_RegisterRxCpltCallback(hUART, USART1_RxCpltCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_UART_RegisterRxCpltCallback failed.\n");
    return status;
  }

  // Register the error callback
  status = HAL_UART_RegisterErrorCallback(hUART, USART1_ErrorCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_UART_RegisterErrorCallback failed.\n");
    return status;
  }

  // Store the pointer to the stream buffers
  sRx_USART1_StreamBuffer = RxUARTStreamBuffer;
  sTx_USART1_StreamBuffer = TxUARTStreamBuffer;
  // Store the interrupt queue pointer
  sInt_USART1_Queue = IntUARTQueue;

  USART1_TxState = TX_IDLE;
  USART1_RxState = RX_IDLE;

  return HAL_OK;
}

/*
 * @brief  Initiate a non-blocking UART receive call if there is space
 *    available in the Rx stream buffer.
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t USART1_Rx() {
  if (USART1_RxState != RX_IDLE) {
    SWD_printf("USART1_Rx: Rx is not idle.\n");
    return HAL_ERROR;
  }

  hal_status_t status;
  hal_uart_handle_t *hUART = mx_usart1_uart_gethandle();

  uint32_t ulFreeSpace = xStreamBufferSpacesAvailable(sRx_USART1_StreamBuffer);
  if (ulFreeSpace > 0) {
    // The number of bytes requested is maximum half the size of the Rx
    // DMA buffer.
    // This code guarantees that we can copy the received data into the stream
    // buffer in the interrupt callback.
    uint32_t ulRequestBytes = ulFreeSpace > RX_DMA_BUFFER_SIZE/2 ?
        RX_DMA_BUFFER_SIZE/2 : ulFreeSpace;
    status = HAL_UART_ReceiveToIdle_DMA(hUART, &USART1_RxDMABuffer,
        ulRequestBytes);

    if (status != HAL_OK) {
      SWD_printf("HAL_UART_ReceiveToIdle_DMA failed: %lx [%lx].\n", status,
          hUART->rx_state);
      return status;
    }

    USART1_RxState = RX_BUSY;
  } else {
    SWD_printf("USART1_Rx: Rx stream buffer full.\n");
  }

  return HAL_OK;
}

/*
 * @brief Reception end of transfer completed callback
 *
 * @param hUART The USART handle
 * @param ulBytesReceived The number of bytes received
 * @param rxEvent the receive event
 */
static void USART1_RxCpltCallback(hal_uart_handle_t *hUART,
    uint32_t ulBytesReceived, hal_uart_rx_event_types_t rxEvent) {
  if (hUART == mx_usart1_uart_gethandle()) {
    USART1_RxState = RX_IDLE;
    //__asm__ volatile ("sev": : :"memory");
    // Write the received data to the Rx stream buffer
    xStreamBufferSendFromISR(sRx_USART1_StreamBuffer, USART1_RxDMABuffer,
        ulBytesReceived, NULL);
    //__asm__ volatile ("sev": : :"memory");

    // Notify the task of this interrupt.
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t ucEvent = EVENT_USART1_RX_COMPLETE;
    if (xQueueSendFromISR(sInt_USART1_Queue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * @brief  Send UART data by writing it to the Tx stream buffer. The function
 *      accepts the entire buffer or 0 bytes if the Tx stream buffer cannot
 *      fit the contents of the send buffer.
 *
 * @param pBuffer Pointer to the data to send
 * @param ulBufferLength The length of the buffer
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t USART1_Send(uint8_t *pBuffer, uint32_t ulBufferLength) {
  uint32_t ulFreeSpace = xStreamBufferSpacesAvailable(sTx_USART1_StreamBuffer);
  if (ulBufferLength > ulFreeSpace) {
    // The Tx stream buffer cannot fit the send buffer content
    return HAL_BUSY;
  } else {
    xStreamBufferSend(sTx_USART1_StreamBuffer, pBuffer, ulBufferLength, 0);

    if (USART1_TxState == TX_IDLE) {
      return USART1_Tx();
    } else {
      return HAL_OK;
    }
  }
}

/*
 * @brief  Send UART data from the Tx stream buffer
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t USART1_Tx() {
  if (USART1_TxState != TX_IDLE) {
    SWD_printf("USART1_Tx: Tx is not idle.\n");
    return HAL_ERROR;
  }

  size_t numBytes = xStreamBufferReceive(sTx_USART1_StreamBuffer,
      USART1_TxDMABuffer, TX_DMA_BUFFER_SIZE, 0);

  if (numBytes > 0) {
    hal_status_t status;
    hal_uart_handle_t *hUART = mx_usart1_uart_gethandle();

    status = HAL_UART_Transmit_DMA(hUART, USART1_TxDMABuffer, numBytes);
    if (status != HAL_OK) {
      SWD_printf("HAL_UART_Transmit_DMA failed.\n");
      return status;
    }

    USART1_TxState = TX_BUSY;
  }

  return HAL_OK;
}

/*
 * @brief Transmission end of transfer completed callback
 *
 * @param hUART The USART handle
 */
static void USART1_TxCpltCallback(hal_uart_handle_t *hUART) {
  if (hUART == mx_usart1_uart_gethandle()) {
    //__asm__ volatile ("sev": : :"memory");
    USART1_TxState = TX_IDLE;

    uint8_t ucEvent = EVENT_USART1_TX_COMPLETE;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sInt_USART1_Queue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * @brief  UART transfer error callback
 *
 * @param hUART The USART handle
 */
static void USART1_ErrorCallback(hal_uart_handle_t *hUART) {
  if (hUART == mx_usart1_uart_gethandle()) {
    uint8_t ucEvent = EVENT_USART1_ERROR;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sInt_USART1_Queue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * @brief Error handling
 *
 * @retval HAL_OK if it succeeded
 */
hal_status_t USART1_Error() {
  SWD_printf("USART1_Error!\n");

  return HAL_OK;
}
