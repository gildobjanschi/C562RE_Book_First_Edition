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

// Comment out the define below to wake up the MCU with the user button.
#define RTC_WAKEUP 1

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
      SWD_printf("--> BTN_1 LONG CLICK\n");
      // Read the value in the tamper backup register 0.
      uint32_t value = HAL_TAMP_ReadBackupRegisterValue(HAL_TAMP_BACKUP_REG_0);

      SWD_printf("Backup value in register 0: %ld\n", value);
      if (value >= 10) {
        value = 0;
      } else {
        value++;
      }

      // Save the value in the tamper backup register 0.
      hal_status_t status = HAL_TAMP_WriteBackupRegisterValue(
          HAL_TAMP_BACKUP_REG_0, value);
      if (status != HAL_OK) {
        SWD_printf("HAL_TAMP_WriteBackupRegisterValue failed\n");
        return;
      }

      // Enter Standby mode
      SWD_printf("--> Going to standby in 2 seconds...\n");
#ifdef RTC_WAKEUP
      SWD_printf("--> The system will wake up in 5 seconds.\n");
#else
      SWD_printf("--> After the LED is off "
          "press user button to wake up the MCU.\n");
#endif
      // Flash the LED fast for 2 seconds
      // Turn the LED OFF
      HAL_GPIO_WritePin(LD1_PORT, LD1_PIN, HAL_GPIO_PIN_RESET);
      for (uint8_t i = 0; i < 20; i++) {
        // Turn the LED ON
        HAL_GPIO_TogglePin(LD1_PORT, LD1_PIN);
        vTaskDelay(50);

        // Turn the LED OFF
        HAL_GPIO_TogglePin(LD1_PORT, LD1_PIN);
        vTaskDelay(50);
      }

#ifdef RTC_WAKEUP
      HAL_RTC_WAKEUP_Start(HAL_RTC_WAKEUP_IT_ENABLE);
#endif
      HAL_PWR_LP_CleanWakeupSource(HAL_PWR_WAKEUP_SOURCE_4);
      HAL_PWR_CleanPreviousSystemPowerMode();
      HAL_GPIO_WritePin(LD1_PORT, LD1_PIN, HAL_GPIO_PIN_RESET);

      LL_PWR_ClearFlag_SB();

      HAL_SuspendTick();
      HAL_RCC_TIM7_DisableClock();
      HAL_PWR_EnterStandbyMode();
      SWD_printf("-- This code will not be reached --\n");
      break;
    }

    case EVENT_LONG_CLICK: {
      //SWD_printf("--> BTN_1 LONG CLICK\n");
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
 */
static void exitAppTask(char *error,
    QueueHandle_t xInputQueue) {
  ErrorHandler(error);

  // Release the input handling
  Input_Free();

  // Free the input queue
  if (xInputQueue != NULL) {
    vQueueDelete(xInputQueue);
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
  QueueHandle_t xInputQueue = xQueueCreate(16, sizeof(uint8_t));
  if (xInputQueue == NULL) {
    return exitAppTask("Cannot create input queue.\n", NULL);
  }

  // Initialize the input
  if (App_Input_Init(xInputQueue) != HAL_OK) {
    return exitAppTask("App_Input_Init failed.\n", xInputQueue);
  }

  SWD_printf("--> Press user button to enter standby mode.\n");

  // Turn on the LED
  HAL_GPIO_WritePin(LD1_PORT, LD1_PIN, HAL_GPIO_PIN_SET);

  // Process input events
  uint8_t ucEvent = 0;
  while (1) {
    if (xQueueReceive(xInputQueue, &ucEvent, portMAX_DELAY) == pdPASS) {
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

