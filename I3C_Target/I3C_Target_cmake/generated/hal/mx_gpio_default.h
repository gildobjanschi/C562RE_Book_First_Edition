/**
  ******************************************************************************
  * @file           : mx_gpio_default.h
  * @brief          : Header for mx_gpio_default.c file.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef MX_GPIO_DEFAULT_H
#define MX_GPIO_DEFAULT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "stm32_hal.h"
#include "mx_def.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/******************************************************************************/
/* Exported defines for gpio_default in HAL layer                             */
/******************************************************************************/

/* Primary aliases for GPIO PB13 pin */
#define LED_R_PORT                                      HAL_GPIOB
#define LED_R_PIN                                       HAL_GPIO_PIN_13
#define LED_R_INIT_STATE                                HAL_GPIO_PIN_RESET
#define LED_R_ACTIVE_STATE                              HAL_GPIO_PIN_SET
#define LED_R_INACTIVE_STATE                            HAL_GPIO_PIN_RESET

/* Primary aliases for GPIO PB14 pin */
#define LED_G_PORT                                      HAL_GPIOB
#define LED_G_PIN                                       HAL_GPIO_PIN_14
#define LED_G_INIT_STATE                                HAL_GPIO_PIN_RESET
#define LED_G_ACTIVE_STATE                              HAL_GPIO_PIN_SET
#define LED_G_INACTIVE_STATE                            HAL_GPIO_PIN_RESET

/* Primary aliases for GPIO PB15 pin */
#define LED_B_PORT                                      HAL_GPIOB
#define LED_B_PIN                                       HAL_GPIO_PIN_15
#define LED_B_INIT_STATE                                HAL_GPIO_PIN_RESET
#define LED_B_ACTIVE_STATE                              HAL_GPIO_PIN_SET
#define LED_B_INACTIVE_STATE                            HAL_GPIO_PIN_RESET

/* Secondary aliases for GPIO PB15 pin */
#define LD1_PORT                                        HAL_GPIOB
#define LD1_PIN                                         HAL_GPIO_PIN_15
#define LD1_INIT_STATE                                  HAL_GPIO_PIN_RESET
#define LD1_ACTIVE_STATE                                HAL_GPIO_PIN_SET
#define LD1_INACTIVE_STATE                              HAL_GPIO_PIN_RESET

/* Exported macros -----------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
/******************************************************************************/
/* Exported functions for gpio_default in HAL layer                           */
/******************************************************************************/
/**
  * @brief mx_gpio_default init function
  * This function configures the hardware resources used in this example
  * @retval 0  GPIO group correctly initialized
  * @retval -1 Issue detected during GPIO group initialization
  */
system_status_t mx_gpio_default_init(void);

/**
  * @brief  De-initialize gpio_default instance.
  */
system_status_t mx_gpio_default_deinit(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MX_GPIO_DEFAULT_H */
