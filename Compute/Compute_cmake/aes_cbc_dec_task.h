/*******************************************************************************
 * file           : aes_cbc_dec_task.h
 * brief          : AES CBC decrypt task definitions.
 ******************************************************************************/
#ifndef AES_CBC_DEC_TASK_H
#define AES_CBC_DEC_TASK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
  StreamBufferHandle_t xAESStreamBuffer;
  SemaphoreHandle_t xPrintMutex;
} AES_CBC_DEC_PARAMETERS;

hal_status_t AES_CBC_Dec_Init(AES_CBC_DEC_PARAMETERS *params);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* AES_CBC_DEC_TASK_H */
