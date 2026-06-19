/**
  ******************************************************************************
  * @file           : mx_tim7.c
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
#include "mx_tim7.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private functions prototype------------------------------------------------*/
/* Exported variables by reference--------------------------------------------*/
static hal_tim_handle_t hTIM7;

/* Exported function definition ----------------------------------------------*/
/******************************************************************************/
/* Exported functions for TIM7 in HAL layer */
/******************************************************************************/
hal_tim_handle_t *mx_tim7_init(void)
{
  if (HAL_TIM_Init(&hTIM7, HAL_TIM7) != HAL_OK)
  {
    return NULL;
  }

  HAL_RCC_TIM7_EnableClock();

  /* No GPIO configuration required for TIM7 */
  /* Enable the Timer global interrupt */
  HAL_CORTEX_NVIC_SetPriority(TIM7_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_0, HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(TIM7_IRQn);

  return &hTIM7;
}

void mx_tim7_deinit(void)
{
  (void)HAL_TIM_DeInit(&hTIM7);

  HAL_RCC_TIM7_DisableClock();

  HAL_RCC_TIM7_Reset();

  /* No GPIO de-initialization required for TIM7 */
  /* Disable Timer global interrupt */
  HAL_CORTEX_NVIC_DisableIRQ(TIM7_IRQn);
}

hal_tim_handle_t *mx_tim7_gethandle(void)
{
  return &hTIM7;
}

/******************************************************************************/
/*                           TIM7 global interrupt                            */
/******************************************************************************/
void TIM7_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&hTIM7);
}
