/**
  ******************************************************************************
  * @file           : mx_tim5.c
  * @brief          : Peripheral initialization
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the mx_stm32c5xx_hal_drivers_license.md file
  * in the same directory as the generated code.
  * If no mx_stm32c5xx_hal_drivers_license.md file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "mx_tim5.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private functions prototype------------------------------------------------*/
/* Exported variables by reference--------------------------------------------*/
static hal_tim_handle_t hTIM5;

/* Exported function definition ----------------------------------------------*/
/******************************************************************************/
/* Exported functions for TIM5 in HAL layer */
/******************************************************************************/
hal_tim_handle_t *mx_tim5_init(void)
{
  if (HAL_TIM_Init(&hTIM5, HAL_TIM5) != HAL_OK)
  {
    return NULL;
  }

  HAL_RCC_TIM5_EnableClock();

  /* Timer configuration to reach the output frequency at 1 kHz */
  hal_tim_config_t config;
  config.prescaler              = 143;
  config.counter_mode           = HAL_TIM_COUNTER_UP;
  config.period                 = 0x3E7;
  config.repetition_counter     = 0;
  config.clock_sel.clock_source = HAL_TIM_CLK_INTERNAL;
  if (HAL_TIM_SetConfig(&hTIM5, &config) != HAL_OK)
  {
    return NULL;
  }

  /* Sampling Clock */
  if (HAL_TIM_SetDTSPrescaler(&hTIM5, HAL_TIM_DTS_DIV1) != HAL_OK)
  {
    return NULL;
  }
  if (HAL_TIM_SetDTS2Prescaler(&hTIM5, HAL_TIM_DTS2_DIV1) != HAL_OK)
  {
    return NULL;
  }

  if (HAL_TIM_EnableAutoReloadPreload(&hTIM5) != HAL_OK)
  {
    return NULL;
  }

  hal_tim_oc_channel_config_t oc_config;

  oc_config.polarity       = HAL_TIM_OC_HIGH;
  if (HAL_TIM_OC_SetConfigChannel(&hTIM5, HAL_TIM_CHANNEL_1, &oc_config) != HAL_OK)
  {
    return NULL;
  }
  hal_tim_oc_compare_unit_config_t oc_compare_unit_config;

  oc_compare_unit_config.mode  = HAL_TIM_OC_TOGGLE;
  oc_compare_unit_config.pulse = 0xFA;
  if (HAL_TIM_OC_SetConfigCompareUnit(&hTIM5, HAL_TIM_OC_COMPARE_UNIT_1,
                                      &oc_compare_unit_config) != HAL_OK)
  {
    return NULL;
  }

  if (HAL_TIM_OC_EnableCompareFastMode(&hTIM5, HAL_TIM_OC_COMPARE_UNIT_1) != HAL_OK)
  {
    return NULL;
  }

  oc_config.polarity       = HAL_TIM_OC_HIGH;
  if (HAL_TIM_OC_SetConfigChannel(&hTIM5, HAL_TIM_CHANNEL_2, &oc_config) != HAL_OK)
  {
    return NULL;
  }
  oc_compare_unit_config.mode  = HAL_TIM_OC_PWM1;
  oc_compare_unit_config.pulse = 0x1F4;
  if (HAL_TIM_OC_SetConfigCompareUnit(&hTIM5, HAL_TIM_OC_COMPARE_UNIT_2,
                                      &oc_compare_unit_config) != HAL_OK)
  {
    return NULL;
  }

  if (HAL_TIM_OC_EnableCompareFastMode(&hTIM5, HAL_TIM_OC_COMPARE_UNIT_2) != HAL_OK)
  {
    return NULL;
  }

  /* Update Event Management */
  if (HAL_TIM_SetUpdateSource(&hTIM5, HAL_TIM_UPDATE_REGULAR) != HAL_OK)
  {
    return NULL;
  }
  if (HAL_TIM_EnableUpdateGeneration(&hTIM5) != HAL_OK)
  {
    return NULL;
  }
  /* Master Mode Configuration */
  /* ### TIM5 GPIO Configuration ########################### */
  /* GPIO Clocks activation */
  HAL_RCC_GPIOA_EnableClock();

  hal_gpio_config_t  gpio_config;

  /**
    [GPIO Pin] ------> [Signal Name] ------> [Labels]

       PA0     ------>   TIM5_CH1   ------>  PA0
       PA1     ------>   TIM5_CH2   ------>  PA1
    **/
  gpio_config.mode        = HAL_GPIO_MODE_ALTERNATE;
  gpio_config.output_type = HAL_GPIO_OUTPUT_PUSHPULL;
  gpio_config.pull        = HAL_GPIO_PULL_NO;
  gpio_config.speed       = HAL_GPIO_SPEED_FREQ_LOW;
  gpio_config.alternate   = HAL_GPIO_AF_2;
  HAL_GPIO_Init(HAL_GPIOA, PA0_PIN | PA1_PIN, &gpio_config);

  /* Enable the Timer global interrupt */
  HAL_CORTEX_NVIC_SetPriority(TIM5_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_5, HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(TIM5_IRQn);

  return &hTIM5;
}

void mx_tim5_deinit(void)
{
  (void)HAL_TIM_DeInit(&hTIM5);

  HAL_RCC_TIM5_DisableClock();

  HAL_RCC_TIM5_Reset();

  /* De-initialize all GPIOA pins associated with TIM5 */
  HAL_GPIO_DeInit(HAL_GPIOA, PA0_PIN | PA1_PIN);

  /* Disable Timer global interrupt */
  HAL_CORTEX_NVIC_DisableIRQ(TIM5_IRQn);
}

hal_tim_handle_t *mx_tim5_gethandle(void)
{
  return &hTIM5;
}

/******************************************************************************/
/*                           TIM5 global interrupt                            */
/******************************************************************************/
void TIM5_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&hTIM5);
}
