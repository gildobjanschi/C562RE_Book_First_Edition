/*******************************************************************************
 * file           : fdcan.h
 * brief          : FDCAN definitions
 ******************************************************************************/
#ifndef FDCAN_H
#define FDCAN_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Interrupt events
#define EVENT_CONTROLLER_TX_COMPLETE   1
#define EVENT_CONTROLLER_RX_1_COMPLETE 2
#define EVENT_RESPONDER_TX_COMPLETE    3
#define EVENT_RESPONDER_RX_0_COMPLETE  4
#define EVENT_ERROR                    5


hal_status_t FDCAN_Init(QueueHandle_t xFDCANQueue);
hal_status_t FDCAN_Controller_Send(uint8_t* pTxData, uint32_t ulTxData);
void FDCAN_Controller_Rx_Complete();
void FDCAN_Responder_Rx_Complete();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FDCAN_H */
