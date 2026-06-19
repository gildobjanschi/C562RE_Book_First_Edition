/*******************************************************************************
 * file           : input_capture_dma.c
 * brief          : Input capture using DMA implementation
 ******************************************************************************/
#include <string.h>
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/queue.h"
#include "../../Shared/Debug/swd_printf.h"
#include "input_capture_dma.h"

// DMA capture buffer
static uint32_t InputDmaBuffer[DMA_BUFFER_SIZE];

// Callback definitions
static void InputCaptureCallback(hal_tim_handle_t *htim,
    hal_tim_channel_t channel);
static void InputCaptureHalfCallback(hal_tim_handle_t *htim,
    hal_tim_channel_t channel);
static void ErrorCallback(hal_tim_handle_t *htim);

static volatile QueueHandle_t sInterruptQueue;

/*
 * @brief  Initialize the input capture using DMA
 *
 * @param interruptQueue The interrupt queue used to notify the task
 *    of interrupt events
 * @retval HAL_OK if it succeeds
 */
hal_status_t Input_Capture_DMA_Init(QueueHandle_t interruptQueue) {
  sInterruptQueue = interruptQueue;

  hal_status_t status;
  hal_tim_handle_t *htim2 = mx_tim2_gethandle();

  status = HAL_TIM_RegisterInputCaptureCallback(htim2, InputCaptureCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_RegisterInputCaptureCallback failed.\n");
    return status;
  }

  status = HAL_TIM_RegisterInputCaptureHalfCpltCallback(htim2,
      InputCaptureHalfCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_RegisterInputCaptureHalfCpltCallback failed.\n");
    return status;
  }

  status = HAL_TIM_RegisterErrorCallback(htim2, ErrorCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_RegisterErrorCallback failed.\n");
    return status;
  }

  return HAL_OK;
}

/*
 * @brief  Start the input capture using DMA
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t Start_Input_Capture_DMA() {
  hal_status_t status;

  // Clear the capture buffer
  memset(InputDmaBuffer, 0, sizeof(InputDmaBuffer));

  // Start the Input Capture in DMA mode
  hal_tim_handle_t *htim2 = mx_tim2_gethandle();
  status = HAL_TIM_IC_StartChannel_DMA(htim2, HAL_TIM_CHANNEL_4,
      (uint8_t*) InputDmaBuffer,
      (uint32_t) (DMA_BUFFER_SIZE * sizeof(uint32_t)));
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_IC_StartChannel_DMA failed.\n");
    return status;
  }

  // Start the timer
  status = HAL_TIM_Start(htim2);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_Start failed.\n");
    return status;
  }

  return HAL_OK;
}

/*
 * @brief  Stop the input capture using DMA
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t Stop_Input_Capture_DMA() {
  hal_status_t status;
  hal_tim_handle_t *htim2 = mx_tim2_gethandle();

  // Stop the Input Capture in DMA mode
  status = HAL_TIM_IC_StopChannel_DMA(htim2, HAL_TIM_CHANNEL_4);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_IC_StopChannel_DMA failed.\n");
    return status;
  }

  // Stop the timer
  status = HAL_TIM_Stop(htim2);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_Stop failed.\n");
    return status;
  }

  return HAL_OK;
}

/*
 * @brief This callback is invoked by the HAL when the first half of the
 *    DMA has been filled.
 *
 * @param hdma Pointer to DMA handle.
 * @param channel The DMA channel
 */
static void InputCaptureHalfCallback(hal_tim_handle_t *htim,
    hal_tim_channel_t channel) {
  if ((htim == mx_tim2_gethandle()) && (channel == HAL_TIM_CHANNEL_4)) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    for (uint32_t i = 0; i < DMA_BUFFER_SIZE / 2; i++) {
      xQueueSendFromISR(sInterruptQueue, InputDmaBuffer + i,
          &xHigherPriorityTaskWoken);
    }

    if (xHigherPriorityTaskWoken) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * @brief This callback is invoked by the HAL when an input capture DMA transfer
 *      window completes.
 *
 * @param htim Pointer to timer handle.
 * @param channel The timer channel
 */
static void InputCaptureCallback(hal_tim_handle_t *htim,
    hal_tim_channel_t channel) {
  if ((htim == mx_tim2_gethandle()) && (channel == HAL_TIM_CHANNEL_4)) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    for (uint32_t i = DMA_BUFFER_SIZE / 2; i < DMA_BUFFER_SIZE; i++) {
      xQueueSendFromISR(sInterruptQueue, InputDmaBuffer + i,
          &xHigherPriorityTaskWoken);
      }

    if (xHigherPriorityTaskWoken) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * @brief: This callback is invoked by the HAL when an error occurs on the
 *      TIM instance.
 *
 * @param htim Pointer to timer handle.
 */
static void ErrorCallback(hal_tim_handle_t *htim) {
  if (htim == mx_tim2_gethandle()) {
    uint16_t uwEvent = EVENT_ERROR;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sInterruptQueue, &uwEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}
