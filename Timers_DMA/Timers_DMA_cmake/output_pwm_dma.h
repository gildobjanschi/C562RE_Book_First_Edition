/*******************************************************************************
 * file           : output_pwm_dma.h
 * brief          : PWM DMA output
 ******************************************************************************/
#ifndef OUTPUT_PWM_DMA_H
#define OUTPUT_PWM_DMA_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

hal_status_t Output_PWM_DMA_Init();
hal_status_t Start_PWM_DMA();
hal_status_t Stop_PWM_DMA();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OUTPUT_PWM_DMA_H */
