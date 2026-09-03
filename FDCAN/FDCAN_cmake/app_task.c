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
#include "fdcan.h"

uint8_t pControllerData[HAL_FDCAN_DATA_LEN_CAN_FDCAN_8_BYTE];

/*
 * @brief:  Process the input event
 *
 * @param ucEvent The event that occurred
 *
 * @retval 1 if an event is handled, 0 if not handled
 */
static uint8_t processInputEvent(uint8_t ucEvent) {
  uint8_t ucInputId = ucEvent & INPUT_ID_MASK;
  uint8_t ucEventType = ucEvent & EVENT_TYPE_MASK;

  switch (ucInputId) {
  case INPUT_ID_BTN_1: {
    switch (ucEventType) {
    case EVENT_CLICK: {
      SWD_printf("--> BTN_1 CLICK\n");
      for (uint32_t i = 0; i < HAL_FDCAN_DATA_LEN_CAN_FDCAN_8_BYTE; i++) {
        pControllerData[i] = i;
      }

      FDCAN_Controller_Send(pControllerData,
          HAL_FDCAN_DATA_LEN_CAN_FDCAN_8_BYTE);
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

  return 0;
}

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 * @param xInputQueue The input queue
 * @param xQueue The FDCAN queue
 */
static void exitAppTask(char *error,
    QueueHandle_t xInputQueue,
    QueueHandle_t xFDCANQueue) {
  ErrorHandler(error);

  // Release the input handling
  Input_Free();

  // Free the input queue
  if (xInputQueue != NULL) {
    vQueueDelete(xInputQueue);
  }

  // Free the FDCAN queue
  if (xFDCANQueue != NULL) {
    vQueueDelete(xFDCANQueue);
  }

  vTaskDelete(NULL);
}

#define INPUT_QUEUE_SIZE  4
#define FDCAN_QUEUE_SIZE  8

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

  // Create the FDCAN queue
  QueueHandle_t xFDCANQueue = xQueueCreate(FDCAN_QUEUE_SIZE, sizeof(uint8_t));
  if (xFDCANQueue == NULL) {
    return exitAppTask("Cannot create FDCAN queue.\n", xInputQueue, NULL);
  }

  // Initialize the FDCAN
  if (FDCAN_Init(xFDCANQueue) != HAL_OK) {
    return exitAppTask("FDCAN_Init failed.\n", xInputQueue, xFDCANQueue);
  }

  // Prepare the queue set
  QueueSetHandle_t xQueueSet = xQueueCreateSet(
      INPUT_QUEUE_SIZE + FDCAN_QUEUE_SIZE);
  xQueueAddToSet(xInputQueue, xQueueSet);
  xQueueAddToSet(xFDCANQueue, xQueueSet);
  if (xQueueSet == NULL) {
    return exitAppTask("xQueueCreateSet failed.\n", xInputQueue, xFDCANQueue);
  }

  SWD_printf("--> Press the user button to send an FDCAN message.\n");

  uint8_t ucEvent;
  QueueSetMemberHandle_t xActivatedMember;
  while (1) {
    xActivatedMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);
    xQueueReceive(xActivatedMember, &ucEvent, 0);
    if (xActivatedMember == xFDCANQueue) {
      switch(ucEvent) {
      case EVENT_CONTROLLER_TX_COMPLETE: {
        FDCAN_Controller_Tx_Complete();
        break;
      }

      case EVENT_CONTROLLER_RX_0_COMPLETE: {
        FDCAN_Controller_Rx_Complete();
        break;
      }

      case EVENT_RESPONDER_TX_COMPLETE: {
        FDCAN_Responder_Tx_Complete();
        break;
      }

      case EVENT_RESPONDER_RX_0_COMPLETE: {
        FDCAN_Responder_Rx_Complete();
        break;
      }

      case EVENT_ERROR: {
        break;
      }

      default: {
        break;
      }
      }
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

