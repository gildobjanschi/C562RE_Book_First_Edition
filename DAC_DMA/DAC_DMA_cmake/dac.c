/*******************************************************************************
 * file           : dac.c
 * brief          : DAC related code.
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/queue.h"
#include "middleware/freertos/include/timers.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Input/input_soft_timers.h"
#include "app_input.h"
#include "dac.h"

// Interrupt callbacks
static void DACHalfTransfCallback(hal_dac_handle_t *hdac,
    hal_dac_channel_t channel);
static void DACTransfCallback(hal_dac_handle_t *hDAC,
    hal_dac_channel_t channel);
static void DACErrorCallback(hal_dac_handle_t *hDAC);

// The DMA buffer size
#define DAC_DMA_BUFFER_SIZE     12
#define DAC_DMA_HALF_BUFFER_SIZE (DAC_DMA_BUFFER_SIZE/2)

// The DAC buffer holds 16 bit samples
uint16_t DAC_DMA_Buffer[DAC_DMA_BUFFER_SIZE];

// Use 1 if you would like to select the sine wave generation
// Use 0 if you would like to select the RTOS response visualizer
#define SINEWAVE 1

#if (SINEWAVE == 1)
#define DAC_SAMPLES 120
static const uint16_t SineWave[DAC_SAMPLES] = {
    1600, 1684, 1767, 1850, 1932, 2013, 2093, 2171, 2248, 2323,
    2396, 2467, 2535, 2601, 2664, 2724, 2781, 2835, 2885, 2932,
    2975, 3014, 3050, 3081, 3108, 3131, 3150, 3165, 3175, 3181,
    3183, 3181, 3175, 3165, 3150, 3131, 3108, 3081, 3050, 3014,
    2975, 2932, 2885, 2835, 2781, 2724, 2664, 2601, 2535, 2467,
    2396, 2323, 2248, 2171, 2093, 2013, 1932, 1850, 1767, 1684,
    1600, 1516, 1433, 1350, 1268, 1187, 1107, 1029,  952,  877,
     804,  733,  665,  599,  536,  476,  419,  365,  315,  268,
     225,  186,  150,  119,   92,   69,   50,   35,   25,   19,
      17,   19,   25,   35,   50,   69,   92,  119,  150,  186,
     225,  268,  315,  365,  419,  476,  536,  599,  665,  733,
     804,  877,  952, 1029, 1107, 1187, 1268, 1350, 1433, 1516
};

static uint32_t ulSamplesIndex;

#else
static uint32_t ulLastCycles;
static int32_t lLastCyclesDiff;
#endif

// The interrupt notification queue
static volatile QueueHandle_t sDACQueue;

/*
 * @brief:  Initialize the DAC
 *
 * @param DACQueue The pointer to the DAC queue
 *
 * @retval: HAL_OK if the method succeeds, HAL_ERROR if it fails.
 */
hal_status_t DAC_Init(QueueHandle_t DACQueue) {
  hal_status_t status;
  hal_dac_handle_t *hDAC = mx_dac1_gethandle();

  // Register the DAC half complete callback
  status = HAL_DAC_RegisterConvHalfCpltCallback(hDAC, DACHalfTransfCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_DAC_RegisterConvHalfCpltCallback failed.\n");
    return status;
  }

  // Register the DAC complete callback
  status = HAL_DAC_RegisterConvCpltCallback(hDAC, DACTransfCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_DAC_RegisterConvCpltCallback failed.\n");
    return status;
  }

  // Register the error callback
  status = HAL_DAC_RegisterErrorCallback(hDAC, DACErrorCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_DAC_RegisterErrorCallback failed.\n");
    return status;
  }

  sDACQueue = DACQueue;

  return HAL_OK;
}

/*
 * @brief:  Start the DAC
 *
 * @retval: HAL_OK if the method succeeds.
 */
hal_status_t DAC_Start() {
  hal_status_t status;
  hal_dac_handle_t *hDAC = mx_dac1_gethandle();

#if (SINEWAVE == 1)
  // Prepare the wave by converting milli volt values to 12 bit data
  for (uint32_t i = 0; i < DAC_DMA_BUFFER_SIZE; i++) {
    DAC_DMA_Buffer[i] = LL_DAC_CALC_VOLTAGE_TO_DATA(VDD_VALUE,
        SineWave[i], LL_DAC_RESOLUTION_12B);
  }
  ulSamplesIndex = DAC_DMA_BUFFER_SIZE;
#endif //#if (SINEWAVE == 1)

  // Perform the calibration
  status = HAL_DAC_CalibrateChannelBuffer(hDAC, HAL_DAC_CHANNEL_1);
  if (status != HAL_OK) {
    SWD_printf("HAL_DAC_CalibrateChannelBuffer failed.\n");
    return status;
  }

  // Start the timer
  hal_tim_handle_t *htim6 = mx_tim6_gethandle();
  status = HAL_TIM_Start(htim6);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_Start failed.\n");
    return status;
  }

#if (SINEWAVE != 1)
  // Enable DWT
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  // Reset Cycle Counter
  DWT->CYCCNT = 0;
  // Enable Cycle Counter
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  ulLastCycles = DWT->CYCCNT;
  lLastCyclesDiff = 0;
#endif

  // Start the DAC conversion by using DMA
  status = HAL_DAC_StartChannel_DMA(hDAC, HAL_DAC_CHANNEL_1,
      (const void *)DAC_DMA_Buffer, DAC_DMA_BUFFER_SIZE * sizeof(uint16_t));
  if (status != HAL_OK) {
    SWD_printf("HAL_DAC_StartChannel_DMA failed.\n");
    return status;
  }

  return HAL_OK;
}

/*
 * @brief:  Stop the DAC
 *
 * @retval: HAL_OK if the method succeeds.
 */
hal_status_t DAC_Stop() {
  hal_status_t status;
  hal_dac_handle_t *hDAC = mx_dac1_gethandle();

  // Stop the DAC conversion
  status = HAL_DAC_StopChannel_DMA(hDAC, HAL_DAC_CHANNEL_1);
  if (status != HAL_OK) {
    SWD_printf("HAL_DAC_StopChannel_DMA failed.\n");
    return status;
  }

  // Stop the timer
  hal_tim_handle_t *htim6 = mx_tim6_gethandle();
  status = HAL_TIM_Stop(htim6);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_StOP failed.\n");
    return status;
  }

  return HAL_OK;
}

/*
 * brief: Half transfer complete
 *
 * @param hDAC The handle to the DAC peripheral
 * @param channel The DMA channel
 */
static void DACHalfTransfCallback(hal_dac_handle_t *hDAC,
    hal_dac_channel_t channel) {
  if (hDAC == mx_dac1_gethandle()) {
    __asm__ volatile ("sev": : :"memory");

    uint8_t ucEvent = EVENT_DAC_HALF_CPLT_DATA;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sDACQueue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * brief: Transfer complete
 *
 * @param hDAC The handle to the DAC peripheral
 * @param channel The DMA channel
 */
static void DACTransfCallback(hal_dac_handle_t *hDAC,
    hal_dac_channel_t channel) {
  if (hDAC == mx_dac1_gethandle()) {
    __asm__ volatile ("sev": : :"memory");
    // Important: If the DAC DMA is setup for 'Direct transfer' the line below
    // must be uncommented to stop the timer (or else an error occurs).
    //DAC_Stop();

    uint8_t ucEvent = EVENT_DAC_CPLT_DATA;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sDACQueue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * brief: Error complete
 *
 * @param hDAC The handle to the DAC peripheral
 */
static void DACErrorCallback(hal_dac_handle_t *hDAC) {
  if (hDAC == mx_dac1_gethandle()) {
    uint8_t ucEvent = EVENT_DAC_ERROR;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sDACQueue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/*
 * brief: This function is called by the task when a complete event occurs
 *
 * @param ucEvent The event that occurred
 */
void DAC_Complete(uint8_t ucEvent) {
#if (SINEWAVE == 1)
  if (ucEvent == EVENT_DAC_HALF_CPLT_DATA) {
    for (uint32_t i = 0, j = 0; i < DAC_DMA_HALF_BUFFER_SIZE; i++, j++) {
      DAC_DMA_Buffer[i] = LL_DAC_CALC_VOLTAGE_TO_DATA(VDD_VALUE,
          SineWave[j + ulSamplesIndex], LL_DAC_RESOLUTION_12B);
    }
  } else {
    for (uint32_t i = DAC_DMA_HALF_BUFFER_SIZE, j = 0; i < DAC_DMA_BUFFER_SIZE;
        i++, j++) {
      DAC_DMA_Buffer[i] = LL_DAC_CALC_VOLTAGE_TO_DATA(VDD_VALUE,
          SineWave[j + ulSamplesIndex], LL_DAC_RESOLUTION_12B);
    }
  }

  ulSamplesIndex += DAC_DMA_HALF_BUFFER_SIZE;
  if (ulSamplesIndex == DAC_SAMPLES) {
    ulSamplesIndex = 0;
  }
#else
  uint32_t ulCycles = DWT->CYCCNT;
  int32_t lCyclesDiff = ulCycles - ulLastCycles;
  ulLastCycles = ulCycles;
  uint16_t uwData = lCyclesDiff - lLastCyclesDiff + 2048;
  lLastCyclesDiff = lCyclesDiff;

  if (ucEvent == EVENT_DAC_HALF_CPLT_DATA) {
    for (uint32_t i = 0, j = 0; i < DAC_DMA_HALF_BUFFER_SIZE; i++, j++) {
      DAC_DMA_Buffer[i] = uwData;
    }
  } else {
    for (uint32_t i = DAC_DMA_HALF_BUFFER_SIZE, j = 0; i < DAC_DMA_BUFFER_SIZE;
        i++, j++) {
      DAC_DMA_Buffer[i] = uwData;
    }
  }
#endif

  __asm__ volatile ("sev": : :"memory");
}

/**
 * @brief: DAC error handler
 */
void DAC_Error() {
  SWD_printf("DAC error!\n");
  DAC_Stop();
}
