/*******************************************************************************
 * file           : spi2_dma.h
 * brief          : SPI 2 DMA definitions
 ******************************************************************************/
#ifndef SPI2_DMA_H
#define SPI2_DMA_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Interrupt events
#define EVENT_TX_COMPLETE  1
#define EVENT_RX_COMPLETE  2
#define EVENT_ERROR        3

// Return code for SPI2_TxComplete indicating that Write state machine ended.
#define HAL_WRITE_COMPLETE 100

hal_status_t SPI2_Init(QueueHandle_t SPIQueue);
hal_status_t SPI2_Write(uint16_t uwAddress, uint8_t* pDataBuffer,
    uint32_t ulDataLength);
hal_status_t SPI2_Read(uint16_t uwAddress, uint32_t ulDataLength);
hal_status_t SPI2_RxComplete(uint8_t **ppRxBuffer, uint32_t *pulBytesRead);
hal_status_t SPI2_TxComplete();
uint32_t SPI2_GetBufferSize();
hal_status_t SPI2_Error();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SPI2_DMA_H */
