/**
  ******************************************************************************
  * @file           : mx_pwr.c
  * @brief          : PWR Peripheral initialization
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
#include "mx_pwr.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private functions prototype------------------------------------------------*/

/******************************************************************************/
/* Exported functions for PWR in HAL layer */
/******************************************************************************/

system_status_t mx_pwr_init(void)
{
  hal_pwr_wakeup_pin_config_t wkup_config;

  /****************************************************************************/
  /* GPIO Configuration                                                       */
  /****************************************************************************/
  /* ### PWR GPIO Configuration ########################### */
  /* GPIO Clocks activation */
  HAL_RCC_GPIOC_EnableClock();

  hal_gpio_config_t  gpio_config;

  /**
    [GPIO Pin] ------> [Signal Name] ------> [Labels]

       PC2     ------>   PWR_CSLEEP   ------>  PC2
       PC3     ------>   PWR_CSTOP   ------>  PC3
    **/
  gpio_config.mode        = HAL_GPIO_MODE_ALTERNATE;
  gpio_config.output_type = HAL_GPIO_OUTPUT_PUSHPULL;
  gpio_config.speed       = HAL_GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_config.alternate   = HAL_GPIO_AF_0;
  HAL_GPIO_Init(HAL_GPIOC, PC2_PIN | PC3_PIN, &gpio_config);

  /****************************************************************************/
  /* Wakeup Pins Configurations                                               */
  /****************************************************************************/
  /* Configuration of selected Wakeup pins with pull NO and HIGH polarity */
  wkup_config.polarity = HAL_PWR_WAKEUP_PIN_POLARITY_HIGH;
  wkup_config.pull     = HAL_PWR_WAKEUP_PIN_PULL_NO;

  if (HAL_PWR_LP_SetConfigWakeupPin(HAL_PWR_WAKEUP_PIN_4, &wkup_config) != HAL_OK)
  {
    return SYSTEM_POWER_ERROR;
  }

  /****************************************************************************/
  /* Enable all configured Wakeup Pins                                        */
  /****************************************************************************/
  HAL_PWR_LP_EnableWakeupPin(HAL_PWR_WAKEUP_PIN_4);

  return SYSTEM_OK;
}

system_status_t mx_pwr_deinit(void)
{
  /****************************************************************************/
  /* Status pin GPIO Deinitialization                                         */
  /****************************************************************************/

  /* De-initialize all GPIOC pins associated with PWR */
  HAL_GPIO_DeInit(HAL_GPIOC, PC2_PIN | PC3_PIN);

  /****************************************************************************/
  /* Disable all configured Wakeup Pins                                       */
  /****************************************************************************/
  HAL_PWR_LP_DisableWakeupPin(HAL_PWR_WAKEUP_PIN_4);

  return SYSTEM_OK;
}
