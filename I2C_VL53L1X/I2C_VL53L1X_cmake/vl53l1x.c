/*******************************************************************************
 * file           : i2c_vl53l1x.c
 * brief          : I2C VL53L1X implementation
 ******************************************************************************/
#include "mx_hal_def.h"
#include "../../Shared/Debug/swd_printf.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/queue.h"
#include "i2c1.h"
#include "vl53l1x.h"

// The I2C address shifted left once (unshifted 0x29)
#define I2C_ADDR_7BIT_SHIFTED 0x52

// VL53L1X definitions
const uint8_t VL51L1X_DEFAULT_CONFIGURATION[] = {
  0x00, /* 0x2d : set bit 2 and 5 to 1 for fast plus mode (1MHz I2C), else don't touch */
  0x01, /* 0x2e : bit 0 if I2C pulled up at 1.8V, else set bit 0 to 1 (pull up at AVDD) */
  0x01, /* 0x2f : bit 0 if GPIO pulled up at 1.8V, else set bit 0 to 1 (pull up at AVDD) */
  0x01, /* 0x30 : set bit 4 to 0 for active high interrupt and 1 for active low (bits 3:0 must be 0x1), use SetInterruptPolarity() */
  0x02, /* 0x31 : bit 1 = interrupt depending on the polarity, use CheckForDataReady() */
  0x00, /* 0x32 : not user-modifiable */
  0x02, /* 0x33 : not user-modifiable */
  0x08, /* 0x34 : not user-modifiable */
  0x00, /* 0x35 : not user-modifiable */
  0x08, /* 0x36 : not user-modifiable */
  0x10, /* 0x37 : not user-modifiable */
  0x01, /* 0x38 : not user-modifiable */
  0x01, /* 0x39 : not user-modifiable */
  0x00, /* 0x3a : not user-modifiable */
  0x00, /* 0x3b : not user-modifiable */
  0x00, /* 0x3c : not user-modifiable */
  0x00, /* 0x3d : not user-modifiable */
  0xff, /* 0x3e : not user-modifiable */
  0x00, /* 0x3f : not user-modifiable */
  0x0F, /* 0x40 : not user-modifiable */
  0x00, /* 0x41 : not user-modifiable */
  0x00, /* 0x42 : not user-modifiable */
  0x00, /* 0x43 : not user-modifiable */
  0x00, /* 0x44 : not user-modifiable */
  0x00, /* 0x45 : not user-modifiable */
  0x20, /* 0x46 : interrupt configuration 0->level low detection, 1-> level high, 2-> Out of window, 3->In window, 0x20-> New sample ready , TBC */
  0x0b, /* 0x47 : not user-modifiable */
  0x00, /* 0x48 : not user-modifiable */
  0x00, /* 0x49 : not user-modifiable */
  0x02, /* 0x4a : not user-modifiable */
  0x0a, /* 0x4b : not user-modifiable */
  0x21, /* 0x4c : not user-modifiable */
  0x00, /* 0x4d : not user-modifiable */
  0x00, /* 0x4e : not user-modifiable */
  0x05, /* 0x4f : not user-modifiable */
  0x00, /* 0x50 : not user-modifiable */
  0x00, /* 0x51 : not user-modifiable */
  0x00, /* 0x52 : not user-modifiable */
  0x00, /* 0x53 : not user-modifiable */
  0xc8, /* 0x54 : not user-modifiable */
  0x00, /* 0x55 : not user-modifiable */
  0x00, /* 0x56 : not user-modifiable */
  0x38, /* 0x57 : not user-modifiable */
  0xff, /* 0x58 : not user-modifiable */
  0x01, /* 0x59 : not user-modifiable */
  0x00, /* 0x5a : not user-modifiable */
  0x08, /* 0x5b : not user-modifiable */
  0x00, /* 0x5c : not user-modifiable */
  0x00, /* 0x5d : not user-modifiable */
  0x01, /* 0x5e : not user-modifiable */
  0xdb, /* 0x5f : not user-modifiable */
  0x0f, /* 0x60 : not user-modifiable */
  0x01, /* 0x61 : not user-modifiable */
  0xf1, /* 0x62 : not user-modifiable */
  0x0d, /* 0x63 : not user-modifiable */
  0x01, /* 0x64 : Sigma threshold MSB (mm in 14.2 format for MSB+LSB), use SetSigmaThreshold(), default value 90 mm  */
  0x68, /* 0x65 : Sigma threshold LSB */
  0x00, /* 0x66 : Min count Rate MSB (MCPS in 9.7 format for MSB+LSB), use SetSignalThreshold() */
  0x80, /* 0x67 : Min count Rate LSB */
  0x08, /* 0x68 : not user-modifiable */
  0xb8, /* 0x69 : not user-modifiable */
  0x00, /* 0x6a : not user-modifiable */
  0x00, /* 0x6b : not user-modifiable */
  0x00, /* 0x6c : Intermeasurement period MSB, 32 bits register, use SetIntermeasurementInMs() */
  0x00, /* 0x6d : Intermeasurement period */
  0x0f, /* 0x6e : Intermeasurement period */
  0x89, /* 0x6f : Intermeasurement period LSB */
  0x00, /* 0x70 : not user-modifiable */
  0x00, /* 0x71 : not user-modifiable */
  0x00, /* 0x72 : distance threshold high MSB (in mm, MSB+LSB), use SetD:tanceThreshold() */
  0x00, /* 0x73 : distance threshold high LSB */
  0x00, /* 0x74 : distance threshold low MSB ( in mm, MSB+LSB), use SetD:tanceThreshold() */
  0x00, /* 0x75 : distance threshold low LSB */
  0x00, /* 0x76 : not user-modifiable */
  0x01, /* 0x77 : not user-modifiable */
  0x0f, /* 0x78 : not user-modifiable */
  0x0d, /* 0x79 : not user-modifiable */
  0x0e, /* 0x7a : not user-modifiable */
  0x0e, /* 0x7b : not user-modifiable */
  0x00, /* 0x7c : not user-modifiable */
  0x00, /* 0x7d : not user-modifiable */
  0x02, /* 0x7e : not user-modifiable */
  0xc7, /* 0x7f : ROI center, use SetROI() */
  0xff, /* 0x80 : XY ROI (X=Width, Y=Height), use SetROI() */
  0x9B, /* 0x81 : not user-modifiable */
  0x00, /* 0x82 : not user-modifiable */
  0x00, /* 0x83 : not user-modifiable */
  0x00, /* 0x84 : not user-modifiable */
  0x01, /* 0x85 : not user-modifiable */
  0x00, /* 0x86 : clear interrupt, use ClearInterrupt() */
  0x00  /* 0x87 : start ranging, use StartRanging() or StopRanging(), If you want an automatic start after VL53L1X_init() call, put 0x40 in location 0x87 */
};

// Register addresses
#define VL53L1_VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND        0x0008
#define GPIO_HV_MUX__CTRL                 0x0030
#define GPIO__TIO_HV_STATUS               0x0031
#define VL53L1_RESULT__FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0       0x0096
#define SYSTEM__INTERRUPT_CLEAR           0x0086
#define SYSTEM__MODE_START                0x0087
#define VL53L1_IDENTIFICATION__MODEL_ID   0x010F

typedef enum {
  SM_INIT,
  SM_INIT_START_RANGING_CLEAR_INT,
  SM_INIT_START_RANGING_START,
  SM_INIT_CHECK_DATA_READY_INT_POLARITY,
  SM_INIT_CHECK_DATA_READY_STATUS,
  SM_INIT_CLEAR_INT,
  SM_INIT_STOP_RANGING,
  SM_INIT_CONFIG_TIMEOUT,
  SM_INIT_COMPLETE,
  SM_GET_MODEL_ID,
  SM_GET_DISTANCE
} I2C_SM;

static I2C_SM VL53L1X_State;
static uint8_t ucIntPolarity;
static uint16_t uwInitRegisterAddr;

// The state machine function
static hal_status_t VL53L1X_StateMachine(uint8_t *pRxBuffer,
    uint32_t ulRxBytes, uint16_t *puwDistance);

/*
 * @brief  Initialize the I2C
 *
 * @param xQueue The I2cQueue
 * @retval HAL_OK if it succeeds
 */
hal_status_t VL53L1X_Init(QueueHandle_t I2cQueue) {
  return I2C1_Init(I2cQueue, VL53L1X_StateMachine, I2C_ADDR_7BIT_SHIFTED);
}

/*
 * @brief  Start the I2C transactions
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t VL53L1X_Start() {
  // Initialize the registers where we write the initialization bytes
  uwInitRegisterAddr = 0x2D;

  // Start the state machine
  VL53L1X_State = SM_INIT;
  VL53L1X_StateMachine(NULL, 0, NULL);

  return HAL_OK;
}

/*
 * @brief  VL53L1X state machine
 *
 * @param pRxBuffer The pointer to the receive buffer (NULL if Tx completed)
 * @param ulRxBytes The number of bytes received (0 if Tx completed)
 * @param puwDistance The returned distance when return value is
 *      HAL_DISTANCE_AVAIL
 *
 * @retval HAL_OK if the function succeeds, HAL_DISTANCE_AVAIL when
 *  the distance is available, error otherwise
 */
hal_status_t VL53L1X_StateMachine(uint8_t *pRxBuffer, uint32_t ulRxBytes,
    uint16_t *puwDistance) {
  hal_status_t status;

  uint8_t ucData;
  switch (VL53L1X_State) {
  case SM_INIT: {
    if (uwInitRegisterAddr > 0x87) {
      // Clear the interrupt
      ucData = 1;
      status = I2C1_Send(SYSTEM__INTERRUPT_CLEAR, &ucData, 1);
      if (status != HAL_OK) {
        return status;
      }

      SWD_printf("SM_INIT -> SM_INIT_START_RANGING_CLEAR_INT.\n");

      VL53L1X_State = SM_INIT_START_RANGING_CLEAR_INT;
    } else {
      ucData = VL51L1X_DEFAULT_CONFIGURATION[uwInitRegisterAddr - 0x2D];
      status = I2C1_Send(uwInitRegisterAddr, &ucData, 1);
      if (status != HAL_OK) {
        return status;
      }

      uwInitRegisterAddr++;
      // Stay in SM_INIT state machine until all registers are initialized.
    }

    break;
  }

  case SM_INIT_START_RANGING_CLEAR_INT: {
    // Start ranging
    ucData = 0x40;
    status = I2C1_Send(SYSTEM__MODE_START, &ucData, 1);
    if (status != HAL_OK) {
      return status;
    }

    SWD_printf("SM_INIT_START_RANGING_CLEAR_INT -> "
        "SM_INIT_START_RANGING_START.\n");

    VL53L1X_State = SM_INIT_START_RANGING_START;
    break;
  }

  case SM_INIT_START_RANGING_START: {
    // Read the interrupt polarity
    status = I2C1_Recv(GPIO_HV_MUX__CTRL, 1);
    if (status != HAL_OK) {
      return status;
    }

    SWD_printf("SM_INIT_START_RANGING_START -> "
        "SM_INIT_CHECK_DATA_READY_INT_POLARITY.\n");

    VL53L1X_State = SM_INIT_CHECK_DATA_READY_INT_POLARITY;
    break;
  }

  case SM_INIT_CHECK_DATA_READY_INT_POLARITY: {
    // It may take as much as 103ms for the sensor to be ready
    HAL_Delay(100);

    // Handle the interrupt polarity
    ucIntPolarity = !((pRxBuffer[0] & 0x10) >> 4);
    SWD_printf("VL53L1X_StateMachine: Int polarity: %x\n", ucIntPolarity);

    // Read the status
    status = I2C1_Recv(GPIO__TIO_HV_STATUS, 1);
    if (status != HAL_OK) {
      return status;
    }

    SWD_printf("SM_INIT_CHECK_DATA_READY_INT_POLARITY -> "
        "SM_INIT_CHECK_DATA_READY_STATUS\n");

    VL53L1X_State = SM_INIT_CHECK_DATA_READY_STATUS;
    break;
  }

  case SM_INIT_CHECK_DATA_READY_STATUS: {
    uint8_t ucVLStatus = pRxBuffer[0];
    SWD_printf("VL53L1X_StateMachine: Status: %x\n", ucVLStatus);

    if ((ucVLStatus & 1) == ucIntPolarity) { // Ready
      // Clear the interrupt again
      ucData = 1;
      status = I2C1_Send(SYSTEM__INTERRUPT_CLEAR, &ucData, 1);
      if (status != HAL_OK) {
        return status;
      }

      SWD_printf("SM_INIT_CHECK_DATA_READY_STATUS -> SM_INIT_CLEAR_INT.\n");

      VL53L1X_State = SM_INIT_CLEAR_INT;
    } else { // Not ready
      // Read the interrupt polarity
      status = I2C1_Recv(GPIO_HV_MUX__CTRL, 1);
      if (status != HAL_OK) {
        return status;
      }

      SWD_printf("SM_INIT_CHECK_DATA_READY_STATUS -> "
          "SM_INIT_CHECK_DATA_READY_INT_POLARITY [try again].\n");

      // Go back to check interrupt polarity.
      VL53L1X_State = SM_INIT_CHECK_DATA_READY_INT_POLARITY;
    }

    break;
  }

  case SM_INIT_CLEAR_INT: {
    // Stop ranging
    ucData = 0;
    status = I2C1_Send(SYSTEM__MODE_START, &ucData, 1);
    if (status != HAL_OK) {
      return status;
    }

    SWD_printf("SM_INIT_CLEAR_INT -> SM_INIT_STOP_RANGING.\n");

    VL53L1X_State = SM_INIT_STOP_RANGING;
    break;
  }

  case SM_INIT_STOP_RANGING: {
    ucData = 0x9;
    status = I2C1_Send(VL53L1_VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND, &ucData,
        1);
    if (status != HAL_OK) {
      return status;
    }

    SWD_printf("SM_INIT_STOP_RANGING -> SM_INIT_CONFIG_TIMEOUT.\n");

    VL53L1X_State = SM_INIT_CONFIG_TIMEOUT;
    break;
  }

  case SM_INIT_CONFIG_TIMEOUT: {
    ucData = 0;
    status = I2C1_Send(0x0B, &ucData, 1);
    if (status != HAL_OK) {
      return status;
    }

    SWD_printf("SM_INIT_CONFIG_TIMEOUT -> SM_INIT_COMPLETE.\n");

    VL53L1X_State = SM_INIT_COMPLETE;
    break;
  }

  case SM_INIT_COMPLETE: {
    // Get the sensor model
    status = I2C1_Recv(VL53L1_IDENTIFICATION__MODEL_ID, 2);
    if (status != HAL_OK) {
      return status;
    }

    SWD_printf("SM_INIT_COMPLETE -> SM_GET_MODEL_ID.\n");

    VL53L1X_State = SM_GET_MODEL_ID;
    break;
  }

  case SM_GET_MODEL_ID: {
    uint16_t uwModelId = pRxBuffer[1] | (pRxBuffer[0] << 8);
    SWD_printf("VL53L1X_StateMachine: Model id: %x\n", uwModelId);

    // Read the distance
    status = I2C1_Recv(VL53L1_RESULT__FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0,
        2);
    if (status != HAL_OK) {
      return status;
    }

    SWD_printf("SM_GET_MODEL_ID -> SM_GET_DISTANCE.\n");

    VL53L1X_State = SM_GET_DISTANCE;
    break;
  }

  case SM_GET_DISTANCE: {
    *puwDistance = pRxBuffer[1] | (pRxBuffer[0] << 8);
    return HAL_DISTANCE_AVAIL;
  }
  }

  return HAL_OK;
}
