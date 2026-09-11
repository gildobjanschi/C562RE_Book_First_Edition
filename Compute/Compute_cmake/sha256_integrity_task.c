/*******************************************************************************
 * file           : sha256_integrity_task.c
 * brief          : The SHA256 integrity task implementation.
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/semphr.h"
#include "middleware/freertos/include/event_groups.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "sha256_integrity_task.h"

#define HASH_OUTPUT_BUFFER_SIZE 32
#define MESSAGE_SIZE 163

/*
 * Extract from NIST Publication: SHA Test Vectors for
 * Hashing Byte-Oriented Messages SHA256LongMsg.rsp
 *
 * Len = 1304
 * Msg = 451101250ec6f26652249d59dc974b7361d571a8101cdfd36aba3b5854d3ae0
 * 86b5fdd4597721b66e3c0dc5d8c606d9657d0e323283a5217d1f53f2f284f57b85c8a
 * 61ac8924711f895c5ed90ef17745ed2d728abd22a5f7a13479a462d71b56c19a74a40
 * b655c58edfe0a188ad2cf46cbf30524f65d423c837dd1ff2bf462ac4198007345bb44
 * dbb7b1c861298cdf61982a833afc728fae1eda2f87aa2c9480858bec
 * MD = 3c593aa539fdcdae516cdf2f15000f6634185c88f505b39775fb9ab137a10aa2
 */
static uint8_t Message[MESSAGE_SIZE] =
{
  0x45, 0x11, 0x01, 0x25, 0x0e, 0xc6, 0xf2, 0x66, 0x52, 0x24, 0x9d, 0x59, 0xdc, 0x97, 0x4b, 0x73,
  0x61, 0xd5, 0x71, 0xa8, 0x10, 0x1c, 0xdf, 0xd3, 0x6a, 0xba, 0x3b, 0x58, 0x54, 0xd3, 0xae, 0x08,
  0x6b, 0x5f, 0xdd, 0x45, 0x97, 0x72, 0x1b, 0x66, 0xe3, 0xc0, 0xdc, 0x5d, 0x8c, 0x60, 0x6d, 0x96,
  0x57, 0xd0, 0xe3, 0x23, 0x28, 0x3a, 0x52, 0x17, 0xd1, 0xf5, 0x3f, 0x2f, 0x28, 0x4f, 0x57, 0xb8,
  0x5c, 0x8a, 0x61, 0xac, 0x89, 0x24, 0x71, 0x1f, 0x89, 0x5c, 0x5e, 0xd9, 0x0e, 0xf1, 0x77, 0x45,
  0xed, 0x2d, 0x72, 0x8a, 0xbd, 0x22, 0xa5, 0xf7, 0xa1, 0x34, 0x79, 0xa4, 0x62, 0xd7, 0x1b, 0x56,
  0xc1, 0x9a, 0x74, 0xa4, 0x0b, 0x65, 0x5c, 0x58, 0xed, 0xfe, 0x0a, 0x18, 0x8a, 0xd2, 0xcf, 0x46,
  0xcb, 0xf3, 0x05, 0x24, 0xf6, 0x5d, 0x42, 0x3c, 0x83, 0x7d, 0xd1, 0xff, 0x2b, 0xf4, 0x62, 0xac,
  0x41, 0x98, 0x00, 0x73, 0x45, 0xbb, 0x44, 0xdb, 0xb7, 0xb1, 0xc8, 0x61, 0x29, 0x8c, 0xdf, 0x61,
  0x98, 0x2a, 0x83, 0x3a, 0xfc, 0x72, 0x8f, 0xae, 0x1e, 0xda, 0x2f, 0x87, 0xaa, 0x2c, 0x94, 0x80,
  0x85, 0x8b, 0xec
};

static uint8_t computed_hash_message[HASH_OUTPUT_BUFFER_SIZE] = {0};
static uint32_t ComputedSize;

/*
 * @brief:  Exit the SHA256 integrity task when there is an error
 *
 * @param error A description of the error that occurred
 */
static void exitSHA256IntegrityTask(char *error) {
  ErrorHandler(error);

  vTaskDelete(NULL);
}

/*
 * @brief: Perform SHA256 integrity calculation
 *
 * @param params The task parameters
 *
 * @return HAL status
 */
static hal_status_t performSHA256Integrity(
    SHA256_INTEGRITY_PARAMETERS * params) {
  HAL_GPIO_WritePin(HAL_GPIOC, PC7_PIN, HAL_GPIO_PIN_SET);

  hal_hash_handle_t * pHash = mx_hash_gethandle();
  hal_status_t hal_status = HAL_HASH_Compute(pHash, Message, MESSAGE_SIZE,
                         computed_hash_message, HASH_OUTPUT_BUFFER_SIZE,
                         &ComputedSize, 1000);
  if (hal_status != HAL_OK) {
    return hal_status;
  }

  if (params->xPrintMutex != NULL) {
    // Print the hash
    if (xSemaphoreTake(params->xPrintMutex, portMAX_DELAY) == pdPASS) {
      SWD_printf("SHA256 integrity hash:\n");
      for (uint32_t i = 0; i < HASH_OUTPUT_BUFFER_SIZE/16; i++) {
        for (uint32_t j = 0; j < 16; j++) {
          SWD_printf("%02x ", computed_hash_message[16*i + j]);
        }
        SWD_printf("\n");
      }
      SWD_printf("-------------------\n");

      xSemaphoreGive(params->xPrintMutex);
    }
  }

  HAL_GPIO_WritePin(HAL_GPIOC, PC7_PIN, HAL_GPIO_PIN_RESET);

  return HAL_OK;
}

/*
 * @brief:  The SHA256 integrity task function
 *
 * @param pvParameters Task parameters
 */
static void vSHA256IntegrityTaskFunction(void *pvParameters) {
  SHA256_INTEGRITY_PARAMETERS * params =
      (SHA256_INTEGRITY_PARAMETERS *)pvParameters;

  EventBits_t uxBits;
  while (1) {
    // Clear the bit on exit. Do not wait for all bits.
    uxBits = xEventGroupWaitBits(params->xTasksEventGroup,
        SHA256_INTEGRITY_EV_GROUP_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    if ((uxBits & SHA256_INTEGRITY_EV_GROUP_BIT) != 0) {
      if (performSHA256Integrity(params) != HAL_OK) {
        exitSHA256IntegrityTask("performSHA256Integrity failed\n");
        return;
      }
    }
  }
}

/*
 * @brief  Initialize the SHA256 integrity task.
 *    When this function is called, buttons must not be pressed.
 *
 * @param params Task parameters
 *
 * @retval HAL_OK if the method succeeds.
 */
hal_status_t SHA256_Integrity_Init(SHA256_INTEGRITY_PARAMETERS *params) {
  if (xTaskCreate(
      vSHA256IntegrityTaskFunction,   // Function that implements the task
      "SHA256_Integrity_Task",        // Text name for the task
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

