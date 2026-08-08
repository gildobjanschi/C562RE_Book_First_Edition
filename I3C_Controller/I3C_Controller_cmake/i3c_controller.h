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

hal_status_t I3C_Init(QueueHandle_t I3CIntQueue);
hal_status_t I3C_StartDAA();
hal_status_t I3C_DAAComplete();
hal_status_t I3C_TransferComplete();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* I3C_CONTROLLER_H */
