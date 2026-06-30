/*******************************************************************************
 * file           : dac_task.h
 * brief          : DAC task definitions.
 ******************************************************************************/
#ifndef DAC_TASK_H
#define DAC_TASK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define FROM_ADC_QUEUE_SIZE 4

hal_status_t DAC_Task_Init(QueueHandle_t xFromADCQueue);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DAC_TASK_H */
