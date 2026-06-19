/*******************************************************************************
 * file           : adc_task.c
 * brief          : The ADC task implementation.
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
#include "adc_task.h"
#include "adc.h"

#define ADC_DATA_SIZE 120
// The size of the buffer must be at least the size of the DMA buffer.
// To capture once period worth of samples value 120 is used.
static uint16_t ADC_Data[ADC_DATA_SIZE];

// Set to 1 to run the SINGLE_DMA_CAPTURE voltage reference measurement
// Set to 2 to run the SINGLE_DMA_CAPTURE temperature measurement
// Set to 3 to run the CONTINUOUS_DMA_CAPTURE CH9 input voltage measurement
#define EXAMPLE_SEL 3

#if (EXAMPLE_SEL == 1)
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
      // Start the Vref measurement
      ADC_Start(HAL_ADC_CHANNEL_VREFINT, SINGLE_DMA_CAPTURE);
      break;
    }

    case EVENT_LONG_CLICK: {
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
 * @brief:  Process an ADC event
 *
 * @param ucEvent The event that occurred
 */
static void processADCEvent(uint8_t ucEvent) {
  uint32_t ulDataLength;
  switch (ucEvent) {
  case EVENT_ADC_CPLT_DATA: {
    ADC_Complete(ucEvent, ADC_Data, ADC_DATA_SIZE, &ulDataLength);
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
#elif (EXAMPLE_SEL == 2)
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
      // Start the temperature measurement
      ADC_Start(HAL_ADC_CHANNEL_TEMPSENSOR, SINGLE_DMA_CAPTURE);
      break;
    }

    case EVENT_LONG_CLICK: {
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
 * @brief:  Process an ADC event
 *
 * @param ucEvent The event that occurred
 */
static void processADCEvent(uint8_t ucEvent) {
  uint32_t ulDataLength;
  switch (ucEvent) {
  case EVENT_ADC_CPLT_DATA: {
    ADC_Complete(ucEvent, ADC_Data, ADC_DATA_SIZE, &ulDataLength);
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

#elif (EXAMPLE_SEL == 3)
static uint32_t ulADCIndex;

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
      // Clear the capture buffer
      ulADCIndex = 0;

      // Capture data until the ADC_Data buffer is full.
      ADC_Start(HAL_ADC_CHANNEL_9, CONTINUOUS_DMA_CAPTURE);
      break;
    }

    case EVENT_LONG_CLICK: {
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
 * @brief:  Process an ADC event
 *
 * @param ucEvent The event that occurred
 */
static void processADCEvent(uint8_t ucEvent) {
  uint32_t ulDataLength;
  switch (ucEvent) {
  case EVENT_ADC_HALF_DATA:
  case EVENT_ADC_CPLT_DATA: {
    if (ulADCIndex < ADC_DATA_SIZE) {
      ADC_Complete(ucEvent, ADC_Data + ulADCIndex, ADC_DATA_SIZE - ulADCIndex,
          &ulDataLength);
      // Accumulate the ADC data
      ulADCIndex += ulDataLength;
    } else {
      // The ADC_Data buffer is full. Stop the conversion.
      ADC_Stop();

      // Print the values of the samples separated by commas
      // to make it easy to save it into a CSV file.
      for (uint32_t i = 0; i < ADC_DATA_SIZE; i++) {
        SWD_printf("%d,", ADC_Data[i]);
      }
      SWD_printf("\n");
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

#endif

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 * @param xInputQueue The input queue
 * @param xADCQueue The ADC queue
 */
static void exitAppTask(char *error, QueueHandle_t xInputQueue,
    QueueHandle_t xADCQueue) {
  ErrorHandler(error);

  // Release the input handling
  Input_Free();

  // Free the input queue
  if (xInputQueue != NULL) {
    vQueueDelete(xInputQueue);
  }

  // Free the ADC queue
  if (xADCQueue != NULL) {
    vQueueDelete(xADCQueue);
  }

  vTaskDelete(NULL);
}

#define INPUT_QUEUE_SIZE  4
#define ADC_QUEUE_SIZE    8

/*
 * @brief:  The ADC task function
 *
 * @param pvParameters Task parameters
 */
static void vADCTaskFunction(void *pvParameters) {
  // Create the input queue
  QueueHandle_t xInputQueue = xQueueCreate(INPUT_QUEUE_SIZE, sizeof(uint8_t));
  if (xInputQueue == NULL) {
    return exitAppTask("Cannot create input queue.\n", NULL, NULL);
  }

  // Initialize the input
  if (App_Input_Init(xInputQueue) != HAL_OK) {
    return exitAppTask("App_Input_Init failed.\n", xInputQueue, NULL);
  }

  // Create the ADC queue
  QueueHandle_t xADCQueue = xQueueCreate(ADC_QUEUE_SIZE, sizeof(uint8_t));
  if (xADCQueue == NULL) {
    return exitAppTask("Cannot create ADC queue.\n", xInputQueue, NULL);
  }

  // Initialize the ADC
  if (ADC_Init(xADCQueue) != HAL_OK) {
    return exitAppTask("ADC_Init failed.\n", xInputQueue, xADCQueue);
  }

  // Prepare the queue set
  QueueSetHandle_t xQueueSet = xQueueCreateSet(
      INPUT_QUEUE_SIZE + ADC_QUEUE_SIZE);
  xQueueAddToSet(xInputQueue, xQueueSet);
  xQueueAddToSet(xADCQueue, xQueueSet);
  if (xQueueSet == NULL) {
    return exitAppTask("xQueueCreateSet failed.\n", xInputQueue, xADCQueue);
  }

#if (EXAMPLE_SEL == 1)
  SWD_printf("--> Press the user button to display the analog voltage "
      "reference value.\n");
#elif (EXAMPLE_SEL == 2)
  SWD_printf("--> Press the user button to display the internal MCU "
      "temperature.\n");
#elif (EXAMPLE_SEL == 3)
  SWD_printf("--> Press the user button to capture the analog DAC output "
      "as ADC values.\n");
#endif
  // Process input and ADC events
  uint8_t ucEvent = 0;
  QueueSetMemberHandle_t xActivatedMember;
  while (1) {
    xActivatedMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);
    xQueueReceive(xActivatedMember, &ucEvent, 0);
    if (xActivatedMember == xADCQueue) {
      processADCEvent(ucEvent);
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
hal_status_t ADC_Task_Init() {
  if (xTaskCreate(
      vADCTaskFunction,   // Function that implements the task
      "ADC_Task",         // Text name for the task
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

