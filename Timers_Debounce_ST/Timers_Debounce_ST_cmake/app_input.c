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
#include "../../Shared/Input/input_soft_timers.h"
#include "app_input.h"

static input_t hBtn1;
static input_t hReBtn;
static input_t hReA;
static input_t hReB;

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
  hBtn1.hTim = NULL;
  hBtn1.hExti = mx_gpio_default_exti13_gethandle();

  if ((status = Input_Add(&hBtn1)) != HAL_OK) {
    return status;
  }

  hReBtn.ucId = INPUT_ID_RE_BTN;
  hReBtn.ucSubId = INPUT_SUB_ID_INVALID;
  hReBtn.port = RE_BTN_PORT;
  hReBtn.ulPin = RE_BTN_PIN;
  hReBtn.ulInputLevel = HAL_GPIO_ReadPin(hReBtn.port, hReBtn.ulPin);
  hReBtn.ulGesture = GESTURE_BUTTON;
  hReBtn.ulActiveLevel = HAL_GPIO_PIN_SET;
  hReBtn.ulActiveTimestamp = 0;
  hReBtn.associatedInput = NULL;
  hReBtn.hTim = NULL;
  hReBtn.hExti = mx_gpio_default_exti7_gethandle();

  if ((status = Input_Add(&hReBtn)) != HAL_OK) {
    return status;
  }

  hReA.ucId = INPUT_ID_RE;
  hReA.ucSubId = INPUT_SUB_ID_RE_A;
  hReA.port = RE_A_PORT;
  hReA.ulPin = RE_A_PIN;
  hReA.ulInputLevel = HAL_GPIO_ReadPin(hReA.port, hReA.ulPin);
  hReA.ulGesture = GESTURE_RE;
  hReA.ulActiveLevel = HAL_GPIO_PIN_RESET;
  hReA.ulActiveTimestamp = 0;
  hReA.associatedInput = &hReB;
  hReA.hTim = NULL;
  hReA.hExti = mx_gpio_default_exti11_gethandle();

  if ((status = Input_Add(&hReA)) != HAL_OK) {
    return status;
  }

  hReB.ucId = INPUT_ID_RE;
  hReB.ucSubId = INPUT_SUB_ID_RE_B;
  hReB.port = RE_B_PORT;
  hReB.ulPin = RE_B_PIN;
  hReB.ulInputLevel = HAL_GPIO_ReadPin(hReB.port, hReB.ulPin);
  hReB.ulGesture = GESTURE_RE;
  hReB.ulActiveLevel = HAL_GPIO_PIN_RESET;
  hReB.ulActiveTimestamp = 0;
  hReB.associatedInput = &hReA;
  hReB.hTim = NULL;
  hReB.hExti = mx_gpio_default_exti2_gethandle();

  if ((status = Input_Add(&hReB)) != HAL_OK) {
    return status;
  }

  return HAL_OK;
}
