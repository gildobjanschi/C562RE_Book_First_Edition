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

#define MODE_TICK               0
#define MODE_TICKLESS_SLEEP     1
#define MODE_TICKLESS_STOP_0    2
#define MODE_TICKLESS_STOP_1    3

// Set the mode that is in use here.
#define MODE_SEL MODE_TICK

/*
 * @brief:  Pre sleep processing
 *
 * @param ulExpectedIdleTime The low power mode expected duration
 */
void app_configPRE_SLEEP_PROCESSING (uint32_t ulExpectedIdleTime) {
#if (MODE_SEL == MODE_TICKLESS_SLEEP)
  HAL_SuspendTick();
#elif (MODE_SEL == MODE_TICKLESS_STOP_0)
  HAL_SuspendTick();

  HAL_RCC_TIM7_DisableClock();
  __disable_irq();
  SCB_EnableDeepSleep();
  LL_PWR_SetPowerMode(HAL_PWR_STOP0_MODE);
#elif (MODE_SEL == MODE_TICKLESS_STOP_1)
  HAL_SuspendTick();

  HAL_RCC_TIM7_DisableClock();
  __disable_irq();
  SCB_EnableDeepSleep();
  LL_PWR_SetPowerMode(HAL_PWR_STOP1_MODE);
#endif
}

/*
 * @brief:  Post sleep processing
 *
 * @param ulExpectedIdleTime The low power mode expected duration
 */
void app_configPOST_SLEEP_PROCESSING (uint32_t ulExpectedIdleTime) {
  __asm__ volatile ("sev": : :"memory");
#if (MODE_SEL == MODE_TICKLESS_SLEEP)
  HAL_ResumeTick();
#elif (MODE_SEL == MODE_TICKLESS_STOP_0)
  SCB_DisableDeepSleep();
  __enable_irq();
  HAL_RCC_TIM7_EnableClock();

  HAL_ResumeTick();
#elif (MODE_SEL == MODE_TICKLESS_STOP_1)
  SCB_DisableDeepSleep();
  __enable_irq();
  HAL_RCC_TIM7_EnableClock();

  HAL_ResumeTick();
#endif
}

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
      HAL_GPIO_WritePin(LD1_PORT, LD1_PIN, HAL_GPIO_PIN_SET);
      HAL_Delay(1);
      HAL_GPIO_WritePin(LD1_PORT, LD1_PIN, HAL_GPIO_PIN_RESET);
      break;
    }

    case EVENT_LONG_CLICK: {
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
}

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 * @param xInputQueue The input queue
 */
static void exitAppTask(char *error, QueueHandle_t xInputQueue) {
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

#if (MODE_SEL == MODE_TICK)
  SWD_printf("--> Operating mode: MODE_TICK\n");
#elif (MODE_SEL == MODE_TICKLESS_SLEEP)
  SWD_printf("--> Operating mode: MODE_TICKLESS_SLEEP\n");
#elif (MODE_SEL == MODE_TICKLESS_STOP_0)
  SWD_printf("--> Operating mode: MODE_TICKLESS_STOP_0\n");
#elif (MODE_SEL == MODE_TICKLESS_STOP_1)
  SWD_printf("--> Operating mode: MODE_TICKLESS_STOP_1\n");
#endif
  // Make sure that the above debug message is printed before continuing.
  vTaskDelay(pdMS_TO_TICKS(100));

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

