/*******************************************************************************
 * file           : dac_task.c
 * brief          : The DAC task implementation.
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/queue.h"
#include "middleware/freertos/include/timers.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "../../Shared/Input/input_soft_timers.h"
#include "app_input.h"
#include "dac_task.h"
#include "dac.h"

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 * @param xDACQueue The DAC queue
 */
static void exitAppTask(char *error, QueueHandle_t xDACQueue) {
  ErrorHandler(error);

  // Free the DAC queue
  if (xDACQueue != NULL) {
    vQueueDelete(xDACQueue);
  }

  vTaskDelete(NULL);
}

/*
 * @brief:  The DAC task function
 *
 * @param pvParameters Task parameters
 */
static void vDACTaskFunction(void *pvParameters) {
  // Create the DAC queue
  QueueHandle_t xDACQueue = xQueueCreate(8, sizeof(uint8_t));
  if (xDACQueue == NULL) {
    return exitAppTask("Cannot create DAC queue.\n", NULL);
  }

  // Initialize the DAC
  if (DAC_Init(xDACQueue) != HAL_OK) {
    return exitAppTask("DAC_Init failed.\n", xDACQueue);
  }

  // Start the DAC
  if (DAC_Start() != HAL_OK) {
    return exitAppTask("DAC_Start failed.\n", xDACQueue);
  }

  // Process DAC events
  uint8_t ucEvent = 0;
  while (1) {
    if (xQueueReceive(xDACQueue, &ucEvent, portMAX_DELAY) == pdPASS) {
      if (ucEvent == EVENT_DAC_ERROR) {
        DAC_Error();
      }
    }
  }
}

/*
 * @brief  Initialize the application main task.
 *    When this function is called, buttons must not be pressed.
 *
 * @retval HAL_OK if the method succeeds.
 */
hal_status_t DAC_Task_Init() {
  if (xTaskCreate(
      vDACTaskFunction,   // Function that implements the task
      "DAC_Task",         // Text name for the task
      256,                // Stack size in words
      NULL,               // Parameter passed into the task
      10,                 // Priority
      NULL                // Used to pass out the task's handle
      ) == pdPASS) {
    return HAL_OK;
  } else {
    return HAL_ERROR;
  }
}

