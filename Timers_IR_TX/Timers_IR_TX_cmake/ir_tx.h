/*******************************************************************************
 * file           : ir_tx.h
 * brief          : IR TX definitions
 ******************************************************************************/
#ifndef IR_TX_H
#define IR_TX_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

hal_status_t IrTx_Init();
hal_status_t IrTxFrame(uint8_t address, uint8_t command);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* IR_TX_H */
