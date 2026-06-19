/*******************************************************************************
 * file           : adc.h
 * brief          : Header ADC definitions.
 ******************************************************************************/
#ifndef ADC_H
#define ADC_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define EVENT_ADC_HALF_DATA   1
#define EVENT_ADC_CPLT_DATA   2
#define EVENT_ADC_ERROR       3

hal_status_t ADC_Init(QueueHandle_t ADCQueue);

typedef enum {
  SINGLE_DMA_CAPTURE,
  CONTINUOUS_DMA_CAPTURE
} app_adc_mode_t;
hal_status_t ADC_Start(uint32_t ulChannel, app_adc_mode_t mode);
hal_status_t ADC_Stop();

void ADC_Complete(uint8_t ucEvent, uint16_t *pBuffer, uint32_t ulBufferLength,
    uint32_t *pulDataLength);
void ADC_Error();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ADC_H */
