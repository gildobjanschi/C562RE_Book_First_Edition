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
#include "dac.h"

// Interrupt callbacks
static void DACErrorCallback(hal_dac_handle_t *hDAC);

/*
Calibration values
--------------------------------------------------------
Indicated value     RMS voltage     Peak-to-peak voltage
--------------------------------------------------------
+5 dB               0.562V           1.59V
+3 dB               0.446V           1.27V
+2 dB               0.398V           1.13V
+1 dB               0.354V           0.99V
 0 dB               0.316V           0.90V
-1 dB               0.281V           0.79V
-2 dB               0.251V           0.71V
-3 dB               0.224V           0.62V
-5 dB               0.178V           0.51V
-7 dB               0.141V           0.40V
-10dB               0.100V           0.28V
-20dB               0.031V           0.08V
--------------------------------------------------------
*/

// The correction table
#define RMS_TO_DB_SIZE 13
static const uint16_t RMS_to_dB[RMS_TO_DB_SIZE][2] = {
  {  0,   0},   //-25dB
  { 31, 200},   //-20dB
  {100, 540},   //-10dB
  {141, 780},   // -7dB
  {178, 980},   // -5dB
  {224, 1220},  // -3dB
  {251, 1360},  // -2dB
  {281, 1500},  // -1dB
  {316, 1720},  //  0dB
  {354, 1910},  // +1dB
  {398, 2080},  // +2dB
  {446, 2300},  // +3dB
  {562, 2910},  // +5dB
};

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

  // Perform the calibration
  status = HAL_DAC_CalibrateChannelBuffer(hDAC, HAL_DAC_CHANNEL_1);
  if (status != HAL_OK) {
    SWD_printf("HAL_DAC_CalibrateChannelBuffer failed.\n");
    return status;
  }

  // Start the DAC conversion
  status = HAL_DAC_StartChannel(hDAC, HAL_DAC_CHANNEL_1);
  if (status != HAL_OK) {
    SWD_printf("HAL_DAC_StartChannel failed.\n");
    return status;
  }

  return HAL_OK;
}

/*
 * @brief:  Output this voltage to the DAC after performing a correction.
 *
 * @param uwVoltage The voltage value to output
 *
 * @retval: HAL_OK if the method succeeds.
 */
hal_status_t DAC_Output(uint16_t uwVoltage) {
  uint16_t uwCorrectedVoltage = VDD_VALUE;
  uint16_t uwOldScaleDiff, uwNewScaleDiff;
  for (uint8_t i = 0; i < RMS_TO_DB_SIZE - 1; i++) {
    if (uwVoltage >= RMS_to_dB[i][0] && uwVoltage < RMS_to_dB[i+1][0]) {
      uwOldScaleDiff = RMS_to_dB[i+1][0] - RMS_to_dB[i][0];
      uwNewScaleDiff = RMS_to_dB[i+1][1] - RMS_to_dB[i][1];
      uwCorrectedVoltage = uwVoltage - RMS_to_dB[i][0];
      uwCorrectedVoltage =
          (uwCorrectedVoltage * uwNewScaleDiff) / uwOldScaleDiff;
      uwCorrectedVoltage += RMS_to_dB[i][1];
      SWD_printf("Set output: %d -> %d.\n", uwVoltage, uwCorrectedVoltage);
      break;
    }
  }

  // Convert voltage to value
  uint16_t uwValue = LL_DAC_CALC_VOLTAGE_TO_DATA(VDD_VALUE, uwCorrectedVoltage,
      LL_DAC_RESOLUTION_12B);

  // Output the value
  hal_dac_handle_t *hDAC = mx_dac1_gethandle();
  return HAL_DAC_SetChannelData(hDAC, HAL_DAC_CHANNEL_1, uwValue);
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
  status = HAL_DAC_StopChannel(hDAC, HAL_DAC_CHANNEL_1);
  if (status != HAL_OK) {
    SWD_printf("HAL_DAC_StopChannel failed.\n");
    return status;
  }

  return HAL_OK;
}

/*
 * @brief: Error callback
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

/**
 * @brief: DAC error handler
 */
void DAC_Error() {
  SWD_printf("DAC error!\n");
  DAC_Stop();
}
