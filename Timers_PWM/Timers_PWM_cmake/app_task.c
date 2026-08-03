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

// Specify which example to run
#define RUN_EXAMPLE 1

#if RUN_EXAMPLE == 1
/*
 * brief:  TIM6 interrupt handler
 */
static void Tim6UpdateCallback(hal_tim_handle_t*) {
  HAL_GPIO_TogglePin(LD1_PORT, LD1_PIN);
}

/*
 * brief:  Example 1: Use TIM6 channel
 *
 * retval: HAL status code
 */
static hal_status_t Example_1() {
  SWD_printf("TIM6 example BEGIN\n");

  hal_tim_handle_t *htim6 = mx_tim6_gethandle();
  hal_status_t status;

  // Register TIM6 interrupt handle
  status = HAL_TIM_RegisterUpdateCallback(htim6, Tim6UpdateCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_RegisterUpdateCallback TIM6 failed.\n");
    return status;
  }

  // Start TIM6
  status = HAL_TIM_Start_IT(htim6);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_Start_IT TIM6 failed.\n");
    return status;
  }

  // Run the timer for about 11 milliseconds
  vTaskDelay(pdMS_TO_TICKS(11));

  // Stop the timer
  status = HAL_TIM_Stop_IT(htim6);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_Stop_IT TIM6 failed.\n");
    return status;
  }

  SWD_printf("TIM6 example END\n");

  return HAL_OK;
}
#endif // RUN_EXAMPLE == 1

#if RUN_EXAMPLE == 2
/*
 * brief:  Example 2: Use TIM5 channel 1 and 2
 *
 * retval: HAL status code
 */
static hal_status_t Example_2() {
  SWD_printf("TIM5 2xCH example BEGIN\n");

  hal_tim_handle_t *htim5 = mx_tim5_gethandle();
  hal_status_t status;

  // Start CH 1
  status = HAL_TIM_OC_StartChannel(htim5, HAL_TIM_CHANNEL_1);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_OC_StartChannel CH 1 failed.\n");
    return status;
  }

  // Start CH 2
  status = HAL_TIM_OC_StartChannel(htim5, HAL_TIM_CHANNEL_2);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_OC_StartChannel CH 2 failed.\n");
    return status;
  }

  // Start the timer
  status = HAL_TIM_Start(htim5);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_Start failed.\n");
    return status;
  }

  vTaskDelay(pdMS_TO_TICKS(10));

  // Stop CH 1
  status = HAL_TIM_OC_StopChannel(htim5, HAL_TIM_CHANNEL_1);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_OC_StopChannel CH1 failed.\n");
    return status;
  }

  // Stop CH 2
  status = HAL_TIM_OC_StopChannel(htim5, HAL_TIM_CHANNEL_2);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_OC_StopChannel CH2 failed.\n");
    return status;
  }

  // Stop the timer
  if (HAL_TIM_Stop(htim5) != HAL_OK) {
    SWD_printf("HAL_TIM_Stop failed.\n");
    return status;
  }

  SWD_printf("TIM5 2xCH example END\n");

  return HAL_OK;
}
#endif

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 */
static void exitAppTask(char *error) {
  ErrorHandler(error);

  vTaskDelete(NULL);
}

/*
 * @brief:  The application task function
 *
 * @param pvParameters Task parameters
 */
static void vAppTaskFunction(void *pvParameters) {
#if RUN_EXAMPLE == 1
  // TIM6 example: timer without channels
  if (Example_1() != HAL_OK) {
    return exitAppTask("Example_1 failed.");
  }
#elif RUN_EXAMPLE == 2
  // TIM5 example: channels 1 and 2
  if (Example_2() != HAL_OK) {
    return exitAppTask("Example_2 failed.");
  }
#endif

  SWD_printf("App_Task will exit.\n");
  vTaskDelete(NULL);
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

