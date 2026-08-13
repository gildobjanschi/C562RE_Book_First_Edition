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
#include "i3c_target.h"

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 * @param xI3CNotifyQueue The I3C notify queue
 * @param xI3CIntQueue The I3C interrupt queue
 */
static void exitAppTask(char *error, QueueHandle_t xI3CNotifyQueue,
    QueueHandle_t xI3CIntQueue) {
  ErrorHandler(error);

  // Free the I3C notify queue
  if (xI3CNotifyQueue != NULL) {
    vQueueDelete(xI3CNotifyQueue);
  }

  // Free the I3C interrupt queue
  if (xI3CIntQueue != NULL) {
    vQueueDelete(xI3CIntQueue);
  }

  vTaskDelete(NULL);
}

#define NOTIFY_QUEUE_SIZE 8
#define INT_QUEUE_SIZE    8

/*
 * @brief:  The application task function
 *
 * @param pvParameters Task parameters
 */
static void vAppTaskFunction(void *pvParameters) {
  // Create the I3C notify queue
  QueueHandle_t I3CNotifyQueue = xQueueCreate(NOTIFY_QUEUE_SIZE,
      sizeof(uint32_t));
  if (I3CNotifyQueue == NULL) {
    return exitAppTask("Cannot create I3C notify queue.\n", NULL, NULL);
  }

  // Create the I3C interrupt queue
  QueueHandle_t I3CIntQueue = xQueueCreate(INT_QUEUE_SIZE, sizeof(uint8_t));
  if (I3CIntQueue == NULL) {
    return exitAppTask("Cannot create I3C interrupt queue.\n", I3CNotifyQueue,
        NULL);
  }

  // Initialize the I3C
  if (I3C_Init(I3CNotifyQueue, I3CIntQueue) != HAL_OK) {
    return exitAppTask("I3C_Init failed.\n", I3CNotifyQueue, I3CIntQueue);
  }

  // Prepare the queue set
  QueueSetHandle_t xQueueSet = xQueueCreateSet(
      NOTIFY_QUEUE_SIZE + INT_QUEUE_SIZE);
  xQueueAddToSet(I3CNotifyQueue, xQueueSet);
  xQueueAddToSet(I3CIntQueue, xQueueSet);
  if (xQueueSet == NULL) {
    return exitAppTask("xQueueCreateSet failed.\n", I3CNotifyQueue,
        I3CIntQueue);
  }

  uint8_t ucEvent;
  uint32_t ulEvent;
  QueueSetMemberHandle_t xActivatedMember;
  while (1) {
    xActivatedMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);
    if (xActivatedMember == I3CIntQueue) {
      xQueueReceive(I3CIntQueue, &ucEvent, 0);
      switch (ucEvent) {
      case EVENT_ERROR: {
        HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, HAL_GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, HAL_GPIO_PIN_SET);
        vTaskDelay(pdMS_TO_TICKS(1000));
        HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, HAL_GPIO_PIN_RESET);

        // Reinitialize the I3C
        if (I3C_Init(I3CNotifyQueue, I3CIntQueue) != HAL_OK) {
          return exitAppTask("I3C_Init failed.\n", I3CNotifyQueue, I3CIntQueue);
        }

        break;
      }

      case EVENT_RX_COMPLETE: {
        I3C_RxComplete();
        break;
      }

      case EVENT_TX_COMPLETE: {
        I3C_TxComplete();
        break;
      }

      default: {
        break;
      }
      }
    } else if (xActivatedMember == I3CNotifyQueue) {
      xQueueReceive(I3CNotifyQueue, &ulEvent, 0);
      if (I3C_Notify(ulEvent) != HAL_OK) {
        HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, HAL_GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, HAL_GPIO_PIN_SET);
        vTaskDelay(pdMS_TO_TICKS(1000));
        HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, HAL_GPIO_PIN_RESET);

        // Reinitialize the I3C
        if (I3C_Init(I3CNotifyQueue, I3CIntQueue) != HAL_OK) {
          return exitAppTask("I3C_Init failed.\n", I3CNotifyQueue, I3CIntQueue);
        }
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

