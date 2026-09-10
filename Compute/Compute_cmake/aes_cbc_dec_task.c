/*******************************************************************************
 * file           : aes_cbc_dec_task.c
 * brief          : The AES CBC decrypt task implementation.
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/semphr.h"
#include "middleware/freertos/include/event_groups.h"
#include "middleware/freertos/include/stream_buffer.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "aes_cbc_dec_task.h"

__attribute__((aligned(4)))
const uint32_t Key[4] = {0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c};
__attribute__((aligned(4)))
const uint32_t IV[4] = {0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f};

__attribute__((aligned(4)))
static uint32_t decComputedCiphertext[16] = {0};
__attribute__((aligned(4)))
static uint32_t decComputedPlaintext [16] = {0};

/*
 * @brief:  Exit the AES CBC decrypt task when there is an error
 *
 * @param error A description of the error that occurred
 */
static void exitAESCBCDecTask(char *error) {
  ErrorHandler(error);

  vTaskDelete(NULL);
}

/*
 * @brief: Perform AEC CBC decrypt calculation
 *
 * @param params The task parameters
 *
 * @return HAL status
 */
static hal_status_t performAESCBCDec(AES_CBC_DEC_PARAMETERS * params) {
  HAL_GPIO_WritePin(HAL_GPIOC, PC5_PIN, HAL_GPIO_PIN_SET);

  SWD_printf("Received AES CBC encrypted:\n");
  for (uint32_t i = 0; i < 2; i++) {
    for (uint32_t j = 0; j < 8; j++) {
      SWD_printf("%08x ", decComputedCiphertext[8*i + j]);
    }
    SWD_printf("\n");
  }

  hal_aes_handle_t * pAES = mx_aes_gethandle();
  HAL_AES_CBC_SetConfig(pAES, IV);
  HAL_AES_SetNormalKey(pAES, HAL_AES_KEY_SIZE_128BIT, Key);
  HAL_AES_SetDataSwapping(pAES, HAL_AES_DATA_SWAPPING_NO);

  hal_status_t hal_status;
  hal_status = HAL_AES_Decrypt(pAES, decComputedCiphertext, 64,
      decComputedPlaintext, 100);
  if (hal_status != HAL_OK) {
    return hal_status;
  }

  // Print the decrypted buffer
  if (xSemaphoreTake(params->xPrintMutex, portMAX_DELAY) == pdPASS) {
    SWD_printf("AES CBC decrypted:\n");
    for (uint32_t i = 0; i < 2; i++) {
      for (uint32_t j = 0; j < 8; j++) {
        SWD_printf("%08x ", decComputedPlaintext[8*i + j]);
      }
      SWD_printf("\n");
    }
    SWD_printf("-------------------\n");

    xSemaphoreGive(params->xPrintMutex);
  }

  HAL_GPIO_WritePin(HAL_GPIOC, PC5_PIN, HAL_GPIO_PIN_RESET);

  return HAL_OK;
}

/*
 * @brief:  The AES CBC decrypt task function
 *
 * @param pvParameters Task parameters
 */
static void vAESCBCDecTaskFunction(void *pvParameters) {
  AES_CBC_DEC_PARAMETERS * params = (AES_CBC_DEC_PARAMETERS *)pvParameters;

  while (1) {
    // Read from the stream buffer
    xStreamBufferReceive(params->xAESStreamBuffer, decComputedCiphertext, 64,
        portMAX_DELAY);
    if (performAESCBCDec(params) != HAL_OK) {
      exitAESCBCDecTask("performAESCBCDec failed\n");
      return;
    }
  }
}

/*
 * @brief  Initialize the AES CBC decrypt task.
 *    When this function is called, buttons must not be pressed.
 *
 * @param params Task parameters
 *
 * @retval HAL_OK if the method succeeds.
 */
hal_status_t AES_CBC_Dec_Init(AES_CBC_DEC_PARAMETERS *params) {
  if (xTaskCreate(
      vAESCBCDecTaskFunction,   // Function that implements the task
      "AESCBCDec_Task",         // Text name for the task
      256,                // Stack size in words
      params,             // Parameter passed into the task
      11,                 // Priority
      NULL                // Used to pass out the task's handle
      ) == pdPASS) {
    return HAL_OK;
  } else {
    return HAL_ERROR;
  }
}

