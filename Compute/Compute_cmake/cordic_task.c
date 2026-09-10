/*******************************************************************************
 * file           : cordic_task.c
 * brief          : The CORDIC task implementation.
 ******************************************************************************/
#include "mx_hal_def.h"
#include <stdlib.h> // Required for function abs()
#include <arm_math.h> // Required for arm_float_to_q31() and PI definition
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/semphr.h"
#include "middleware/freertos/include/event_groups.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "cordic_task.h"

#define ARRAY_SIZE        64U

/* Input angles in radians in range [-pi, pi[ regularly incremented */
static const float32_t Angles[ARRAY_SIZE] = {
  -3.141592653589793,      -3.043417883165112,     -2.945243112740431,     -2.84706834231575,
  -2.748893571891069,      -2.650718801466388,     -2.552544031041707,     -2.454369260617026,
  -2.356194490192345,      -2.2580197197676637,    -2.1598449493429825,    -2.061670178918302,
  -1.9634954084936207,     -1.8653206380689396,    -1.7671458676442586,    -1.6689710972195777,
  -1.5707963267948966,     -1.4726215563702154,    -1.3744467859455345,    -1.2762720155208536,
  -1.1780972450961724,     -1.0799224746714913,    -0.9817477042468106,    -0.8835729338221294,
  -0.7853981633974483,     -0.6872233929727671,    -0.589048622548086,     -0.4908738521234053,
  -0.39269908169872414,    -0.294524311274043,     -0.1963495408493623,    -0.09817477042468115,
  0.0,                     0.09817477042468115,    0.1963495408493623,     0.294524311274043,
  0.39269908169872414,     0.4908738521234053,     0.589048622548086,      0.6872233929727671,
  0.7853981633974483,      0.883572933822129,      0.9817477042468106,     1.0799224746714913,
  1.178097245096172,       1.2762720155208536,     1.3744467859455343,     1.4726215563702159,
  1.5707963267948966,      1.6689710972195773,     1.7671458676442588,     1.8653206380689396,
  1.9634954084936211,      2.061670178918302,      2.1598449493429825,     2.258019719767664,
  2.356194490192345,       2.4543692606170255,     2.552544031041707,      2.650718801466388,
  2.7488935718910685,      2.84706834231575,       2.945243112740431,      3.0434178831651124
};

/* Input angles in radians divided by pi */
static float32_t AnglesDivPi[ARRAY_SIZE];

/* Q1.31 format representation of the angles divided by pi, buffer used by CPU*/
static q31_t Q1_31[ARRAY_SIZE];

/* Output array of the CORDIC calculated sines in Q1.31 format, used by CPU */
static int32_t SineValues[ARRAY_SIZE];
/*
 * @brief:  Exit the CORDIC task when there is an error
 *
 * @param error A description of the error that occurred
 */
static void exitCORDICTask(char *error) {
  ErrorHandler(error);

  vTaskDelete(NULL);
}

/*
 * @brief: Perform CORDIC sine generation
 *
 * @param params The task parameters
 *
 * @return HAL status
 */
static hal_status_t performCORDIC(CORDIC_PARAMETERS * params) {
  HAL_GPIO_WritePin(HAL_GPIOC, PC2_PIN, HAL_GPIO_PIN_SET);

  hal_cordic_handle_t *pCORDIC = mx_cordic_gethandle();
  hal_cordic_buffer_desc_t SourceBuffer = {Q1_31, ARRAY_SIZE};
  hal_cordic_buffer_desc_t DestBuffer = {SineValues, ARRAY_SIZE};

  // Start CORDIC calculations
  hal_status_t hal_status = HAL_CORDIC_Calculate(pCORDIC,
      &SourceBuffer, &DestBuffer, 1000);
  if (hal_status != HAL_OK) {
    return hal_status;
  }

  if (xSemaphoreTake(params->xPrintMutex, portMAX_DELAY) == pdPASS) {
    // Print the CORDIC sine values
    SWD_printf("CORDIC Sine values\n");
    for (uint32_t i = 0; i < ARRAY_SIZE/8; i++) {
      for (uint32_t j = 0; j < 8; j++) {
        SWD_printf("%08x ", SineValues[8*i + j]);
      }
      SWD_printf("\n");
    }
    SWD_printf("-------------------\n");

    xSemaphoreGive(params->xPrintMutex);
  }

  HAL_GPIO_WritePin(HAL_GPIOC, PC2_PIN, HAL_GPIO_PIN_RESET);

  return HAL_OK;
}

/*
 * @brief:  The CORDIC task function
 *
 * @param pvParameters Task parameters
 */
static void vCORDICTaskFunction(void *pvParameters) {
  // CORDIC input must be angles in radians divided by pi (range [-1, 1])
  for (uint32_t i = 0 ; i < ARRAY_SIZE; i++) {
    AnglesDivPi[i] = Angles[i] / PI;
  }

  // CORDIC input must be written in Q1.31 format
  arm_float_to_q31(AnglesDivPi, Q1_31, ARRAY_SIZE);

  CORDIC_PARAMETERS * params = (CORDIC_PARAMETERS *)pvParameters;
  EventBits_t uxBits;
  while (1) {
    // Clear the bit on exit. Do not wait for all bits.
    uxBits = xEventGroupWaitBits(params->xTasksEventGroup, CORDIC_EV_GROUP_BIT,
        pdTRUE, pdFALSE, portMAX_DELAY);
    if ((uxBits & CORDIC_EV_GROUP_BIT) != 0) {
      if (performCORDIC(params) != HAL_OK) {
        exitCORDICTask("performCORDIC failed\n");
        return;
      }
    }
  }
}

/*
 * @brief  Initialize the CORDIC task.
 *    When this function is called, buttons must not be pressed.
 *
 * @param params Task parameters
 *
 * @retval HAL_OK if the method succeeds.
 */
hal_status_t CORDIC_Init(CORDIC_PARAMETERS *params) {
  if (xTaskCreate(
      vCORDICTaskFunction,// Function that implements the task
      "CORDIC_Task",      // Text name for the task
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

