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
#include "i2c1.h"
#include "vl53l1x.h"

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
  case INPUT_ID_BTN_1 : {
    switch (ucEventType) {
    case EVENT_CLICK : {
      SWD_printf("--> BTN_1 CLICK\n");
      VL53L1X_Start();
      return 1;
    }

    case EVENT_LONG_CLICK : {
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
 * @brief:  Process I2C events
 *
 * @param xI2cQueue The I2C queue
 */
void processI2cEvents(QueueHandle_t xI2cQueue) {
  uint8_t ucExitI2cLoop = 0;
  uint8_t ucEvent;

  while (1) {
    if (xQueueReceive(xI2cQueue, &ucEvent, portMAX_DELAY) == pdPASS) {
      switch (ucEvent) {
      case EVENT_TX_COMPLETE: {
        if (I2C1_TxComplete() != HAL_OK) {
          ucExitI2cLoop = 1;
        }

        break;
      }

      case EVENT_RX_COMPLETE: {
        uint16_t uwDistance;
        hal_status_t status = I2C1_RxComplete(&uwDistance);
        if (status == HAL_OK) {
          // Continue
        } else if (status == HAL_DISTANCE_AVAIL) {
          SWD_printf("Distance: %umm\n", uwDistance);
          ucExitI2cLoop = 1;
        } else { // Error
          ucExitI2cLoop = 1;
        }

        break;
      }

      case EVENT_ERROR: {
        I2C1_Error();
        ucExitI2cLoop = 1;
        break;
      }
      }

      if (ucExitI2cLoop == 1) {
        break;
      }
    }
  }
}

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 * @param xInputQueue The input queue
 * @param xI2cQueue The I2C queue
 */
static void exitAppTask(char *error,
    QueueHandle_t xInputQueue,
    QueueHandle_t xI2cQueue) {
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
    return exitAppTask("Cannot create input queue.\n", NULL, NULL);
  }

  // Initialize the input
  if (App_Input_Init(xInputQueue) != HAL_OK) {
    return exitAppTask("App_Input_Init failed.\n", xInputQueue, NULL);
  }

  // Create the I2C queue
  QueueHandle_t xI2cQueue = xQueueCreate(8, sizeof(uint8_t));
  if (xI2cQueue == NULL) {
    return exitAppTask("Cannot create I2C queue.\n", xInputQueue, NULL);
  }

  // Initialize the sensor
  if (VL53L1X_Init(xI2cQueue) != HAL_OK) {
    return exitAppTask("VL53L1X_Init failed.\n", xInputQueue, xI2cQueue);
  }

  SWD_printf("--> Place an object in front of the sensor and press the button "
      "to do a distance measurement.\n");

  // Process input and I2C events
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
  if (xTaskCreate(vAppTaskFunction,   // Function that implements the task
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

