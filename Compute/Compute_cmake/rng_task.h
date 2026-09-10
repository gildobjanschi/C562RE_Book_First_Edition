/*******************************************************************************
 * file           : rng_task.h
 * brief          : RNG task definitions.
 ******************************************************************************/
#ifndef RNG_TASK_H
#define RNG_TASK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define RNG_EV_GROUP_BIT (1 << 0)

typedef struct {
  EventGroupHandle_t xTasksEventGroup;
  SemaphoreHandle_t xPrintMutex;
} RNG_PARAMETERS;

hal_status_t RNG_Init(RNG_PARAMETERS *params);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RNG_TASK_H */
