/*******************************************************************************
 * file           : app_task.c
 * brief          : The application task implementation.
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
#include "app_task.h"
#include "dac.h"

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
      DAC_Start();
      SWD_printf("--> BTN_1 CLICK\n");
      break;
    }

    case EVENT_LONG_CLICK: {
      SWD_printf("--> BTN_1 LONG CLICK\n");
      DAC_Stop();
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
 * @brief:  Process an DAC event
 *
 * @param ucEvent The event that occurred
 */
static void processDACEvent(uint8_t ucEvent) {
  switch (ucEvent) {
  case EVENT_DAC_CPLT_DATA:
  case EVENT_DAC_HALF_CPLT_DATA: {
    DAC_Complete(ucEvent);
    break;
  }

  case EVENT_DAC_ERROR: {
    DAC_Error();
    break;
  }

  default: {
    SWD_printf("Unhandled event: %d\n", ucEvent);
    break;
  }
  }
}

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 * @param xInputQueue The input queue
 * @param xDACQueue The DAC queue
 */
static void exitAppTask(char *error, QueueHandle_t xInputQueue,
    QueueHandle_t xDACQueue) {
  ErrorHandler(error);

  // Release the input handling
  Input_Free();

  // Free the input queue
  if (xInputQueue != NULL) {
    vQueueDelete(xInputQueue);
  }

  // Free the DAC queue
  if (xDACQueue != NULL) {
    vQueueDelete(xDACQueue);
  }

  vTaskDelete(NULL);
}

#define INPUT_QUEUE_SIZE  4
#define DAC_QUEUE_SIZE    16

/*
 * @brief:  The application task function
 *
 * @param pvParameters Task parameters
 */
static void vAppTaskFunction(void *pvParameters) {
  // Create the input queue
  QueueHandle_t xInputQueue = xQueueCreate(INPUT_QUEUE_SIZE, sizeof(uint8_t));
  if (xInputQueue == NULL) {
    return exitAppTask("Cannot create input queue.\n", NULL, NULL);
  }

  // Initialize the input
  if (App_Input_Init(xInputQueue) != HAL_OK) {
    return exitAppTask("App_Input_Init failed.\n", xInputQueue, NULL);
  }

  // Create the DAC queue
  QueueHandle_t xDACQueue = xQueueCreate(DAC_QUEUE_SIZE, sizeof(uint8_t));
  if (xDACQueue == NULL) {
    return exitAppTask("Cannot create DAC queue.\n", xInputQueue, NULL);
  }

  // Initialize the DAC
  if (DAC_Init(xDACQueue) != HAL_OK) {
    return exitAppTask("DAC_Init failed.\n", xInputQueue, xDACQueue);
  }

  // Prepare the queue set
  QueueSetHandle_t xQueueSet = xQueueCreateSet(
      INPUT_QUEUE_SIZE + DAC_QUEUE_SIZE);
  xQueueAddToSet(xInputQueue, xQueueSet);
  xQueueAddToSet(xDACQueue, xQueueSet);
  if (xQueueSet == NULL) {
    return exitAppTask("xQueueCreateSet failed.\n", xInputQueue, xDACQueue);
  }

  SWD_printf("--> Click the user button to start the wave generation "
      "and long click to stop it\n");

  // Process input and DAC events
  uint8_t ucEvent = 0;
  QueueSetMemberHandle_t xActivatedMember;
  while (1) {
    xActivatedMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);
    xQueueReceive(xActivatedMember, &ucEvent, 0);
    if (xActivatedMember == xDACQueue) {
      processDACEvent(ucEvent);
    } else if (xActivatedMember == xInputQueue) {
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

