/*******************************************************************************
 * file           : input_soft_timers.c
 * brief          : Input related code.
 ******************************************************************************/
#include <stdio.h>
#include "../shared_def.h"
#include "../Debug/swd_printf.h"
#include "input_soft_timers.h"

// The minimum duration of a long click in ms.
#define LONG_CLICK_TIME 1000

static void vTimerExpiredCallback(xTimerHandle pxTimer);
static void ExtiInputTriggerCallback(hal_exti_handle_t *hexti,
    hal_exti_trigger_t trigger);

// Store pointers to inputs
#define MAX_INPUTS 8
static input_t *pInputs[MAX_INPUTS];
static uint32_t ulInputCount;

// The queue used to inform the owning task of input events
static volatile QueueHandle_t sInputQueue;

/*
 * @brief:  Initialize the input. When this function is called,
 * 	buttons must not be pressed.
 *
 * @param inputQueue The pointer to the input queue
 *
 * @retval: HAL_OK if the method succeeds, HAL_ERROR if it fails.
 */
hal_status_t Input_Init(QueueHandle_t inputQueue) {
  sInputQueue = inputQueue;
  ulInputCount = 0;
  return HAL_OK;
}

/*
 * @brief:  Add an input
 *
 * @retval: HAL_OK if the method succeeds, HAL_ERROR if it fails.
 */
hal_status_t Input_Add(input_t *pInput) {
  if (ulInputCount >= MAX_INPUTS) {
    return HAL_ERROR;
  }

  // Build the timer name
  char szTimerName[8];
  sprintf(szTimerName, "tim_%ld", ulInputCount);

  // Create a 5ms one shot timer for this input
  pInput->hTim = xTimerCreate(szTimerName, pdMS_TO_TICKS(5), pdFALSE,
        (void*)NULL, vTimerExpiredCallback);
  if (pInput->hTim == NULL) {
    return HAL_ERROR;
  }

  HAL_EXTI_Disable(pInput->hExti);
  hal_status_t status = HAL_EXTI_RegisterTriggerCallback(pInput->hExti,
      ExtiInputTriggerCallback);
  if (status != HAL_OK) {
    return status;
  }
  HAL_EXTI_Enable(pInput->hExti, HAL_EXTI_MODE_INTERRUPT);

  // Store the input structure pointer
  pInputs[ulInputCount] = pInput;
  ulInputCount++;

  return HAL_OK;
}

/*
 * @brief:  Release resources
 */
void Input_Free() {
  for (uint32_t i = 0; i < ulInputCount; i++) {
    HAL_EXTI_Disable(pInputs[i]->hExti);
  }

  ulInputCount = 0;
}

/*
 * @brief:  Report input event
 *
 * @param ucEvent The event that occurred
 */
void reportInputEvent(uint8_t ucEvent) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sInputQueue, &ucEvent,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * @brief  EXTI line trigger edge default callback.
 *
 * @param hexti Pointer to EXTI handle.
 * @param trigger This parameter can be one of the values of
 *  @ref hal_exti_trigger_t Trigger value.
 */
void ExtiInputTriggerCallback(hal_exti_handle_t *hexti,
    hal_exti_trigger_t trigger) {
  for (uint32_t i = 0; i < ulInputCount; i++) {
    if (pInputs[i]->hExti == hexti) {
      TimerHandle_t hTim = pInputs[i]->hTim;
      if (xTimerIsTimerActive(hTim)) {
        xTimerStop(hTim, 0);
      }

      xTimerStart(hTim, 0);

      // Break from the for loop
      break;
    }
  }
}

/*
 * @brief  FreeRTOS timer expired callback
 *
 * @param hTim The timer handle
 */
static void vTimerExpiredCallback(xTimerHandle htim) {
  for (uint32_t i = 0; i < ulInputCount; i++) {
    if (pInputs[i]->hTim == htim) {
      input_t *input = pInputs[i];
      uint32_t ulPinState = HAL_GPIO_ReadPin(input->port, input->ulPin);

      input->ulInputLevel = ulPinState;
      if (ulPinState == HAL_GPIO_PIN_SET) {
        SWD_printf("TIM: id: %x HIGH [%lu]\n", input->ucId, HAL_GetTick());
      } else {
        SWD_printf("TIM: id: %x LOW [%lu]\n", input->ucId, HAL_GetTick());
      }

      switch (input->ulGesture) {
      case GESTURE_NONE: {
        reportInputEvent(input->ucId | (ulPinState == HAL_GPIO_PIN_SET ?
            EVENT_LEVEL_HIGH : EVENT_LEVEL_LOW));
        break;
      }

      case GESTURE_BUTTON: {
        if (input->ulInputLevel == input->ulActiveLevel) {
          input->ulActiveTimestamp = HAL_GetTick();
        } else {
          if (HAL_GetTick() - input->ulActiveTimestamp > LONG_CLICK_TIME) {
            reportInputEvent(input->ucId | EVENT_LONG_CLICK);
          } else {
            reportInputEvent(input->ucId | EVENT_CLICK);
          }
        }

        break;
      }

      case GESTURE_RE: {
        if (input->ucSubId == INPUT_SUB_ID_RE_A) {
          if (input->associatedInput->ulInputLevel == HAL_GPIO_PIN_SET) {
            // A = (C), B = H
            reportInputEvent(input->ucId |
                (ulPinState == HAL_GPIO_PIN_SET ? EVENT_RE_CCW : EVENT_RE_CW));
          } else {
            // A = (C), B = L
            reportInputEvent(input->ucId |
                (ulPinState == HAL_GPIO_PIN_SET ? EVENT_RE_CW : EVENT_RE_CCW));
          }
        } else if (input->ucSubId == INPUT_SUB_ID_RE_B) {
          if (input->associatedInput->ulInputLevel == HAL_GPIO_PIN_SET) {
            // A = H, B = (C)
            reportInputEvent(input->ucId |
                (ulPinState == HAL_GPIO_PIN_SET ? EVENT_RE_CW : EVENT_RE_CCW));
          } else {
            // A = L, B = (C)
            reportInputEvent(input->ucId |
                (ulPinState == HAL_GPIO_PIN_SET ? EVENT_RE_CCW : EVENT_RE_CW));
          }
        }

        break;
      }

      default: {
        break;
      }
      }

      // Exit for loop
      break;
    }
  }
}
