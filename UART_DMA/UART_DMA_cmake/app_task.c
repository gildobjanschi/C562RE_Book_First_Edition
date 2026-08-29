/*******************************************************************************
 * file           : app_task.c
 * brief          : The application task implementation.
 ******************************************************************************/
#include <string.h>
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/queue.h"
#include "middleware/freertos/include/stream_buffer.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "../../Shared/Input/input.h"
#include "app_input.h"
#include "app_task.h"
#include "usart1_dma.h"

#define RX_BUFFER_SIZE 32
uint8_t RxBuffer[RX_BUFFER_SIZE];

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
      char* message =
          "Clicked button abcdefghijklmnop!"
          "Clicked button abcdefghijklmnop!"
          "Clicked button!\n";
      USART1_Send((uint8_t*)message, strlen(message));
      break;
    }

    case EVENT_LONG_CLICK: {
      SWD_printf("--> BTN_1 LONG CLICK\n");
      char* message = "Long clicked button!\r\n";
      USART1_Send((uint8_t*)message, strlen(message));
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
 * @param xRxUARTStreamBuffer The Rx stream buffer
 * @param xTxUARTStreamBuffer The Tx stream buffer
 * @param xIntUARTQueue The interrupt queue
 */
static void exitAppTask(char *error,
    QueueHandle_t xInputQueue, StreamBufferHandle_t xRxUARTStreamBuffer,
    StreamBufferHandle_t xTxUARTStreamBuffer, QueueHandle_t xIntUARTQueue) {
  ErrorHandler(error);

  // Release the input handling
  Input_Free();

  // Free the input queue
  if (xInputQueue != NULL) {
    vQueueDelete(xInputQueue);
  }

  // Free the Rx stream buffer
  if (xRxUARTStreamBuffer != NULL) {
    vStreamBufferDelete(xRxUARTStreamBuffer);
  }

  // Free the Tx stream buffer
  if (xTxUARTStreamBuffer != NULL) {
    vStreamBufferDelete(xTxUARTStreamBuffer);
  }

  // Free the interrupt queue
  if (xIntUARTQueue != NULL) {
    vQueueDelete(xIntUARTQueue);
  }

  vTaskDelete(NULL);
}

// Define the size of the queues
#define INPUT_QUEUE_SIZE  8
#define INT_QUEUE_SIZE    8

/*
 * @brief:  The application task function
 *
 * @param pvParameters Task parameters
 */
static void vAppTaskFunction(void *pvParameters) {
  // Create the input queue
  QueueHandle_t xInputQueue = xQueueCreate(8, sizeof(uint8_t));
  if (xInputQueue == NULL) {
    return exitAppTask("Cannot create input queue.\n", NULL, NULL, NULL, NULL);
  }

  // Initialize the input
  if (App_Input_Init(xInputQueue) != HAL_OK) {
    return exitAppTask("App_Input_Init failed.\n",
        xInputQueue, NULL, NULL, NULL);
  }

  // Create the UART Rx stream buffer
  StreamBufferHandle_t xRxUARTStreamBuffer =
      xStreamBufferCreate(4 * RX_DMA_BUFFER_SIZE, 1);
  if (xRxUARTStreamBuffer == NULL) {
    return exitAppTask("Cannot create UART Rx stream buffer.\n",
        xInputQueue, NULL, NULL, NULL);
  }

  // Create the UART Tx stream buffer
  StreamBufferHandle_t xTxUARTStreamBuffer =
      xStreamBufferCreate(4 * TX_DMA_BUFFER_SIZE, 1);
  if (xTxUARTStreamBuffer == NULL) {
    return exitAppTask("Cannot create UART Tx stream buffer.\n",
        xInputQueue, xRxUARTStreamBuffer, NULL, NULL);
  }

  // Create the UART interrupt queue
  QueueHandle_t xIntUARTQueue = xQueueCreate(INT_QUEUE_SIZE, sizeof(uint8_t));
  if (xIntUARTQueue == NULL) {
    return exitAppTask("Cannot create UART interrupt queue.\n",
        xInputQueue, xRxUARTStreamBuffer, xTxUARTStreamBuffer, NULL);
  }

  // Initialize the UART
  if (USART1_Init(xRxUARTStreamBuffer, xTxUARTStreamBuffer, xIntUARTQueue)
      != HAL_OK) {
    return exitAppTask("USART1_Init failed.\n",
        xInputQueue, xRxUARTStreamBuffer, xTxUARTStreamBuffer, xIntUARTQueue);
  }

  if (USART1_Rx() != HAL_OK) {
    return exitAppTask("USART1_Rx failed.\n",
        xInputQueue, xRxUARTStreamBuffer, xTxUARTStreamBuffer, xIntUARTQueue);
  }

  // Prepare the queue set
  QueueSetHandle_t xQueueSet = xQueueCreateSet(
      INPUT_QUEUE_SIZE + INT_QUEUE_SIZE);
  xQueueAddToSet(xInputQueue, xQueueSet);
  xQueueAddToSet(xIntUARTQueue, xQueueSet);
  if (xQueueSet == NULL) {
    return exitAppTask("xQueueCreateSet failed.\n",
        xInputQueue, xRxUARTStreamBuffer, xTxUARTStreamBuffer, xIntUARTQueue);
  }

  uint8_t ucEvent;
  QueueSetMemberHandle_t xActivatedMember;
  while (1) {
    xActivatedMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);
    xQueueReceive(xActivatedMember, &ucEvent, 0);
    if (xActivatedMember == xIntUARTQueue) {
      switch (ucEvent) {
      case EVENT_USART1_RX_COMPLETE: {
        HAL_GPIO_WritePin(HAL_GPIOC, PC0_PIN, HAL_GPIO_PIN_SET);
        if (USART1_Rx() != HAL_OK) {
          SWD_printf("USART1_Rx error!");
        }

        // Consume all the bytes in the Rx stream buffer
        uint32_t ulRxBytes;
        while ((ulRxBytes = xStreamBufferReceive(xRxUARTStreamBuffer,
            RxBuffer, RX_BUFFER_SIZE, 0)) > 0) {
          // Null terminate the string
          RxBuffer[ulRxBytes] = 0;
          SWD_printf("%s", RxBuffer);
        }

        HAL_GPIO_WritePin(HAL_GPIOC, PC0_PIN, HAL_GPIO_PIN_RESET);
        break;
      }

      case EVENT_USART1_TX_COMPLETE: {
        if (USART1_Tx() != HAL_OK) {
          SWD_printf("USART1_Tx error!");
        }
        //vTaskDelay(1);

        break;
      }

      case EVENT_USART1_ERROR: {
        USART1_Error();
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

