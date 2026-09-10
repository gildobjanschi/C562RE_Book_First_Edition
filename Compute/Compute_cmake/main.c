/*******************************************************************************
 * file           : main.c
 * brief          : Main program body
 ******************************************************************************/
#include "mx_hal_def.h"
#include "mx_system.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/semphr.h"
#include "middleware/freertos/include/event_groups.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "../../Shared/Faults/m33_it.h"
#include "rng_task.h"
#include "cordic_task.h"
#include "crc_task.h"

static EventGroupHandle_t xTasksEventGroup;
static RNG_PARAMETERS rngParams;
static CORDIC_PARAMETERS cordicParams;
static CRC_PARAMETERS crcParams;

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

  // Create the event group
  xTasksEventGroup = xEventGroupCreate();
  if (xTasksEventGroup == NULL) {
    ErrorHandler("Cannot create event group.\n");
    return (-1);
  }

  // Initialize the RNG task
  rngParams.xTasksEventGroup = xTasksEventGroup;
  rngParams.xPrintMutex = xPrintMutex;
  if (RNG_Init(&rngParams) != HAL_OK) {
    ErrorHandler("RNG_Init failed.");
    return (-1);
  }

  // Initialize the CORDIC task
  cordicParams.xTasksEventGroup = xTasksEventGroup;
  cordicParams.xPrintMutex = xPrintMutex;
  if (CORDIC_Init(&cordicParams) != HAL_OK) {
    ErrorHandler("CORDIC_Init failed.");
    return (-1);
  }

  // Initialize the CRC task
  crcParams.xTasksEventGroup = xTasksEventGroup;
  crcParams.xPrintMutex = xPrintMutex;
  if (CRC_Init(&crcParams) != HAL_OK) {
    ErrorHandler("CRC_Init failed.");
    return (-1);
  }

  // Set the event bits for all tasks
  xEventGroupSetBits(xTasksEventGroup,
      RNG_EV_GROUP_BIT | CORDIC_EV_GROUP_BIT | CRC_EV_GROUP_BIT);

  // Start the scheduler
  vTaskStartScheduler();
}
