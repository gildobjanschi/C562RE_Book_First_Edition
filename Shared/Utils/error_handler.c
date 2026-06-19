/*******************************************************************************
 * @file    error_handler.c
 * @brief   Application error handler
 ******************************************************************************/
#include "../shared_def.h"
#include "../Debug/swd_printf.h"
#include "error_handler.h"

/*
 * @brief  The application error handler
 *
 * @param error A description of the error
 */
void ErrorHandler(char *error) {
  SWD_printf("ErrorHandler: %s\n", error);
#ifndef DEBUG
  STM32_UNUSED(error);
#endif
  // Turn the LED OFF
  HAL_GPIO_WritePin(LD1_PORT, LD1_PIN, HAL_GPIO_PIN_RESET);

  // Flash the LED fast
  while (1) {
    // Turn the LED ON
    HAL_GPIO_TogglePin(LD1_PORT, LD1_PIN);
    for (volatile uint32_t i = 0; i < 2500000; i++);

    // Turn the LED OFF
    HAL_GPIO_TogglePin(LD1_PORT, LD1_PIN);
    for (volatile uint32_t i = 0; i < 2500000; i++);
  }
}
