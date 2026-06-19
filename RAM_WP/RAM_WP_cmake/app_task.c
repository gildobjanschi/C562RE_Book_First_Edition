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

/*
 * @brief:  The first write protect memory test
 */
static void WP_Test_1() {
  // Erase SRAM2
  if (HAL_RAMCFG_MassErase(HAL_RAMCFG_SRAM2, 1000UL) != HAL_OK) {
    SWD_printf("HAL_RAMCFG_MassErase failed.\n");
    return;
  }

  // Enable SRAM2 write protection for 1 page starting at page 0
  if (HAL_RAMCFG_EnablePageWRP(HAL_RAMCFG_SRAM2, 0U, 1) != HAL_OK) {
    SWD_printf("HAL_RAMCFG_EnablePageWRP failed.\n");
    return;
  }

  SWD_printf("Attempting to write to read-only memory. "
      "Expect a Bus fault.\n");
  vTaskDelay(100);

  uint32_t *SRAM_BASE = (uint32_t *)0x20010000;
  // Attempt to write to protected memory
  SRAM_BASE[0] = 0x10121416;

  // The code should not get here if the SRAM region is protected
  SWD_printf("--> You must reset the MCU to disable write protection.\n");
}

/*
 * @brief:  The second write protect memory test.
 *    The region 0x20018000 to 0x2001801f is protected.
 *    See STM32CubeMX2 MPU Region 1 configuration.
 */
static void WP_Test_2() {
  // Disable write protection that is enabled through STM32CubeMX2 configuration.
  HAL_CORTEX_MPU_DisableRegion(HAL_CORTEX_MPU_REGION_1);

  SWD_printf("Attempting to write to unprotected memory...\n");
  vTaskDelay(100);

  uint32_t *SRAM_BASE = (uint32_t *)0x20018000;
  // Attempt to write to protected memory
  SRAM_BASE[0] = 0x10121418;

  SWD_printf("Success.\n");

  // Protect a region of the SRAM2
  hal_cortex_mpu_region_config_t p_region_config = {0};

  p_region_config.base_addr = 0x20018000;
  p_region_config.limit_addr = 0x2001801F;
  p_region_config.access_attr = HAL_CORTEX_MPU_REGION_ALL_RO;
  p_region_config.exec_attr = HAL_CORTEX_MPU_EXECUTION_ATTR_DISABLE;
  p_region_config.attr_idx = HAL_CORTEX_MPU_MEM_ATTR_0;
  HAL_CORTEX_MPU_SetConfigRegion(HAL_CORTEX_MPU_REGION_1, &p_region_config);

  HAL_CORTEX_MPU_EnableRegion(HAL_CORTEX_MPU_REGION_1);

  SWD_printf("Attempting to write to protected memory. "
      "Expect a MemManage fault.\n");
  vTaskDelay(100);

  // Attempt to write to protected memory
  SRAM_BASE[0] = 0x10121418;

  // The code should not get here if the SRAM region is protected
  SWD_printf("Success.\n");
}

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
      WP_Test_1();
      break;
    }

    case EVENT_LONG_CLICK: {
      SWD_printf("--> BTN_1 LONG CLICK\n");
      WP_Test_2();
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

#define INPUT_QUEUE_SIZE  8

/*
 * @brief:  The task function
 *
 * @param pvParameters Task parameters
 */
static void vTaskFunction(void *pvParameters) {
  // Create the input queue
  QueueHandle_t xInputQueue = xQueueCreate(INPUT_QUEUE_SIZE, sizeof(uint8_t));
  if (xInputQueue == NULL) {
    return exitAppTask("Cannot create input queue.\n", xInputQueue);
  }

  // Initialize the input
  if (App_Input_Init(xInputQueue) != HAL_OK) {
    return exitAppTask("App_Input_Init failed.\n", xInputQueue);
  }

  SWD_printf("--> Press the user button to start the SRAM2 "
      "write protection test.\n");

  uint8_t ucEvent;
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
      vTaskFunction,  // Function that implements the task
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

