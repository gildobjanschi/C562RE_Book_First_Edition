/*******************************************************************************
 * file           : adc.c
 * brief          : ADC related code.
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/queue.h"
#include "middleware/freertos/include/timers.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Input/input_soft_timers.h"
#include "app_input.h"
#include "adc.h"

// Interrupt callbacks
static void ADCTransfHalfCallback(hal_adc_handle_t *hADC);
static void ADCTransfCallback(hal_adc_handle_t *hADC);
static void ADCErrorCallback(hal_adc_handle_t *hADC);

// The ADC buffer holds 16 bit values
#define ADC_DMA_BUFFER_SIZE     16
uint16_t ADC_DMA_Buffer[ADC_DMA_BUFFER_SIZE];

app_adc_mode_t ADCMode;
uint32_t ulADCChannel;
uint32_t ulADCVrefVoltage = VDD_VALUE;
uint32_t ulADCTemp;

// The interrupt notification queue
static volatile QueueHandle_t sADCQueue;

/*
 * @brief:  Initialize the ADC
 *
 * @param ADCQueue The pointer to the ADC queue
 *
 * @retval: HAL_OK if the method succeeds, HAL_ERROR if it fails.
 */
hal_status_t ADC_Init(QueueHandle_t ADCQueue) {
  hal_status_t status;
  hal_adc_handle_t *hADC = mx_adc1_gethandle();

  // Register the ADC half complete callback
  status = HAL_ADC_RegisterDataTransferHalfCallback(hADC,
      ADCTransfHalfCallback);
  if (status != HAL_OK) {
    return status;
  }

  // Register the ADC complete callback
  status = HAL_ADC_RegisterDataTransferCpltCallback(hADC, ADCTransfCallback);
  if (status != HAL_OK) {
    return status;
  }

  // Register the error callback
  status = HAL_ADC_RegisterErrorCallback(hADC, ADCErrorCallback);
  if (status != HAL_OK) {
    return status;
  }

  sADCQueue = ADCQueue;

  return HAL_OK;
}

/*
 * @brief:  Start the ADC
 *
 * @param ulChannel The channel
 *    (HAL_ADC_CHANNEL_VREFINT, HAL_ADC_CHANNEL_TEMPSENSOR, HAL_ADC_CHANNEL_x)
 * @param mode The mode used for the conversion
 *
 * @retval: HAL_OK if the method succeeds.
 */
hal_status_t ADC_Start(uint32_t ulChannel, app_adc_mode_t mode) {
  hal_status_t status;
  hal_adc_handle_t *hADC = mx_adc1_gethandle();

  if (hADC->global_state != HAL_ADC_STATE_IDLE) {
    SWD_printf("ADC_Start: Invalid state: %d\n", hADC->global_state);
    return HAL_ERROR;
  }

  // Set the channel
  hal_adc_channel_config_t adc_channel_config;
  adc_channel_config.group = HAL_ADC_GROUP_REGULAR;
  adc_channel_config.sequencer_rank = 1;
  adc_channel_config.sampling_time = HAL_ADC_SAMPLING_TIME_289CYCLES;
  adc_channel_config.input_mode = HAL_ADC_IN_SINGLE_ENDED;
  HAL_ADC_SetConfigChannel(hADC, ulChannel, &adc_channel_config);

  ulADCChannel = ulChannel;
  ADCMode = mode;

  // Start ADC
  status = HAL_ADC_Start(hADC);
  if (status != HAL_OK) {
    return status;
  }

  // Perform ADC calibration
  status = HAL_ADC_Calibrate(hADC);
  if (status != HAL_OK) {
    return status;
  }

  // Start the ADC conversion by using DMA
  return HAL_ADC_REG_StartConv_DMA(hADC, (uint8_t*) ADC_DMA_Buffer,
        ADC_DMA_BUFFER_SIZE * sizeof(uint16_t));
}

/*
 * @brief:  Stop the ADC
 *
 * @retval: HAL_OK if the method succeeds.
 */
hal_status_t ADC_Stop() {
  hal_adc_handle_t *hADC = mx_adc1_gethandle();

  // Stop the ADC conversion
  HAL_ADC_REG_StopConv_DMA(hADC);

  return HAL_ADC_Stop(hADC);
}

/*
 * @brief: Half transfer complete
 *
 * @param hADC The handle to the ADC peripheral
 */
static void ADCTransfHalfCallback(hal_adc_handle_t *hADC) {
  if (hADC == mx_adc1_gethandle() && ADCMode == CONTINUOUS_DMA_CAPTURE) {
    //__asm__ volatile ("sev": : :"memory");
    uint8_t ucEvent = EVENT_ADC_HALF_DATA;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sADCQueue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * @brief: Transfer complete
 *
 * @param hADC The handle to the ADC peripheral
 */
static void ADCTransfCallback(hal_adc_handle_t *hADC) {
  if (hADC == mx_adc1_gethandle()) {
    //__asm__ volatile ("sev": : :"memory");
    uint8_t ucEvent = EVENT_ADC_CPLT_DATA;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sADCQueue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * @brief: Error complete
 *
 * @param hADC The handle to the ADC peripheral
 */
static void ADCErrorCallback(hal_adc_handle_t *hADC) {
  if (hADC == mx_adc1_gethandle()) {
    uint8_t ucEvent = EVENT_ADC_ERROR;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sADCQueue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * @brief: This function is called by the task when a complete event occurs
 *
 * @param ucEvent The event received by the task
 * @param pBuffer Pointer to the application buffer.
 *  The minimum length of this buffer must be the size of the DMA buffer.
 * @param ulBufferLength The buffer length
 * @param pulDataLength The number of samples written into the buffer
 */
void ADC_Complete(uint8_t ucEvent, uint16_t *pBuffer, uint32_t ulBufferLength,
    uint32_t *pulDataLength) {
  if (ADCMode == CONTINUOUS_DMA_CAPTURE) {
    if (ulBufferLength < ADC_DMA_BUFFER_SIZE / 2) {
      *pulDataLength = 0;
      return;
    }

    if (ucEvent == EVENT_ADC_HALF_DATA) {
      if (ulADCChannel == HAL_ADC_CHANNEL_VREFINT) {
        ulADCVrefVoltage = 0;

        for (uint32_t i = 0; i < ADC_DMA_BUFFER_SIZE / 2; i++) {
          pBuffer[i] = HAL_ADC_CALC_VREFANALOG_VOLTAGE(ADC_DMA_Buffer[i],
              HAL_ADC_RESOLUTION_12_BIT);
          ulADCVrefVoltage += pBuffer[i];
        }
      } else if (ulADCChannel == HAL_ADC_CHANNEL_TEMPSENSOR) {
        ulADCTemp = 0;

        for (uint32_t i = 0; i < ADC_DMA_BUFFER_SIZE / 2; i++) {
          pBuffer[i] = HAL_ADC_CALC_TEMPERATURE(ulADCVrefVoltage,
              ADC_DMA_Buffer[i], HAL_ADC_RESOLUTION_12_BIT);
          ulADCTemp += pBuffer[i];
        }
      } else {
        for (uint32_t i = 0; i < ADC_DMA_BUFFER_SIZE / 2; i++) {
          pBuffer[i] = HAL_ADC_CALC_DATA_TO_VOLTAGE(ulADCVrefVoltage,
              ADC_DMA_Buffer[i], HAL_ADC_RESOLUTION_12_BIT);
        }
      }

      *pulDataLength = ADC_DMA_BUFFER_SIZE / 2;
    } else if (ucEvent == EVENT_ADC_CPLT_DATA) {
      if (ulADCChannel == HAL_ADC_CHANNEL_VREFINT) {
        for (uint32_t i = ADC_DMA_BUFFER_SIZE / 2, j = 0;
            i < ADC_DMA_BUFFER_SIZE; i++, j++) {
          pBuffer[j] = HAL_ADC_CALC_VREFANALOG_VOLTAGE(ADC_DMA_Buffer[i],
              HAL_ADC_RESOLUTION_12_BIT);
          ulADCVrefVoltage += pBuffer[j];
        }

        ulADCVrefVoltage /= ADC_DMA_BUFFER_SIZE;
        SWD_printf("Ref voltage: %d mV\n", ulADCVrefVoltage);
      } else if (ulADCChannel == HAL_ADC_CHANNEL_TEMPSENSOR) {
        for (uint32_t i = ADC_DMA_BUFFER_SIZE / 2, j = 0;
            i < ADC_DMA_BUFFER_SIZE; i++, j++) {
          pBuffer[j] = HAL_ADC_CALC_TEMPERATURE(ulADCVrefVoltage,
              ADC_DMA_Buffer[i], HAL_ADC_RESOLUTION_12_BIT);
          ulADCTemp += pBuffer[j];
        }

        ulADCTemp = (ulADCTemp * 10) / ADC_DMA_BUFFER_SIZE;
        SWD_printf("Temperature: %d.%dC\n", ulADCTemp / 10, ulADCTemp % 10);
      } else {
        for (uint32_t i = ADC_DMA_BUFFER_SIZE / 2, j = 0;
            i < ADC_DMA_BUFFER_SIZE; i++, j++) {
          pBuffer[j] = HAL_ADC_CALC_DATA_TO_VOLTAGE(ulADCVrefVoltage,
              ADC_DMA_Buffer[i], HAL_ADC_RESOLUTION_12_BIT);
        }
      }

      *pulDataLength = ADC_DMA_BUFFER_SIZE / 2;
    } else {
      *pulDataLength = 0;
    }
  } else { // SINGLE_DMA_CAPTURE
    if (ulBufferLength < ADC_DMA_BUFFER_SIZE) {
      *pulDataLength = 0;
      return;
    }

    if (ucEvent == EVENT_ADC_CPLT_DATA) {
      // Stop the ADC
      ADC_Stop();

      if (ulADCChannel == HAL_ADC_CHANNEL_VREFINT) {
        ulADCVrefVoltage = 0;

        for (uint32_t i = 0; i < ADC_DMA_BUFFER_SIZE; i++) {
          pBuffer[i] = HAL_ADC_CALC_VREFANALOG_VOLTAGE(ADC_DMA_Buffer[i],
              HAL_ADC_RESOLUTION_12_BIT);
          ulADCVrefVoltage += pBuffer[i];
        }

        ulADCVrefVoltage /= ADC_DMA_BUFFER_SIZE;

        SWD_printf("Ref voltage: %d mV\n", ulADCVrefVoltage);
      } else if (ulADCChannel == HAL_ADC_CHANNEL_TEMPSENSOR) {
        ulADCTemp = 0;

        for (uint32_t i = 0; i < ADC_DMA_BUFFER_SIZE; i++) {
          pBuffer[i] = HAL_ADC_CALC_TEMPERATURE(ulADCVrefVoltage,
              ADC_DMA_Buffer[i], HAL_ADC_RESOLUTION_12_BIT);
          ulADCTemp += pBuffer[i];
        }

        ulADCTemp = (ulADCTemp * 10) / ADC_DMA_BUFFER_SIZE;
        SWD_printf("Temperature: %d.%dC\n", ulADCTemp / 10, ulADCTemp % 10);
      } else {
        for (uint32_t i = 0; i < ADC_DMA_BUFFER_SIZE; i++) {
          pBuffer[i] = HAL_ADC_CALC_DATA_TO_VOLTAGE(ulADCVrefVoltage,
              ADC_DMA_Buffer[i], HAL_ADC_RESOLUTION_12_BIT);
        }
      }

      *pulDataLength = ADC_DMA_BUFFER_SIZE;
    }
  }
}

/**
 * @brief: ADC error handler
 */
void ADC_Error() {
  SWD_printf("ADC error!\n");
}
