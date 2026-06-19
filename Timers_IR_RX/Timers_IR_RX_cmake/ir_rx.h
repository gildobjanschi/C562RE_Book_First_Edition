/*******************************************************************************
 * file           : ir_rx.h
 * brief          : Definitions for IR receiver
 ******************************************************************************/
#ifndef IR_RX_H
#define IR_RX_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

hal_status_t IrRx_Init(QueueHandle_t rxQueue);
uint8_t IrRx_Decode(uint8_t ucEvent, uint8_t *pucAddress, uint8_t *pucCommand);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* IR_RX_H */
