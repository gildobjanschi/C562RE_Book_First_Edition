/*******************************************************************************
 * file           : adc_task.c
 * brief          : The ADC task implementation.
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/queue.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "adc_task.h"
#include "adc.h"

static uint16_t ADC_Data[ADC_DMA_BUFFER_SIZE];
static uint32_t ulSamplesSum, ulSamplesCount;

/*
 * @brief:  Process an ADC event
 *
 * @param ucEvent The event that occurred
 * @param xFromADCQueue Queue to send messages to the DAC task
 */
static void processADCEvent(uint8_t ucEvent, QueueHandle_t xFromADCQueue) {
  uint32_t ulDataLength;
  switch (ucEvent) {
  case EVENT_ADC_HALF_DATA: {
  case EVENT_ADC_CPLT_DATA:
    ADC_Complete(ucEvent, ADC_Data, ADC_DMA_BUFFER_SIZE, &ulDataLength);

    // Compute the sum of the acquired samples
    for (uint32_t i = 0; i < ulDataLength; i++) {
      if (ADC_Data[i] >= 1650) {
        ulSamplesSum += (ADC_Data[i] - 1650);
      } else {
        ulSamplesSum += (1650 - ADC_Data[i]);
      }
    }

    ulSamplesCount += ulDataLength;
    // The device acquires samples 30,000 times per second.
    // The output is updated 20 times per second (every 1500 samples)
    if (ulSamplesCount >= 1500) {
      // We need to multiply by a factor to ensure full scale deflection
      // when a sinewave with an amplitude of 3.3V is present at the ADC input.
      uint16_t uwVoltage = (3.2 * ulSamplesSum) / ulSamplesCount;
      xQueueSend(xFromADCQueue, &uwVoltage, 0);

      ulSamplesCount = 0;
      ulSamplesSum = 0;
    }
    break;
  }

  case EVENT_ADC_ERROR: {
    ADC_Error();
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
 * @param xADCQueue The ADC queue
 */
static void exitAppTask(char *error, QueueHandle_t xADCQueue) {
  ErrorHandler(error);

  // Free the ADC queue
  if (xADCQueue != NULL) {
    vQueueDelete(xADCQueue);
  }

  vTaskDelete(NULL);
}

#define ADC_QUEUE_SIZE    8

/*
 * @brief:  The ADC task function
 *
 * @param pvParameters Task parameters
 */
static void vADCTaskFunction(void *pvParameters) {
  QueueHandle_t xFromADCQueue = (QueueHandle_t)pvParameters;

  // Create the ADC queue
  QueueHandle_t xADCQueue = xQueueCreate(ADC_QUEUE_SIZE, sizeof(uint8_t));
  if (xADCQueue == NULL) {
    return exitAppTask("Cannot create ADC queue.\n", NULL);
  }

  // Initialize the ADC
  if (ADC_Init(xADCQueue) != HAL_OK) {
    return exitAppTask("ADC_Init failed.\n", xADCQueue);
  }

  // Start the ADC
  ulSamplesSum = 0;
  ulSamplesCount = 0;
  if (ADC_Start(HAL_ADC_CHANNEL_9) != HAL_OK) {
    return exitAppTask("xQueueCreateSet failed.\n", xADCQueue);
  }

  // Process ADC events
  uint8_t ucEvent = 0;
  while (1) {
    xQueueReceive(xADCQueue, &ucEvent, portMAX_DELAY);
    processADCEvent(ucEvent, xFromADCQueue);
  }
}

/*
 * @brief  Initialize the application main task.
 *    When this function is called, buttons must not be pressed.
 *
 * @param xFromADCQueue The parameter to pass to the task function
 *
 * @retval HAL_OK if the method succeeds.
 */
hal_status_t ADC_Task_Init(QueueHandle_t xFromADCQueue) {
  if (xTaskCreate(
      vADCTaskFunction,   // Function that implements the task
      "ADC_Task",         // Text name for the task
      256,                // Stack size in words
      xFromADCQueue,      // Parameter passed into the task
      10,                 // Priority
      NULL                // Used to pass out the task's handle
      ) == pdPASS) {
    return HAL_OK;
  } else {
    return HAL_ERROR;
  }
}

