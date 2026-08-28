/*******************************************************************************
 * file           : app_task.c
 * brief          : The application task implementation.
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "app_task.h"

// Needed only for Experiment 3
#define GPIO_GET_INSTANCE(instance)  ((GPIO_TypeDef *)((uint32_t)(instance)))

/*
 * @brief:  The application task function
 *
 * @param pvParameters Task parameters
 */
static void vAppTaskFunction(void *pvParameters) {
  /*
  // -------------------------------------------------------------------------
  // This code will trigger a [MemManage_Handler: Instr. access violation.]
  // Execute a function located at an invalid code execution address
  void (*func_ptr)(void) = (void (*)(void))0x8FFE000;
  func_ptr();
  // -------------------------------------------------------------------------
   */

  /*
  // -------------------------------------------------------------------------
  // This code will trigger a [BusFault_Handler: Imprecise bus error.]
  uint32_t* bad_pointer = (uint32_t*)1000011112;
  *bad_pointer = 0;
  // -------------------------------------------------------------------------
  */

  /*
  // -------------------------------------------------------------------------
  // This code will trigger a [UsageFault_Handler: Divide-by-zero.]
  volatile int numerator = 42;
  volatile int denominator = 0;
  volatile int result;

  // This will trigger UsageFault if trap is enabled
  result = numerator / denominator;
  #ifdef DEBUG
  SWD_printf("Result = %u\n", result);
  #endif
  // -------------------------------------------------------------------------
  */

  /*
  // Experiment 1
  while (1) {
    HAL_GPIO_TogglePin(LD1_PORT, LD1_PIN);
    HAL_Delay(1);
  }
  */

  /*
  // Experiment 2
  while (1) {
    HAL_GPIO_TogglePin(LD1_PORT, LD1_PIN);
  }
  */

  /*
  // Experiment 3
  while (1) {
    LL_GPIO_TogglePin(GPIO_GET_INSTANCE(LD1_PORT), LD1_PIN);
  }
  */

  SWD_printf("--> You should see the LED blinking.\n");

  while (1) {
    HAL_GPIO_TogglePin(LD1_PORT, LD1_PIN);

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/*
 * @brief  Initialize the application main task.
 *    When this function is called, buttons must not be pressed.
 *
 * @retval HAL_OK if the method succeeds.
 */
hal_status_t App_Init() {
  if (xTaskCreate(
    vAppTaskFunction, // Function that implements the task
    "App_Task",       // Text name for the task
    128,                // Stack size in words
    NULL,               // Parameter passed into the task
    10,                 // Priority
    NULL                // Used to pass out the task's handle
  ) == pdPASS) {
    return HAL_OK;
  } else {
    return HAL_ERROR;
  }
}

