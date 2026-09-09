/*******************************************************************************
 * file           : main.c
 * brief          : Main program body
 ******************************************************************************/
#include "mx_hal_def.h"
#include "mx_system.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/semphr.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "../../Shared/Faults/m33_it.h"
#include "rng_task.h"
#include "cordic_task.h"

static RNG_PARAMETERS rngParams;
static CORDIC_PARAMETERS cordicParams;

/*
 * brief:  The application entry point.
 *
 * @retval: none but we specify int to comply with C99 standard
 */
int main(void) {
  SWD_Init();
#if SWD_DEBUG == RTT_DEBUG
  // SWO is configured at 144MHz and at this point the clock is set to 48MHz.
  // RTT does not require the MCU clock and therefore prints correctly
  // the message below.
  SWD_printf("---- Compute started at %lu[Hz] ----\n",
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

  // Create the print mutex
  SemaphoreHandle_t xPrintMutex = xSemaphoreCreateMutex();
  if (xPrintMutex == NULL) {
    ErrorHandler("Cannot create mutex.\n");
    return (-1);
  }

  // Initialize the RNG task
  rngParams.xPrintMutex = xPrintMutex;
  if (RNG_Init(&rngParams) != HAL_OK) {
    ErrorHandler("RNG_Init failed.");
    return (-1);
  }

  // Initialize the CORDIC task
  cordicParams.xPrintMutex = xPrintMutex;
  if (CORDIC_Init(&cordicParams) != HAL_OK) {
    ErrorHandler("CORDIC_Init failed.");
    return (-1);
  }

  // Start the scheduler
  vTaskStartScheduler();
}
