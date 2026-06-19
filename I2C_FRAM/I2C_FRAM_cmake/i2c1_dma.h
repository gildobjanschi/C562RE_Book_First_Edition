/*******************************************************************************
 * file           : i2c1_dma.h
 * brief          : I2C 1 DMA definitions
 ******************************************************************************/
#ifndef I2C1_DMA_H
#define I2C1_DMA_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define HAL_ADDR_SENT 1

// Interrupt events
#define EVENT_TX_COMPLETE  1
#define EVENT_RX_COMPLETE  2
#define EVENT_ERROR        3

hal_status_t I2C1_Init(QueueHandle_t I2cQueue, uint32_t ulDeviceAddress);
uint32_t I2C1_GetWriteBufferSize();
uint32_t I2C1_GetReadBufferSize();
hal_status_t I2C1_Write(uint16_t uwAddress, uint8_t* pDataBuffer,
    uint32_t ulDataLength);
hal_status_t I2C1_TxComplete();
hal_status_t I2C1_Read(uint16_t uwAddress, uint32_t ulDataLength);
hal_status_t I2C1_RxComplete(uint8_t **pRxBuffer, uint32_t* pulRxBytes);
hal_status_t I2C1_Error();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* I2C1_DMA_H */
