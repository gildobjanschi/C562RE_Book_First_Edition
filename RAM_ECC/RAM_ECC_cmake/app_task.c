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

#define SRAM_BASE 0x20010000
#define SRAM_SINGLE_ERR_ADDR_BASE SRAM_BASE
#define SRAM_DOUBLE_ERR_ADDR_BASE (SRAM_BASE + 0x200U)
#define SRAM_SIZE (0x200U / 0x04U)

// The number of single and double errors recorded
uint32_t ulSingleErrors = 0U;
uint32_t ulDoubleErrors = 0U;

/*
 * @brief: The RAM ECC error callback
 *
 * @param hramcfg The RAM config handler
 */
hal_status_t HAL_RAMCFG_ECC_ErrorCallback(hal_ramcfg_t hramcfg) {
  hal_ramcfg_ecc_info_t p_info;

  HAL_RAMCFG_ECC_GetInfo(hramcfg, &p_info);
  if (p_info.type == HAL_RAMCFG_ECC_SINGLE) {
    ulSingleErrors++;
  } else {
    ulDoubleErrors++;
  }
  return HAL_OK;
}

/*
 * @brief: The RAM ECC function
 */
static void RAM_ECC_Test() {
  // Erase SRAM2
  if (HAL_RAMCFG_MassErase(HAL_RAMCFG_SRAM2, 1000UL) != HAL_OK) {
    SWD_printf("HAL_RAMCFG_MassErase failed.\n");
    return;
  }

  // Enable ECC for SRAM2
  if (HAL_RAMCFG_ECC_Enable(HAL_RAMCFG_SRAM2) != HAL_OK) {
    SWD_printf("HAL_RAMCFG_ECC_Enable failed.\n");
    return;
  }

  // Write data to SRAM2
  for (uint32_t i = 0; i < SRAM_SIZE << 1; i++) {
    ((uint32_t *)SRAM_SINGLE_ERR_ADDR_BASE)[i] = 0xAA55AA55UL;
  }

  SWD_printf("Data write successful.\n");

  // Disable ECC for SRAM2
  if (HAL_RAMCFG_ECC_Disable(HAL_RAMCFG_SRAM2) != HAL_OK) {
    SWD_printf("HAL_RAMCFG_ECC_Disable failed.\n");
    return;
  }

  // Write single error data to SRAM2
  for (uint32_t i = 0; i < SRAM_SIZE; i++) {
    ((uint32_t *)SRAM_SINGLE_ERR_ADDR_BASE)[i] = 0xAA55AA54UL;
  }

  // Write double error data to SRAM2
  for (uint32_t i = 0; i < SRAM_SIZE; i++) {
    ((uint32_t *)SRAM_DOUBLE_ERR_ADDR_BASE)[i] = 0xAA55AA50UL;
  }
  SWD_printf("Error data write successful.\n");

  ulSingleErrors = 0;
  ulDoubleErrors = 0;

  // Enable interrupts for ECC errors
  if (HAL_RAMCFG_ECC_Enable_IT(HAL_RAMCFG_SRAM2,
      (HAL_RAMCFG_IT_ECC_SINGLE | HAL_RAMCFG_IT_ECC_DOUBLE)) != HAL_OK) {
    SWD_printf("HAL_RAMCFG_ECC_Enable_IT failed.\n");
    return;
  }

  // Read single error data and compare with the expected value
  for (uint32_t i = 0; i < SRAM_SIZE; i++) {
    if (0xAA55AA55UL != ((uint32_t *)SRAM_SINGLE_ERR_ADDR_BASE)[i]) {
      SWD_printf("Single RAM ECC mismatch @%ld | expected %lx | actual %lx.\n",
          i, 0xAA55AA55UL, ((uint32_t *)SRAM_SINGLE_ERR_ADDR_BASE)[i]);
      return;
    }
  }

  SWD_printf("Corrected read single error data successful.\n");

  // Read double error data and compare with the expected value
  for (uint32_t i = 0; i < SRAM_SIZE; i++) {
    if (0xAA55AA50UL != ((uint32_t *)SRAM_DOUBLE_ERR_ADDR_BASE)[i]) {
      SWD_printf("Double RAM ECC mismatch @%ld | expected %lx | actual %lx.\n",
          i, 0xAA55AA50UL, ((uint32_t *)SRAM_DOUBLE_ERR_ADDR_BASE)[i]);
      return;
    }
  }

  SWD_printf("Not corrected read double error data successful.\n");

  // Check the number of corrected single errors
  if (ulSingleErrors != SRAM_SIZE) {
    SWD_printf("Unexpected single errors: %ld.\n", ulSingleErrors);
    return;
  }

  // Check the number of not corrected double errors
  if (ulDoubleErrors != SRAM_SIZE) {
    SWD_printf("Unexpected double errors: %ld.\n", ulDoubleErrors);
    return;
  }

  SWD_printf("Test successful.\n");
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

  SWD_printf("--> Press the user button to start the RAM ECC test.\n");

  uint8_t ucEvent;
  while (1) {
    if (xQueueReceive(xInputQueue, &ucEvent, portMAX_DELAY) == pdPASS) {
      SWD_printf("--> BTN_1 CLICK\n");
      RAM_ECC_Test();
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

