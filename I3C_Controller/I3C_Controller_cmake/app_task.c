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
#include "app_task.h"
#include "app_input.h"
#include "i3c_controller.h"

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
      I3C_StartDAA();
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
 * @param xI3CIntQueue The I3C interrupt queue
 */
static void exitAppTask(char *error,
    QueueHandle_t xInputQueue,
    QueueHandle_t xI3CIntQueue) {
  ErrorHandler(error);

  // Release the input handling
  Input_Free();

  // Free the input queue
  if (xInputQueue != NULL) {
    vQueueDelete(xInputQueue);
  }

  // Free the I3C interrupt queue
  if (xI3CIntQueue != NULL) {
    vQueueDelete(xI3CIntQueue);
  }

  vTaskDelete(NULL);
}

#define INPUT_QUEUE_SIZE  4
#define NOTIFY_QUEUE_SIZE 8
#define INT_QUEUE_SIZE    8

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

  // Create the I3C interrupt queue
  QueueHandle_t I3CIntQueue = xQueueCreate(INT_QUEUE_SIZE, sizeof(uint8_t));
  if (I3CIntQueue == NULL) {
    return exitAppTask("Cannot create I3C interrupt queue.\n",
        xInputQueue, NULL);
  }

  // Initialize the I3C
  if (I3C_Init(I3CIntQueue) != HAL_OK) {
    return exitAppTask("I3C_Init failed.\n", xInputQueue, I3CIntQueue);
  }

  // Prepare the queue set
  QueueSetHandle_t xQueueSet = xQueueCreateSet(
      INPUT_QUEUE_SIZE + NOTIFY_QUEUE_SIZE + INT_QUEUE_SIZE);
  xQueueAddToSet(xInputQueue, xQueueSet);
  xQueueAddToSet(I3CIntQueue, xQueueSet);
  if (xQueueSet == NULL) {
    return exitAppTask("xQueueCreateSet failed.\n", xInputQueue, I3CIntQueue);
  }

  uint8_t ucEvent;
  QueueSetMemberHandle_t xActivatedMember;
  while (1) {
    xActivatedMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);
    if (xActivatedMember == I3CIntQueue) {
      xQueueReceive(I3CIntQueue, &ucEvent, 0);
      switch (ucEvent) {
      case EVENT_DAA_COMPLETE: {
        I3C_DAAComplete();
        break;
      }

      case EVENT_TRANSFER_COMPLETE: {
        I3C_TransferComplete();
        break;
      }

      case EVENT_ERROR: {
        SWD_printf("I3C error.\n");
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Reinitialize the I3C
        if (I3C_Init(I3CIntQueue) != HAL_OK) {
          return exitAppTask("I3C_Init failed.\n", xInputQueue, I3CIntQueue);
        }

        break;
      }

      default: {
        break;
      }
      }
    } else if (xActivatedMember == xInputQueue) {
      xQueueReceive(xInputQueue, &ucEvent, 0);
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

