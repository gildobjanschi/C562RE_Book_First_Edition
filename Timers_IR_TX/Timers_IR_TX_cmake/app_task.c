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
#include "../../Shared/Input/input.h"
#include "app_input.h"
#include "app_task.h"
#include "ir_tx.h"

#define MAIN_ADDRESS    ((uint8_t)0xa)

/*
 * @brief:  Process the input event
 *
 * @param ucEvent The event that occurred
 */
static void processInputEvent(uint8_t ucEvent) {
  uint8_t ucInputId = ucEvent & INPUT_ID_MASK;
  uint8_t ucEventType = ucEvent & EVENT_TYPE_MASK;

  switch (ucInputId) {
  case INPUT_ID_BTN_1: {
    switch (ucEventType) {
    case EVENT_CLICK: {
      SWD_printf("--> BTN_1 CLICK\n");
      IrTxFrame(MAIN_ADDRESS, 0xa);
      break;
    }

    case EVENT_LONG_CLICK: {
      SWD_printf("--> BTN_1 LONG CLICK\n");
      break;
    }

    default: {
      break;
    }
    }
    break;
  }

  default: {
    SWD_printf("Unhandled id: %d, event: %d\n", ucInputId, ucEventType);
    break;
  }
  }
}

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 * @param xInputQueue The input queue
 */
static void exitAppTask(char *error, QueueHandle_t xInputQueue) {
  ErrorHandler(error);

  Input_Free();

  // Free the input queue
  if (xInputQueue != NULL) {
    vQueueDelete(xInputQueue);
  }

  vTaskDelete(NULL);
}

/*
 * @brief:  The application task function
 *
 * @param pvParameters Task parameters
 */
static void vAppTaskFunction(void *pvParameters) {
  // Create the input queue
  QueueHandle_t xInputQueue = xQueueCreate(16, sizeof(uint8_t));
  if (xInputQueue == NULL) {
    return exitAppTask("Cannot create input queue.\n", NULL);
  }

  // Initialize the input
  if (App_Input_Init(xInputQueue) != HAL_OK) {
    return exitAppTask("App_Input_Init failed.\n", xInputQueue);
  }

  // Initialize the IR Tx
  if (IrTx_Init() != HAL_OK) {
    return exitAppTask("IrTx_Init failed.", xInputQueue);
  }

  // Process input events
  uint8_t ucEvent = 0;
  while (1) {
    if (xQueueReceive(xInputQueue, &ucEvent, portMAX_DELAY) == pdPASS) {
      processInputEvent(ucEvent);
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

