/*******************************************************************************
 * file           : main.c
 * brief          : Main program body
 ******************************************************************************/
#include "mx_hal_def.h"
#include "mx_system.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "../../Shared/Faults/m33_it.h"
#include "app_task.h"

/*
 * brief:  The application entry point.
 *
 * @retval: none but we specify int to comply with C99 standard
 */
int main(void) {
  SWD_Init();
#if SWD_PRINTF == RTT_PRINTF
  // SWO is configured at 144MHz and at this point the clock is set to 48MHz.
  // RTT does not require the MCU clock and therefore prints correctly
  // the message below.
  SWD_printf("---- RTC started at %lu[Hz] ----\n",
      HAL_RCC_GetHCLKFreq());
#endif

  /*
   * System Init: this code placed in targets folder initializes your system.
   * It calls the initialization (and sets the initial configuration) of the
   * peripherals. You can use STM32CubeMX to generate and call this code or
   * not in this project. It also contains the HAL initialization and the
   * initial clock configuration.
   */
  if (mx_system_init() != SYSTEM_OK) {
    ErrorHandler("mx_system_init failed.");
    return (-1);
  }

  // Configure fault handling
  Fault_Config();

  SWD_printf("---- MCU configured at %lu[Hz] ----\n", HAL_RCC_GetHCLKFreq());

  // Initialize the application
  if (App_Init() != HAL_OK) {
    ErrorHandler("App_Init failed.");
    return (-1);
  }

  // Start the scheduler
  vTaskStartScheduler();
}

/*
 * @brief: User hook function called before the HAL_Init() function
 */
system_status_t pre_system_init_hook(void)
{
  /*
   * Reset the backup domain to prevent reusing any previous
   * RTC calendar and alarm.
   * So RTC peripheral registers and RTC clock source (LSE or LSI) will be in
   * their reset state whatever was programmed before.
   * This is implemented before the LSE or the LSI is properly configured in
   * `system_clock_config()`.
   */
  HAL_PWR_DisableRTCDomainWriteProtection();
  HAL_RCC_ResetRTCDomain();

  return SYSTEM_OK;
}
