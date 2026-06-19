/*******************************************************************************
 * file           : i2c1.h
 * brief          : I2C 1 definitions
 ******************************************************************************/
#ifndef I2C1_H
#define I2C1_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Interrupt events
#define EVENT_TX_COMPLETE  1
#define EVENT_RX_COMPLETE  2
#define EVENT_ERROR        3

// State machine return value when the distance is available
#define HAL_DISTANCE_AVAIL 100

// The application state machine callback
typedef  hal_status_t (*hal_i2c1_sm_cb_t)(uint8_t* pRxBuffer,
    uint32_t ulRxBytes, uint16_t *puwDistance);

hal_status_t I2C1_Init(QueueHandle_t I2cQueue, hal_i2c1_sm_cb_t pCallback,
    uint32_t ulDeviceAddress);
hal_status_t I2C1_Send(uint16_t uwRegisterAddr, uint8_t* pDataBuffer,
    uint32_t ulDataLength);
hal_status_t I2C1_Recv(uint16_t uwRegisterAddr, uint32_t ulDataLength);
hal_status_t I2C1_TxComplete();
hal_status_t I2C1_RxComplete(uint16_t *puwDistance);
hal_status_t I2C1_Error();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* I2C1_H */
