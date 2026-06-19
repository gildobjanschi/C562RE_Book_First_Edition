/**
  ******************************************************************************
  * @file           : mx_gpio_default.c
  * @brief          : gpio_default Peripheral initialization
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
#include "mx_gpio_default.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/* Exported variables by reference -------------------------------------------*/
static hal_exti_handle_t hEXTI7;
static hal_exti_handle_t hEXTI11;
static hal_exti_handle_t hEXTI13;
static hal_exti_handle_t hEXTI2;

/******************************************************************************/
/* Exported functions for GPIO in HAL layer                                   */
/******************************************************************************/
system_status_t mx_gpio_default_init(void)
{
  hal_gpio_config_t  gpio_config;

  HAL_RCC_GPIOA_EnableClock();

  HAL_RCC_GPIOC_EnableClock();

  HAL_RCC_GPIOD_EnableClock();

  /*
    GPIO pin labels :
    PA5   ---------> PA5, LD1
    */
  /* Configure PA5 GPIO pin in output mode */
  gpio_config.mode            = HAL_GPIO_MODE_OUTPUT;
  gpio_config.speed           = HAL_GPIO_SPEED_FREQ_LOW;
  gpio_config.pull            = HAL_GPIO_PULL_NO;
  gpio_config.output_type     = HAL_GPIO_OUTPUT_PUSHPULL;
  gpio_config.init_state      = PA5_INIT_STATE;
  if (HAL_GPIO_Init(PA5_PORT, PA5_PIN, &gpio_config) != HAL_OK)
  {
    return SYSTEM_PERIPHERAL_ERROR;
  }

  /*
    GPIO pin labels :
    PA7   ---------> PA7, RE_BTN
    */
  /* Configure PA7 GPIO pin in input mode */
  gpio_config.mode            = HAL_GPIO_MODE_INPUT;
  gpio_config.pull            = HAL_GPIO_PULL_NO;
  if (HAL_GPIO_Init(PA7_PORT, PA7_PIN, &gpio_config) != HAL_OK)
  {
    return SYSTEM_PERIPHERAL_ERROR;
  }

  hal_exti_config_t exti_config;

  /* Initialize the EXTI for line 7 */
  HAL_EXTI_Init(&hEXTI7, HAL_EXTI_LINE_7);

  /* Set the trigger as RISING_FALLING for the GPIOA */
  exti_config.trigger   = HAL_EXTI_TRIGGER_RISING_FALLING;
  exti_config.gpio_port = HAL_EXTI_GPIOA;
  HAL_EXTI_SetConfig(&hEXTI7, &exti_config);

  /* Enable the INTERRUPT mode */
  HAL_EXTI_Enable(&hEXTI7, HAL_EXTI_MODE_INTERRUPT);

  /* Set line 7 Interrupt priority */
  HAL_CORTEX_NVIC_SetPriority(EXTI7_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_5, HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(EXTI7_IRQn);

  /*
    GPIO pin labels :
    PC11  ---------> PC11, RE_A
    */
  /* Configure PC11 GPIO pin in input mode */
  gpio_config.mode            = HAL_GPIO_MODE_INPUT;
  gpio_config.pull            = HAL_GPIO_PULL_DOWN;
  if (HAL_GPIO_Init(PC11_PORT, PC11_PIN, &gpio_config) != HAL_OK)
  {
    return SYSTEM_PERIPHERAL_ERROR;
  }

  /* Initialize the EXTI for line 11 */
  HAL_EXTI_Init(&hEXTI11, HAL_EXTI_LINE_11);

  /* Set the trigger as RISING_FALLING for the GPIOC */
  exti_config.trigger   = HAL_EXTI_TRIGGER_RISING_FALLING;
  exti_config.gpio_port = HAL_EXTI_GPIOC;
  HAL_EXTI_SetConfig(&hEXTI11, &exti_config);

  /* Enable the INTERRUPT mode */
  HAL_EXTI_Enable(&hEXTI11, HAL_EXTI_MODE_INTERRUPT);

  /* Set line 11 Interrupt priority */
  HAL_CORTEX_NVIC_SetPriority(EXTI11_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_5, HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(EXTI11_IRQn);

  /*
    GPIO pin labels :
    PC13  ---------> PC13, B1
    */
  /* Configure PC13 GPIO pin in input mode */
  gpio_config.mode            = HAL_GPIO_MODE_INPUT;
  gpio_config.pull            = HAL_GPIO_PULL_NO;
  if (HAL_GPIO_Init(PC13_PORT, PC13_PIN, &gpio_config) != HAL_OK)
  {
    return SYSTEM_PERIPHERAL_ERROR;
  }

  /* Initialize the EXTI for line 13 */
  HAL_EXTI_Init(&hEXTI13, HAL_EXTI_LINE_13);

  /* Set the trigger as RISING_FALLING for the GPIOC */
  exti_config.trigger   = HAL_EXTI_TRIGGER_RISING_FALLING;
  exti_config.gpio_port = HAL_EXTI_GPIOC;
  HAL_EXTI_SetConfig(&hEXTI13, &exti_config);

  /* Enable the INTERRUPT mode */
  HAL_EXTI_Enable(&hEXTI13, HAL_EXTI_MODE_INTERRUPT);

  /* Set line 13 Interrupt priority */
  HAL_CORTEX_NVIC_SetPriority(EXTI13_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_5, HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(EXTI13_IRQn);

  /*
    GPIO pin labels :
    PD2   ---------> PD2, RE_B
    */
  /* Configure PD2 GPIO pin in input mode */
  gpio_config.mode            = HAL_GPIO_MODE_INPUT;
  gpio_config.pull            = HAL_GPIO_PULL_DOWN;
  if (HAL_GPIO_Init(PD2_PORT, PD2_PIN, &gpio_config) != HAL_OK)
  {
    return SYSTEM_PERIPHERAL_ERROR;
  }

  /* Initialize the EXTI for line 2 */
  HAL_EXTI_Init(&hEXTI2, HAL_EXTI_LINE_2);

  /* Set the trigger as RISING_FALLING for the GPIOD */
  exti_config.trigger   = HAL_EXTI_TRIGGER_RISING_FALLING;
  exti_config.gpio_port = HAL_EXTI_GPIOD;
  HAL_EXTI_SetConfig(&hEXTI2, &exti_config);

  /* Enable the INTERRUPT mode */
  HAL_EXTI_Enable(&hEXTI2, HAL_EXTI_MODE_INTERRUPT);

  /* Set line 2 Interrupt priority */
  HAL_CORTEX_NVIC_SetPriority(EXTI2_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_5, HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(EXTI2_IRQn);

  return SYSTEM_OK;
}

system_status_t mx_gpio_default_deinit(void)
{
  /* De-initialize the EXTI for GPIOA line7 */
  HAL_EXTI_DeInit(&hEXTI7);

  /* set line 7 Interrupt priority */
  HAL_CORTEX_NVIC_DisableIRQ(EXTI7_IRQn);

  /* De-initialize the EXTI for GPIOC line11 */
  HAL_EXTI_DeInit(&hEXTI11);

  /* set line 11 Interrupt priority */
  HAL_CORTEX_NVIC_DisableIRQ(EXTI11_IRQn);

  /* De-initialize the EXTI for GPIOC line13 */
  HAL_EXTI_DeInit(&hEXTI13);

  /* set line 13 Interrupt priority */
  HAL_CORTEX_NVIC_DisableIRQ(EXTI13_IRQn);

  /* De-initialize the EXTI for GPIOD line2 */
  HAL_EXTI_DeInit(&hEXTI2);

  /* set line 2 Interrupt priority */
  HAL_CORTEX_NVIC_DisableIRQ(EXTI2_IRQn);

  /* De-initialize pins of GPIOA port */
  HAL_GPIO_DeInit(HAL_GPIOA, PA5_PIN | PA7_PIN);

  /* De-initialize pins of GPIOC port */
  HAL_GPIO_DeInit(HAL_GPIOC, PC11_PIN | PC13_PIN);

  /* De-initialize pins of GPIOD port */
  HAL_GPIO_DeInit(PD2_PORT, PD2_PIN);

  return SYSTEM_OK;
}

hal_exti_handle_t *mx_gpio_default_exti7_gethandle(void)
{
  return &hEXTI7;
}

hal_exti_handle_t *mx_gpio_default_exti11_gethandle(void)
{
  return &hEXTI11;
}

hal_exti_handle_t *mx_gpio_default_exti13_gethandle(void)
{
  return &hEXTI13;
}

hal_exti_handle_t *mx_gpio_default_exti2_gethandle(void)
{
  return &hEXTI2;
}

/******************************************************************************/
/*                            EXTI Line7 interrupt                            */
/******************************************************************************/
void EXTI7_IRQHandler(void)
{
  HAL_EXTI_IRQHandler(&hEXTI7);
}

/******************************************************************************/
/*                           EXTI Line11 interrupt                            */
/******************************************************************************/
void EXTI11_IRQHandler(void)
{
  HAL_EXTI_IRQHandler(&hEXTI11);
}

/******************************************************************************/
/*                           EXTI Line13 interrupt                            */
/******************************************************************************/
void EXTI13_IRQHandler(void)
{
  HAL_EXTI_IRQHandler(&hEXTI13);
}

/******************************************************************************/
/*                            EXTI Line2 interrupt                            */
/******************************************************************************/
void EXTI2_IRQHandler(void)
{
  HAL_EXTI_IRQHandler(&hEXTI2);
}
