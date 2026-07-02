/*******************************************************************************
 * file           : dac_task.c
 * brief          : The DAC task implementation.
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/queue.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
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

#define DAC_QUEUE_SIZE      8
/*
 * @brief:  The DAC task function
 *
 * @param pvParameters Task parameters
 */
static void vDACTaskFunction(void *pvParameters) {
  QueueHandle_t xRMSToDACQueue = (QueueHandle_t)pvParameters;

  // Create the DAC queue
  QueueHandle_t xDACQueue = xQueueCreate(DAC_QUEUE_SIZE, sizeof(uint8_t));
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

  // Prepare the queue set
  QueueSetHandle_t xQueueSet = xQueueCreateSet(
      DAC_QUEUE_SIZE + FROM_ADC_QUEUE_SIZE);
  xQueueAddToSet(xDACQueue, xQueueSet);
  xQueueAddToSet(xRMSToDACQueue, xQueueSet);
  if (xQueueSet == NULL) {
    return exitAppTask("xQueueCreateSet failed.\n", xDACQueue);
  }

  // Process ADC and DAC queue values
  QueueSetMemberHandle_t xActivatedMember;
  while (1) {
    xActivatedMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);
    if (xActivatedMember == xDACQueue) {
      uint8_t ucEvent = 0;
      xQueueReceive(xActivatedMember, &ucEvent, 0);
      if (ucEvent == EVENT_DAC_ERROR) {
        DAC_Error();
      }
    } else if (xActivatedMember == xRMSToDACQueue) {
      uint16_t uwVoltage = 0;
      xQueueReceive(xActivatedMember, &uwVoltage, 0);
      DAC_Output(uwVoltage);
    }
  }
}

/*
 * @brief  Initialize the application main task.
 *    When this function is called, buttons must not be pressed.
 *
 * @param xRMSToDACQueue The parameter to pass to the task function
 *
 * @retval HAL_OK if the method succeeds.
 */
hal_status_t DAC_Task_Init(QueueHandle_t xRMSToDACQueue) {
  if (xTaskCreate(
      vDACTaskFunction,   // Function that implements the task
      "DAC_Task",         // Text name for the task
      256,                // Stack size in words
      xRMSToDACQueue,     // Parameter passed into the task
      10,                 // Priority
      NULL                // Used to pass out the task's handle
      ) == pdPASS) {
    return HAL_OK;
  } else {
    return HAL_ERROR;
  }
}

