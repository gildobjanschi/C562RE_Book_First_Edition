/*******************************************************************************
 * file           : main.c
 * brief          : Main program body
 ******************************************************************************/
#include "mx_hal_def.h"
#include "mx_system.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/queue.h"
#include "middleware/freertos/include/task.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "../../Shared/Faults/m33_it.h"
#include "adc_task.h"
#include "dac_task.h"

/*
 * @brief:  The application entry point.
 *
 * @retval: none but we specify int to comply with C99 standard
 */
int main(void) {
  SWD_Init();
#if SWD_PRINTF == RTT_PRINTF
  // SWO is configured at 144MHz and at this point the clock is set to 48MHz.
  // RTT does not require the MCU clock and therefore prints correctly
  // the message below.
  SWD_printf("---- VU_Meter started at %lu[Hz] ----\n",
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

  // Create the queue used to pass data from the ADC task to the DAC task.
  QueueHandle_t xFromADCQueue = xQueueCreate(FROM_ADC_QUEUE_SIZE,
      sizeof(uint16_t));
  if (xFromADCQueue == NULL) {
    ErrorHandler("xQueueCreate failed.");
    return (-1);
  }

  // Initialize the DAC task
  if (DAC_Task_Init(xFromADCQueue) != HAL_OK) {
    ErrorHandler("DAC_Task_Init failed.");
    return (-1);
  }

  // Initialize the ADC task
  if (ADC_Task_Init(xFromADCQueue) != HAL_OK) {
    ErrorHandler("ADC_Task_Init failed.");
    return (-1);
  }

  // Start the scheduler
  vTaskStartScheduler();
}

