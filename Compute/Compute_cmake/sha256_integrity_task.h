/*******************************************************************************
 * file           : sha256_integrity_task.h
 * brief          : SHA256 integrity task definitions.
 ******************************************************************************/
#ifndef SHA256_INTEGRITY_TASK_H
#define SHA256_INTEGRITY_TASK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define SHA256_INTEGRITY_EV_GROUP_BIT (1 << 3)

typedef struct {
  EventGroupHandle_t xTasksEventGroup;
  SemaphoreHandle_t xPrintMutex;
} SHA256_INTEGRITY_PARAMETERS;

hal_status_t SHA256_Integrity_Init( SHA256_INTEGRITY_PARAMETERS *params);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SHA256_INTEGRITY_TASK_H */
