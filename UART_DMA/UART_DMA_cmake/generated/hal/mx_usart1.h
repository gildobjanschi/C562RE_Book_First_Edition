/**
  ******************************************************************************
  * @file           : mx_usart1.h
  * @brief          : Header for mx_usart1.c file.
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
#ifndef MX_USART1_H
#define MX_USART1_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "stm32_hal.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

/** Primary aliases for USART1_RX pin */
#define PA10_PORT                             HAL_GPIOA
#define PA10_PIN                              HAL_GPIO_PIN_10

/** Primary aliases for USART1_TX pin */
#define PA9_PORT                              HAL_GPIOA
#define PA9_PIN                               HAL_GPIO_PIN_9

/** Primary aliases for USART1_CTS pin */
#define USB_FS_N_PORT                         HAL_GPIOA
#define USB_FS_N_PIN                          HAL_GPIO_PIN_11

/** Primary aliases for USART1_RTS pin */
#define USB_FS_P_PORT                         HAL_GPIOA
#define USB_FS_P_PIN                          HAL_GPIO_PIN_12

/* Exported macros -----------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

/******************************************************************************/
/* Exported functions for UART in HAL layer */
/******************************************************************************/
/**
  * @brief mx_usart1_uart init function
  * This function configures the hardware resources used in this example
  * @retval pointer to handle or NULL in case of failure
  */
hal_uart_handle_t *mx_usart1_uart_init(void);

/**
  * @brief  De-initialize mx_usart1_uart instance and return it.
  * @retval None
  */
void mx_usart1_uart_deinit(void);

/**
  * @brief  Get the mx_usart1_uart object.
  * @retval Pointer on the mx_usart1_uartHandle
  */
hal_uart_handle_t *mx_usart1_uart_gethandle(void);

/******************************************************************************/
/*                          USART1 global interrupt                           */
/******************************************************************************/
void USART1_IRQHandler(void);

/******************************************************************************/
/*                      LPDMA1 channel0 global interrupt                      */
/******************************************************************************/
void LPDMA1_CH0_IRQHandler(void);

/******************************************************************************/
/*                      LPDMA1 channel1 global interrupt                      */
/******************************************************************************/
void LPDMA1_CH1_IRQHandler(void);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MX_USART1_H */
