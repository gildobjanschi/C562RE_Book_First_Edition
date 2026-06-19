/*******************************************************************************
 * file           : usart1_dma.h
 * brief          : USART1 DMA definitions
 ******************************************************************************/
#ifndef USART1_DMA_H
#define USART1_DMA_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Interrupt events
#define EVENT_USART1_RX_COMPLETE  0
#define EVENT_USART1_TX_COMPLETE  1
#define EVENT_USART1_ERROR        2

// The Rx DMA buffer size
#define RX_DMA_BUFFER_SIZE 64
// The Tx DMA buffer size
#define TX_DMA_BUFFER_SIZE 64

hal_status_t USART1_Init(StreamBufferHandle_t RxUARTStreamBuffer,
    StreamBufferHandle_t TxUARTStreamBuffer, QueueHandle_t IntUARTQueue);
hal_status_t USART1_Send(uint8_t *pBuffer, uint32_t ulBufferLength);
hal_status_t USART1_Rx();
hal_status_t USART1_Tx();
hal_status_t USART1_Error();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* USART1_DMA_H */
