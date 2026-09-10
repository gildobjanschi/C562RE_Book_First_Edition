/*******************************************************************************
 * file           : crc_task.h
 * brief          : CRC task definitions.
 ******************************************************************************/
#ifndef CRC_TASK_H
#define CRC_TASK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define CRC_EV_GROUP_BIT (1 << 2)

typedef struct {
  EventGroupHandle_t xTasksEventGroup;
  SemaphoreHandle_t xPrintMutex;
} CRC_PARAMETERS;

hal_status_t CRC_Init(CRC_PARAMETERS *params);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CRC_TASK_H */
