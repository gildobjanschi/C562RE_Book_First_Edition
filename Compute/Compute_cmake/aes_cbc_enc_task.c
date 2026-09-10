/*******************************************************************************
 * file           : aes_cbc_enc_task.c
 * brief          : The AES CBC encrypt task implementation.
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/semphr.h"
#include "middleware/freertos/include/event_groups.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "aes_cbc_enc_task.h"

static const uint32_t plainText[16] =
{
  0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a,
  0xae2d8a57, 0x1e03ac9c, 0x9eb76fac, 0x45af8e51,
  0x30c81c46, 0xa35ce411, 0xe5fbc119, 0x1a0a52ef,
  0xf69f2445, 0xdf4f9b17, 0xad2b417b, 0xe66c3710
};

/* Computed data buffers */
__attribute__((aligned(4)))
static uint32_t computedCiphertext[16] = {0};

/*
 * @brief:  Exit the AES CBC encrypt task when there is an error
 *
 * @param error A description of the error that occurred
 */
static void exitAESCBCEncTask(char *error) {
  ErrorHandler(error);

  vTaskDelete(NULL);
}

/*
 * @brief: Perform AEC CBC encrypt calculation
 *
 * @param params The task parameters
 *
 * @return HAL status
 */
static hal_status_t performAESCBCEnc(AES_CBC_ENC_PARAMETERS * params) {
  HAL_GPIO_WritePin(HAL_GPIOC, PC8_PIN, HAL_GPIO_PIN_SET);

  hal_aes_handle_t * pAES = mx_aes_gethandle();
  hal_status_t hal_status;
  hal_status = HAL_AES_Encrypt(pAES, plainText, 64, computedCiphertext, 100);
  if (hal_status != HAL_OK) {
    return hal_status;
  }

  // Print the encrypted buffer
  if (xSemaphoreTake(params->xPrintMutex, portMAX_DELAY) == pdPASS) {
    SWD_printf("AES CBC encryption\n");
    for (uint32_t i = 0; i < 2; i++) {
      for (uint32_t j = 0; j < 8; j++) {
        SWD_printf("%08x ", computedCiphertext[8*i + j]);
      }
      SWD_printf("\n");
    }
    SWD_printf("-------------------\n");

    xSemaphoreGive(params->xPrintMutex);
  }

  HAL_GPIO_WritePin(HAL_GPIOC, PC8_PIN, HAL_GPIO_PIN_RESET);

  return HAL_OK;
}

/*
 * @brief:  The AES CBC encrypt task function
 *
 * @param pvParameters Task parameters
 */
static void vAESCBCEncTaskFunction(void *pvParameters) {
  AES_CBC_ENC_PARAMETERS * params = (AES_CBC_ENC_PARAMETERS *)pvParameters;

  EventBits_t uxBits;
  while (1) {
    // Clear the bit on exit. Do not wait for all bits.
    uxBits = xEventGroupWaitBits(params->xTasksEventGroup,
        AES_CBC_ENC_EV_GROUP_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    if ((uxBits & AES_CBC_ENC_EV_GROUP_BIT) != 0) {
      if (performAESCBCEnc(params) != HAL_OK) {
        exitAESCBCEncTask("performAESCBCEnc failed\n");
        return;
      }
    }
  }
}

/*
 * @brief  Initialize the AES CBC encrypt task.
 *    When this function is called, buttons must not be pressed.
 *
 * @param params Task parameters
 *
 * @retval HAL_OK if the method succeeds.
 */
hal_status_t AES_CBC_Enc_Init(AES_CBC_ENC_PARAMETERS *params) {
  if (xTaskCreate(
      vAESCBCEncTaskFunction,   // Function that implements the task
      "AESCBCEnc_Task",         // Text name for the task
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

