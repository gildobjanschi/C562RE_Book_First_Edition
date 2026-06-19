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
#include "spi2_dma.h"

// The FRAM write buffer
static uint8_t *pWrBuffer;

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
      if (SPI2_Write(0, pWrBuffer, SPI2_GetBufferSize()) == HAL_OK) {
        return 1;
      }

      break;
    }

    case EVENT_LONG_CLICK: {
      SWD_printf("--> BTN_1 LONG CLICK\n");
      if (SPI2_Read(0, SPI2_GetBufferSize()) == HAL_OK) {
        return 1;
      }

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
 * @brief:  Process SPI events
 *
 * @param xSPIQueue The SPI queue
 */
static void processSPIEvents(QueueHandle_t xSPIQueue) {
  uint8_t ucExitLoop = 0;
  uint8_t ucEvent;

  while (1) {
    if (xQueueReceive(xSPIQueue, &ucEvent, portMAX_DELAY) == pdPASS) {
      switch (ucEvent) {
      case EVENT_TX_COMPLETE: {
        hal_status_t status = SPI2_TxComplete();
        if (status == HAL_WRITE_COMPLETE) {
          SWD_printf("EVENT_TX_COMPLETE: Write completed\n");
          ucExitLoop = 1;
        } else if (status == HAL_OK) {
          // Continue. The state machine has not completed.
        } else {
          // An error had occurred
          ucExitLoop = 1;
        }

        break;
      }

      case EVENT_RX_COMPLETE: {
        uint8_t *pRxBuffer;
        uint32_t ulReadBytes;
        SPI2_RxComplete(&pRxBuffer, &ulReadBytes);
        if (ulReadBytes == SPI2_GetBufferSize()) {
          // Compare the read buffer to the write buffer.
          // memcmp is not being used in order to get the index where the
          // buffers differ.
          uint32_t ulErrorIndex = 0xffffffff;
          for (uint32_t i = 0; i < ulReadBytes; i++) {
            if (pRxBuffer[i] != pWrBuffer[i]) {
              ulErrorIndex = i;
              break;
            }
          }

          if (ulErrorIndex == 0xffffffff) {
            SWD_printf("EVENT_RX_COMPLETE: Read OK\n");
          } else {
            SWD_printf("EVENT_RX_COMPLETE: error at index: %ld\n",
                ulErrorIndex);
          }
        } else {
          SWD_printf("EVENT_RX_COMPLETE: bad length: %ld\n", ulReadBytes);
        }

        ucExitLoop = 1;
        break;
      }

      case EVENT_ERROR: {
        SPI2_Error();
        ucExitLoop = 1;
        break;
      }
      }
    }

    if (ucExitLoop == 1) {
      break;
    }
  }
}

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 * @param xInputQueue The input queue
 * @param xSPIQueue The SPI queue
 * @param pWrBuffer The pointer to the write buffer
 */
static void exitAppTask(char *error, QueueHandle_t xInputQueue,
    QueueHandle_t xSPIQueue, uint8_t *pWrBuffer) {
  ErrorHandler(error);

  // Release the input handling
  Input_Free();

  // Free the input queue
  if (xInputQueue != NULL) {
    vQueueDelete(xInputQueue);
  }

  // Free the input queue
  if (xSPIQueue != NULL) {
    vQueueDelete(xSPIQueue);
  }

  // Free the write buffer
  if (pWrBuffer != NULL) {
    free(pWrBuffer);
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
  QueueHandle_t xInputQueue = xQueueCreate(8, sizeof(uint8_t));
  if (xInputQueue == NULL) {
    return exitAppTask("Cannot create input queue.\n", xInputQueue, NULL, NULL);
  }

  // Initialize the input
  if (App_Input_Init(xInputQueue) != HAL_OK) {
    return exitAppTask("App_Input_Init failed.\n", xInputQueue, NULL, NULL);
  }

  // Create the SPI queue
  QueueHandle_t xSPIQueue = xQueueCreate(8, sizeof(uint8_t));
  if (xSPIQueue == NULL) {
    return exitAppTask("Cannot create SPI queue.\n", xInputQueue, NULL, NULL);
  }

  // Allocate the write buffer
  uint32_t ulBufferSize = SPI2_GetBufferSize();
  if ((pWrBuffer = malloc(ulBufferSize)) == NULL) {
    return exitAppTask("Cannot allocate write buffer.\n", xInputQueue,
        xSPIQueue, NULL);
  }

  // Initialize the write buffer with data
  for (uint32_t i = 0; i < ulBufferSize; i++) {
    pWrBuffer[i] = ((uint8_t) i);
  }

  // Initialize SPI
  if (SPI2_Init(xSPIQueue) != HAL_OK) {
    return exitAppTask("SPI2_Init failed.\n", xInputQueue, xSPIQueue, pWrBuffer);
  }

  SWD_printf("--> Press the user button to write data to FRAM. "
      "Long press the user button to read FRAM data and compare "
      "with written data.\n");

  // Process input events
  uint8_t ucEvent;
  while (1) {
    if (xQueueReceive(xInputQueue, &ucEvent, portMAX_DELAY) == pdPASS) {
      if (processInputEvent(ucEvent) == 1) {
        processSPIEvents(xSPIQueue);
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

