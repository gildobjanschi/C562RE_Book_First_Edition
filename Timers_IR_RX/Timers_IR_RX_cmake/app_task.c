/*******************************************************************************
 * file           : app_task.c
 * brief          : The application task implementation.
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/queue.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "app_task.h"
#include "ir_rx.h"

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 * @param xIrRxQueue The IR queue
 */
static void exitAppTask(char *error, QueueHandle_t xIrRxQueue) {
  ErrorHandler(error);

  // Free the interrupt queue
  if (xIrRxQueue != NULL) {
    vQueueDelete(xIrRxQueue);
  }

  vTaskDelete(NULL);
}

/*
 * @brief:  The application task function
 *
 * @param pvParameters Task parameters
 */
static void vAppTaskFunction(void *pvParameters) {
  // Create the IR receive queue
  QueueHandle_t xIrRxQueue = xQueueCreate(16, sizeof(uint8_t));
  if (xIrRxQueue == NULL) {
    return exitAppTask("Cannot create input queue.\n", NULL);
  }

  // Initialize the IR Rx
  if (IrRx_Init(xIrRxQueue) != HAL_OK) {
    return exitAppTask("IrRx_Init failed.\n", xIrRxQueue);
  }

  // Process IR Rx events
  uint8_t ucEvent, ucAddress, ucCommand;
  while (1) {
    if (xQueueReceive(xIrRxQueue, &ucEvent, portMAX_DELAY) == pdPASS) {
      if (IrRx_Decode(ucEvent, &ucAddress, &ucCommand) == 1) {
        SWD_printf(">> IR ADDR: %x, CMD: %x\n", ucAddress, ucCommand);
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
hal_status_t App_Init() {
  if (xTaskCreate(
      vAppTaskFunction,   // Function that implements the task
      "App_Task",         // Text name for the task
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

