/*******************************************************************************
 * file           : app_input.c
 * brief          : Input related code.
 ******************************************************************************/
#include <stdint.h>
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/queue.h"
#include "middleware/freertos/include/timers.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Input/input.h"
#include "app_input.h"

static input_t hBtn1;

/*
 * @brief:  Initialize the input. When this function is called,
 *  buttons must not be pressed.
 *
 * @param inputQueue The pointer to the input queue
 *
 * @retval: HAL_OK if the method succeeds, HAL_ERROR if it fails.
 */
hal_status_t App_Input_Init(QueueHandle_t inputQueue) {
  hal_status_t status;
  if ((status = Input_Init(inputQueue)) != HAL_OK) {
    return status;
  }

  hBtn1.ucId = INPUT_ID_BTN_1;
  hBtn1.ucSubId = INPUT_SUB_ID_INVALID;
  hBtn1.port = B1_PORT; // Use B1_PORT for Nucleo user button
  hBtn1.ulPin = B1_PIN; // Use B1_PIN for Nucleo user button
  hBtn1.ulInputLevel = HAL_GPIO_ReadPin(hBtn1.port, hBtn1.ulPin);
  hBtn1.ulGesture = GESTURE_BUTTON;
  hBtn1.ulActiveLevel = HAL_GPIO_PIN_SET;
  hBtn1.ulActiveTimestamp = 0;
  hBtn1.associatedInput = NULL;
  hBtn1.hTim = mx_tim12_gethandle();
  hBtn1.hExti = mx_gpio_default_exti13_gethandle();

  if ((status = Input_Add(&hBtn1)) != HAL_OK) {
    return status;
  }

  return HAL_OK;
}
