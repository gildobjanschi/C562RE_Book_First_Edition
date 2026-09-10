/*******************************************************************************
 * file           : aes_cbc_enc_task.h
 * brief          : AES CBC encrypt task definitions.
 ******************************************************************************/
#ifndef AES_CBC_ENC_TASK_H
#define AES_CBC_ENC_TASK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define AES_CBC_ENC_EV_GROUP_BIT (1 << 4)

typedef struct {
  EventGroupHandle_t xTasksEventGroup;
  SemaphoreHandle_t xPrintMutex;
} AES_CBC_ENC_PARAMETERS;

hal_status_t AES_CBC_Enc_Init(AES_CBC_ENC_PARAMETERS *params);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* AES_CBC_ENC_TASK_H */
