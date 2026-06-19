/*******************************************************************************
 * file           : output_pwm_dma.c
 * brief          : Output PWM DMA implementation
 ******************************************************************************/
#include "mx_hal_def.h"
#include "../../Shared/Debug/swd_printf.h"
#include "output_pwm_dma.h"

// The size of the DMA buffer
#define DMA_BUFFER_SIZE 20U

// The number of pulses that make up the timer period
#define TIM5_ARR 1000
// The DMA buffer
uint32_t OutputDmaBuffer[DMA_BUFFER_SIZE] = {
  TIM5_ARR * 1 / 20,
  TIM5_ARR * 2 / 20,
  TIM5_ARR * 3 / 20,
  TIM5_ARR * 4 / 20,
  TIM5_ARR * 5 / 20,
  TIM5_ARR * 6 / 20,
  TIM5_ARR * 7 / 20,
  TIM5_ARR * 8 / 20,
  TIM5_ARR * 9 / 20,
  TIM5_ARR * 10 / 20,
  TIM5_ARR * 11 / 20,
  TIM5_ARR * 12 / 20,
  TIM5_ARR * 13 / 20,
  TIM5_ARR * 14 / 20,
  TIM5_ARR * 15 / 20,
  TIM5_ARR * 16 / 20,
  TIM5_ARR * 17 / 20,
  TIM5_ARR * 18 / 20,
  TIM5_ARR * 19 / 20,
  0
};

/*
 * @brief  Initialize the PWM DMA output
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t Output_PWM_DMA_Init() {

  return HAL_OK;
}

/*
 * @brief  Start PWM DMA output
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t Start_PWM_DMA() {
  hal_status_t status;
  hal_tim_handle_t *htim5 = mx_tim5_gethandle();

  // Start the DMA channel
  status = HAL_TIM_OC_StartChannel_DMA(htim5, HAL_TIM_CHANNEL_1,
      (uint8_t*) OutputDmaBuffer, DMA_BUFFER_SIZE * sizeof(uint32_t));
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_OC_StartChannel_DMA failed.\n");
    return status;
  }

  // Start the timer
  status = HAL_TIM_Start(htim5);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_Start failed.\n");
    return status;
  }

  return HAL_OK;
}

/*
 * @brief  Stop PWM DMA output
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t Stop_PWM_DMA() {
  hal_status_t status;
  hal_tim_handle_t *htim5 = mx_tim5_gethandle();

  // Stop the DMA channel
  status = HAL_TIM_OC_StopChannel_DMA(htim5, HAL_TIM_CHANNEL_1);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_OC_StopChannel_DMA failed.\n");
    return status;
  }

  // Stop the timer
  status = HAL_TIM_Stop(htim5);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_Stop failed.\n");
    return status;
  }

  return HAL_OK;
}
