/*******************************************************************************
 * file           : rng_task.c
 * brief          : The RNG task implementation.
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/semphr.h"
#include "middleware/freertos/include/event_groups.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "rng_task.h"

/*
 * @brief:  Exit the RNG task when there is an error
 *
 * @param error A description of the error that occurred
 */
static void exitRNGTask(char *error) {
  ErrorHandler(error);

  vTaskDelete(NULL);
}

// How many random numbers to generate
#define RNG_NUMBERS 8

/*
 * @brief: Perform Random Numbers Generation
 *
 * @param params The task parameters
 *
 * @return HAL status
 */
static hal_status_t performRNG(RNG_PARAMETERS * params) {
  HAL_GPIO_WritePin(HAL_GPIOC, PC0_PIN, HAL_GPIO_PIN_SET);
  uint32_t RandomNumbers[RNG_NUMBERS] = {0};

  hal_rng_handle_t * pRNG = mx_rng_gethandle();
  // Generate the random numbers using a 10 milliseconds timeout.
  hal_status_t hal_status = HAL_RNG_GenerateRandomNumber(pRNG, RandomNumbers,
      RNG_NUMBERS, 10);
  if (hal_status != HAL_OK) {
    if (HAL_RNG_GetLastErrorCodes(pRNG) != HAL_RNG_ERROR_SEED) {
      HAL_GPIO_WritePin(HAL_GPIOC, PC0_PIN, HAL_GPIO_PIN_RESET);
      SWD_printf("Last error != HAL_RNG_ERROR_SEED\n");
      return hal_status;
    }

    if (HAL_RNG_RecoverSeedError(pRNG) != HAL_OK) {
      HAL_GPIO_WritePin(HAL_GPIOC, PC0_PIN, HAL_GPIO_PIN_RESET);
      SWD_printf("HAL_RNG_RecoverSeedError failed\n");
      return hal_status;
    }
  }

  // Print the random numbers
  if (xSemaphoreTake(params->xPrintMutex, portMAX_DELAY) == pdPASS) {
    SWD_printf("Random numbers\n");
    for (uint32_t i = 0; i < RNG_NUMBERS; i++) {
      SWD_printf("%08x ", RandomNumbers[i]);
    }
    SWD_printf("\n-------------------\n");

    xSemaphoreGive(params->xPrintMutex);
  }

  HAL_GPIO_WritePin(HAL_GPIOC, PC0_PIN, HAL_GPIO_PIN_RESET);

  return HAL_OK;
}

/*
 * @brief:  The RNG task function
 *
 * @param pvParameters Task parameters
 */
static void vRNGTaskFunction(void *pvParameters) {
  RNG_PARAMETERS * params = (RNG_PARAMETERS *)pvParameters;

  EventBits_t uxBits;
  while (1) {
    // Clear the bit on exit. Do not wait for all bits.
    uxBits = xEventGroupWaitBits(params->xTasksEventGroup, RNG_EV_GROUP_BIT,
        pdTRUE, pdFALSE, portMAX_DELAY);

    if ((uxBits & RNG_EV_GROUP_BIT) != 0) {
      if (performRNG(params) != HAL_OK) {
        exitRNGTask("performRNG failed\n");
        return;
      }
    }
  }
}

/*
 * @brief  Initialize the RNG task.
 *    When this function is called, buttons must not be pressed.
 *
 * @param params Task parameters
 *
 * @retval HAL_OK if the method succeeds.
 */
hal_status_t RNG_Init(RNG_PARAMETERS *params) {
  if (xTaskCreate(
      vRNGTaskFunction,   // Function that implements the task
      "RNG_Task",         // Text name for the task
      256,                // Stack size in words
      params,             // Parameter passed into the task
      10,                 // Priority
      NULL                // Used to pass out the task's handle
      ) == pdPASS) {
    return HAL_OK;
  } else {
    return HAL_ERROR;
  }
}

