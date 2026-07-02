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

// The maximum number of bytes received is half of the size of the DMA buffer.
static uint16_t ADC_Data[ADC_DMA_BUFFER_SIZE/2];
static uint32_t ulSquareSum, ulSamplesCount, ulMinSample, ulMaxSample;

// Peak voltage is the 0dB Vpp input voltage devided by 2
#define PEAK_VOLTAGE (900/2)
/*
 * @brief:  Process an ADC event
 *
 * @param ucEvent The event that occurred
 * @param xRMSToDACQueue Queue to send messages to the DAC task
 */
static void processADCEvent(uint8_t ucEvent, QueueHandle_t xRMSToDACQueue) {
  uint32_t ulDataLength;
  switch (ucEvent) {
  case EVENT_ADC_HALF_DATA: {
  case EVENT_ADC_CPLT_DATA:
    ADC_Complete(ucEvent, ADC_Data, ADC_DMA_BUFFER_SIZE/2, &ulDataLength);

    // Compute the sum of the squares of the acquired samples
    int32_t lDiff;
    for (uint32_t i = 0; i < ulDataLength; i++) {
      if (ADC_Data[i] < ulMinSample) {
        ulMinSample = ADC_Data[i];
      }
      if (ADC_Data[i] > ulMaxSample) {
        ulMaxSample = ADC_Data[i];
      }

      lDiff = ADC_Data[i] - (VDD_VALUE/2);
      ulSquareSum += lDiff * lDiff;
    }

    ulSamplesCount += ulDataLength;
    // The device acquires samples 60,000 times per second.
    // The output is updated 30 times per second (every 2000 samples)
    if (ulSamplesCount >= 2000) {
      // Turn the Peak LED on/off
      if (ulMinSample < (VDD_VALUE/2) - PEAK_VOLTAGE ||
          ulMaxSample > (VDD_VALUE/2) + PEAK_VOLTAGE) {
        HAL_GPIO_WritePin(LD_PEAK_PORT, LD_PEAK_PIN, HAL_GPIO_PIN_SET);
      } else {
        HAL_GPIO_WritePin(LD_PEAK_PORT, LD_PEAK_PIN, HAL_GPIO_PIN_RESET);
      }
      uint16_t uwVoltage = sqrtf(ulSquareSum/ulSamplesCount);
      xQueueSend(xRMSToDACQueue, &uwVoltage, 0);

      ulSamplesCount = 0;
      ulSquareSum = 0;
      ulMinSample = 1650;
      ulMaxSample = 1650;
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
  QueueHandle_t xRMSToDACQueue = (QueueHandle_t)pvParameters;

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
  ulSquareSum = 0;
  ulSamplesCount = 0;
  ulMinSample = 1650;
  ulMaxSample = 1650;

  if (ADC_Start(HAL_ADC_CHANNEL_9) != HAL_OK) {
    return exitAppTask("xQueueCreateSet failed.\n", xADCQueue);
  }

  // Process ADC events
  uint8_t ucEvent = 0;
  while (1) {
    xQueueReceive(xADCQueue, &ucEvent, portMAX_DELAY);
    processADCEvent(ucEvent, xRMSToDACQueue);
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
hal_status_t ADC_Task_Init(QueueHandle_t xRMSToDACQueue) {
  if (xTaskCreate(
      vADCTaskFunction,   // Function that implements the task
      "ADC_Task",         // Text name for the task
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

