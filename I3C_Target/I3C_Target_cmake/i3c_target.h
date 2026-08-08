/*******************************************************************************
 * file           : i3c_target.h
 * brief          : I3C definitions
 ******************************************************************************/
#ifndef I3C_TARGET_H
#define I3C_TARGET_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Interrupt events
#define EVENT_RX_COMPLETE  1
#define EVENT_TX_COMPLETE  2
#define EVENT_ERROR        3

hal_status_t I3C_Init(QueueHandle_t I3CNotifyQueue, QueueHandle_t I3CIntQueue);
hal_status_t I3C_Notify(uint32_t ulNotifyId);
hal_status_t I3C_RxComplete();
hal_status_t I3C_TxComplete();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* I3C_TARGET_H */
