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
#include "i2c1_dma.h"

// The FRAM write buffer
static uint8_t* pWrBuffer;

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
      if (I2C1_Write(128, pWrBuffer, I2C1_GetWriteBufferSize()) == HAL_OK) {
        return 1;
      }

      break;
    }

    case EVENT_LONG_CLICK: {
      SWD_printf("--> BTN_1 LONG CLICK\n");
      if (I2C1_Read(128, I2C1_GetReadBufferSize()) == HAL_OK) {
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
 * @brief:  Process I2C events
 *
 * @param xI2cQueue The I2C queue
 */
void processI2cEvents(QueueHandle_t xI2cQueue) {
  uint8_t ucExitLoop = 0;
  uint8_t ucEvent;

  while (1) {
    if (xQueueReceive(xI2cQueue, &ucEvent, portMAX_DELAY) == pdPASS) {
      switch (ucEvent) {
      case EVENT_TX_COMPLETE: {
        // If I2C1_TxComplete returns HAL_ADDR_SENT we stay in the loop
        hal_status_t status = I2C1_TxComplete();
        if (status == HAL_OK) {
          SWD_printf("EVENT_TX_COMPLETE: OK.\n");
          ucExitLoop = 1;
        } else if (status == HAL_ADDR_SENT) {
          // Continue the loop. An address was sent for reading.
        } else {
          SWD_printf("EVENT_TX_COMPLETE: Error\n");
          ucExitLoop = 1;
        }

        break;
      }

      case EVENT_RX_COMPLETE: {
        uint8_t* pRdBuffer;
        uint32_t ulReadBytes;
        I2C1_RxComplete(&pRdBuffer, &ulReadBytes);
        if (ulReadBytes == I2C1_GetReadBufferSize()) {
          // Compare the read buffer to the write buffer.
          // memcmp is not being used in order to get the index where the
          // buffers differ.
          uint32_t ulErrorIndex = 0xffffffff;
          for (uint32_t i = 0; i < ulReadBytes; i++) {
            if (pRdBuffer[i] != pWrBuffer[i]) {
              ulErrorIndex = i;
              break;
            }
          }

          if (ulErrorIndex == 0xffffffff) {
            SWD_printf("EVENT_RX_COMPLETE: Read OK.\n");
          } else {
            SWD_printf("EVENT_RX_COMPLETE: error at index: %ld. W: %d, R: %d\n",
                ulErrorIndex, pWrBuffer[ulErrorIndex], pRdBuffer[ulErrorIndex]);
          }
        } else {
          SWD_printf("EVENT_RX_COMPLETE: bad length: %ld\n", ulReadBytes);
        }

        ucExitLoop = 1;
        break;
      }

      case EVENT_ERROR: {
        I2C1_Error();
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
 * @param xI2cQueue The I2C queue
 * @param pWrBuffer The write buffer
 */
static void exitAppTask(char *error,
    QueueHandle_t xInputQueue,
    QueueHandle_t xI2cQueue,
    uint8_t* pWrBuffer) {
  ErrorHandler(error);

  // Release the input handling
  Input_Free();

  // Free the input queue
  if (xInputQueue != NULL) {
    vQueueDelete(xInputQueue);
  }

  // Free the I2C queue
  if (xI2cQueue != NULL) {
    vQueueDelete(xI2cQueue);
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
    return exitAppTask("Cannot create input queue.\n", NULL, NULL, NULL);
  }

  // Initialize the input
  if (App_Input_Init(xInputQueue) != HAL_OK) {
    return exitAppTask("App_Input_Init failed.\n", xInputQueue, NULL, NULL);
  }

  // Create the I2C queue
  QueueHandle_t xI2cQueue = xQueueCreate(8, sizeof(uint8_t));
  if (xI2cQueue == NULL) {
    return exitAppTask("Cannot create I2C queue.\n", xInputQueue, NULL, NULL);
  }

  // Allocate the write buffer
  uint32_t ulBufferSize = I2C1_GetWriteBufferSize();
  if ((pWrBuffer = malloc(ulBufferSize)) == NULL) {
    return exitAppTask("Cannot allocate write buffer.\n", xInputQueue,
        xI2cQueue, NULL);
  }

  // Initialize the write buffer with data
  for (uint32_t i = 0; i < ulBufferSize; i++) {
    pWrBuffer[i] = ((uint8_t)i);
  }

  // 0xa0 is the I2C shifted address of the FRAM (the unshifted address is 0x50)
  if (I2C1_Init(xI2cQueue, 0xa0) != HAL_OK) {
    return exitAppTask("I2C1_Init failed.\n", xInputQueue, xI2cQueue,
        pWrBuffer);
  }

  SWD_printf("--> Press the user button to write data to FRAM. "
      "Long press the user button to read FRAM data and compare "
      "with written data.\n");

  // Process input events
  uint8_t ucEvent;
  while (1) {
    if (xQueueReceive(xInputQueue, &ucEvent, portMAX_DELAY) == pdPASS) {
      if (processInputEvent(ucEvent) == 1) {
        processI2cEvents(xI2cQueue);
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

