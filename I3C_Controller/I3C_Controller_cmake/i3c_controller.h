/*******************************************************************************
 * file           : i3c_controller.h
 * brief          : I3C controller definitions
 ******************************************************************************/
#ifndef I3C_CONTROLLER_H
#define I3C_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Interrupt events
#define EVENT_DAA_COMPLETE        1
#define EVENT_TRANSFER_COMPLETE   2
#define EVENT_ERROR               3

/* Size of the Rx Buffer in bytes. */
#define RX_BUFFER_SIZE  32U
/* Size of the Tx Buffer in bytes. */
#define TX_BUFFER_SIZE  32U

hal_status_t I3C_Init(QueueHandle_t I3CIntQueue);
hal_status_t I3C_IsDAACompleted();
hal_status_t I3C_StartDAA();
hal_status_t I3C_DAAComplete();
hal_status_t I3C_TransferComplete(uint8_t** ppRxBuffer,
    uint32_t* pulRxByteCount);
hal_status_t I3C_StoreCmd(uint16_t uwAddress, uint8_t* pBuf, uint32_t ulLength);
hal_status_t I3C_LoadCmd(uint16_t uwAddress, uint32_t ulLength);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* I3C_CONTROLLER_H */
