/*******************************************************************************
 * file           : cordic_task.h
 * brief          : CORDIC task definitions.
 ******************************************************************************/
#ifndef CORDIC_TASK_H
#define CORDIC_TASK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
  xSemaphoreHandle xPrintMutex;
} CORDIC_PARAMETERS;

hal_status_t CORDIC_Init(CORDIC_PARAMETERS *params);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CORDIC_TASK_H */
