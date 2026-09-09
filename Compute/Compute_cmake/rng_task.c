/*******************************************************************************
 * file           : rng_task.c
 * brief          : The RNG task implementation.
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/semphr.h"
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
 * @brief:  The RNG task function
 *
 * @param pvParameters Task parameters
 */
static void vRNGTaskFunction(void *pvParameters) {
  uint32_t RandomNumbers[RNG_NUMBERS] = {0};

  hal_rng_handle_t * pRNG = mx_rng_gethandle();
  // Generate the random numbers using a 10 milliseconds timeout
  if (HAL_RNG_GenerateRandomNumber(pRNG, RandomNumbers, RNG_NUMBERS, 10)
      != HAL_OK) {
    if (HAL_RNG_GetLastErrorCodes(pRNG) != HAL_RNG_ERROR_SEED) {
      exitRNGTask("Last error != HAL_RNG_ERROR_SEED\n");
      return;
    }

    if (HAL_RNG_RecoverSeedError(pRNG) != HAL_OK) {
      exitRNGTask("HAL_RNG_RecoverSeedError failed.\n");
      return;
    }
  }

  RNG_PARAMETERS * params = (RNG_PARAMETERS *)pvParameters;
  if (xSemaphoreTake(params->xPrintMutex, portMAX_DELAY) == pdPASS) {
    // Critical section — exclusive access
    // Print the random numbers
    for (uint32_t i = 0; i < RNG_NUMBERS; i++) {
      SWD_printf("Random number: %08x\n", RandomNumbers[i]);
    }

    // Release the mutex when done
    xSemaphoreGive(params->xPrintMutex);
  }

  // Exit the task
  vTaskDelete(NULL);
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

