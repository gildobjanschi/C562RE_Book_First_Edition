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
#include "input_capture_dma.h"
#include "output_pwm_dma.h"

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 * @param xInterruptQueue The interrupt queue
 */
static void exitAppTask(char *error, QueueHandle_t xInterruptQueue) {
  ErrorHandler(error);

  // Free the interrupt queue
  if (xInterruptQueue != NULL) {
    vQueueDelete(xInterruptQueue);
  }

  vTaskDelete(NULL);
}

/*
 * @brief:  The application task function
 *
 * @param pvParameters Task parameters
 */
static void vAppTaskFunction(void *pvParameters) {
  // Create the interrupt queue. The size must be at least the size
  // of the DMA buffer so it can accommodate half of the DMA buffer when
  // an input half complete/complete interrupt occurs.
  QueueHandle_t xInterruptQueue = xQueueCreate(DMA_BUFFER_SIZE,
      sizeof(uint16_t));
  if (xInterruptQueue == NULL) {
    return exitAppTask("Cannot create input queue.\n", NULL);
  }

  // Initialize the DMA input capture
  if (Input_Capture_DMA_Init(xInterruptQueue) != HAL_OK) {
    return exitAppTask("Input_Capture_DMA_Init failed.\n", xInterruptQueue);
  }

  // Initialize the PWM DMA output
  if (Output_PWM_DMA_Init() != HAL_OK) {
    return exitAppTask("Output_PWM_DMA_Init failed.\n", xInterruptQueue);
  }

  if (Start_Input_Capture_DMA() != HAL_OK) {
    return exitAppTask("Start_Input_Capture_DMA failed.\n", xInterruptQueue);
  }

  SWD_printf("Generating PMW pulses on PA0.\n");
  if (Start_PWM_DMA() != HAL_OK) {
    return exitAppTask("Start_PWM_DMA failed.\n", xInterruptQueue);
  }

  uint16_t uwEvent;
  while (1) {
    if (xQueueReceive(xInterruptQueue, &uwEvent, portMAX_DELAY) == pdPASS) {
      if (uwEvent == EVENT_ERROR) {
        SWD_printf("Error!\n");
        break;
      } else {
        SWD_printf("IC: %d.\n", uwEvent);
      }
    }
  }

  if (Stop_PWM_DMA() != HAL_OK) {
    return exitAppTask("Stop_PWM_DMA failed.\n", xInterruptQueue);
  }

  if (Stop_Input_Capture_DMA() != HAL_OK) {
    return exitAppTask("Stop_Input_Capture_DMA failed.\n", xInterruptQueue);
  }

  exitAppTask("Exiting App_Task...\n", xInterruptQueue);
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

