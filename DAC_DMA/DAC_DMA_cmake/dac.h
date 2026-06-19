/*******************************************************************************
 * file           : dac.h
 * brief          : Header DAC definitions.
 ******************************************************************************/
#ifndef DAC_H
#define DAC_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Interrupt events
#define EVENT_DAC_HALF_CPLT_DATA  1
#define EVENT_DAC_CPLT_DATA       2
#define EVENT_DAC_ERROR           3

hal_status_t DAC_Init(QueueHandle_t DACQueue);
hal_status_t DAC_Start();
hal_status_t DAC_Stop();
void DAC_Complete(uint8_t ucEvent);
void DAC_Error();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DAC_H */
