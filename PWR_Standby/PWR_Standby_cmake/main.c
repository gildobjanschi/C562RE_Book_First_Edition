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
  SWD_printf("---- PWR_Standby started at %lu[Hz] ----\n",
      HAL_RCC_GetHCLKFreq());
#endif
  /*
   * The following code must be added in mx_rtc_init() (in mx_rtc.c )
   * after HAL_RTC_ExitInitMode() and before HAL_RTC_WAKEUP_SetConfig().
   *
   * // Gil: Modification after code was generated
   * if (LL_PWR_IsActiveFlag_SB() == 1U) {
   *   HAL_RTC_WAKEUP_Stop();
   * }
   */

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


