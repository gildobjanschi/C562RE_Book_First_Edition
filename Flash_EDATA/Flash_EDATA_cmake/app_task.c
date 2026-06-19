/*******************************************************************************
 * file           : app_task.c
 * brief          : The application task implementation.
 ******************************************************************************/
#include <string.h>
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/queue.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "../../Shared/Input/input.h"
#include "app_input.h"
#include "app_task.h"

/*
 * @brief:  The NMI handler
 */
system_status_t FLASH_NMI_IRQHandler(void) {
  hal_flash_handle_t *pFLASH = mx_flash_gethandle();
  return ((HAL_FLASH_NMI_IRQHandler(pFLASH) == HAL_OK) ?
      SYSTEM_OK : SYSTEM_PERIPHERAL_ERROR);
}

#define FLASH_EDATA_ADDRESS (uint32_t) (FLASH_EDATA_BASE + FLASH_EDATA_BANK_SIZE)
volatile uint32_t EccErrorDetection = 0;

/*
 * @brief:  The ECC error callback
 *
 * @param hflash The handle to the flash
 * @param bank The flash bank
 */
static hal_status_t ECCErrorCallback(hal_flash_handle_t *hflash,
    hal_flash_bank_t bank) {
  STM32_UNUSED(bank);
  hal_flash_ecc_info_t info;
  // Manage the ECC error generated when reading back erased EDATA memory.
  hal_flash_handle_t *pFLASH = mx_flash_gethandle();
  // Retrieve information about the ECC failure
  HAL_FLASH_ECC_GetInfo(pFLASH, bank, &info);
  if (info.addr >= (FLASH_EDATA_ADDRESS)
    && info.addr < (FLASH_EDATA_ADDRESS + FLASH_EDATA_PAGE_SIZE)) {
    EccErrorDetection = 1U;
    //SWD_printf("ECCErrorCallback: @%lx.\n", info.addr);
    return HAL_OK;
  } else {
    SWD_printf("ECCErrorCallback incorrect range @%lx\n", info.addr);
    return HAL_ERROR;
  }
}

#define BUFFER_SIZE   (FLASH_EDATA_PAGE_SIZE / sizeof(uint32_t))
uint32_t WriteBuffer[BUFFER_SIZE] __attribute__ ((aligned (8)));
uint32_t ReadBuffer[BUFFER_SIZE] __attribute__ ((aligned (8)));

#define TIMEOUT_MS 1000UL
/*
 * @brief:  The EDATA test
 */
static void EDATA_Test() {
  SWD_printf("Erasing flash...\n");
  // Erase the flash
  hal_flash_handle_t *pFLASH = mx_flash_gethandle();
  if (HAL_FLASH_EDATA_EraseByAddr(pFLASH, FLASH_EDATA_ADDRESS,
      FLASH_EDATA_PAGE_SIZE, TIMEOUT_MS) != HAL_OK) {
    SWD_printf("HAL_FLASH_EDATA_EraseByAddr failed\n");
    return;
  }

  SWD_printf("Reading erased flash...\n");
  // Read from flash
  uint32_t *pR;
  for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
    pR = ReadBuffer + i;
    *pR = *(volatile uint32_t *)(FLASH_EDATA_ADDRESS + (i * sizeof(uint32_t)));
    if (EccErrorDetection == 1) {
      EccErrorDetection = 0;
      *pR = 0;
    }
  }
  SWD_printf("Erased flash read done.\n");

  // Verify that the data was erased
  memset(WriteBuffer, 0, (BUFFER_SIZE * sizeof(uint32_t)));
  if (memcmp(ReadBuffer, WriteBuffer, (BUFFER_SIZE * sizeof(uint32_t))) != 0U) {
    SWD_printf("memcmp failed after erase\n");
    return;
  }
  SWD_printf("Erased flash read OK.\n");

  SWD_printf("Programming flash...\n");
  // Write to flash
  memset(WriteBuffer, 0xEA, (BUFFER_SIZE * sizeof(uint32_t)));
  if (HAL_FLASH_EDATA_ProgramByAddr(pFLASH, FLASH_EDATA_ADDRESS, WriteBuffer,
      FLASH_EDATA_PAGE_SIZE, TIMEOUT_MS) != HAL_OK) {
    SWD_printf("HAL_FLASH_EDATA_ProgramByAddr failed\n");
    return;
  }

  SWD_printf("Reading programmed flash...\n");
  // Read from flash
  for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
    *(ReadBuffer + i) =
        (*(volatile uint32_t *)(FLASH_EDATA_ADDRESS + (i * sizeof(uint32_t))));
  }
  SWD_printf("Programmed flash read done.\n");

  // Compare what was read with what was written
  if (memcmp(ReadBuffer, WriteBuffer, (BUFFER_SIZE * sizeof(uint32_t))) != 0U) {
    SWD_printf("memcmp failed after programming\n");
    return;
  }

  SWD_printf("Programmed flash read OK.\n");
}

/*
 * @brief:  The EDATA read
 */
static void EDATA_Read() {
  SWD_printf("Reading programmed flash...\n");
  // Read from flash
  for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
    *(ReadBuffer + i) =
        (*(__IO uint32_t *)(FLASH_EDATA_ADDRESS + (i * sizeof(uint32_t))));
  }
  SWD_printf("Flash read done.\n");

  // Compare what was read with what was written
  // Prepare the buffer for comparison with flash data
  memset(WriteBuffer, 0xEA, (BUFFER_SIZE * sizeof(uint32_t)));
  if (memcmp(ReadBuffer, WriteBuffer, (BUFFER_SIZE * sizeof(uint32_t))) != 0U) {
    SWD_printf("Flash is not programmed.\n");
  } else {
    SWD_printf("Flash is programmed.\n");
  }
}

/*
 * @brief:  Process the input event
 *
 * @param ucEvent The event that occurred
 *
 * @retval 1 if an event is handled, 0 if not handled
 */
static uint8_t processInputEvent(uint8_t ucEvent) {
  uint8_t ucInputId = ucEvent & INPUT_ID_MASK;
  uint8_t ucEventType = ucEvent & EVENT_TYPE_MASK;

  switch (ucInputId) {
  case INPUT_ID_BTN_1: {
    switch (ucEventType) {
    case EVENT_CLICK: {
      SWD_printf("--> BTN_1 CLICK\n");
      EDATA_Test();
      break;
    }

    case EVENT_LONG_CLICK: {
      SWD_printf("--> BTN_1 LONG CLICK\n");
      EDATA_Read();
      break;
    }

    default: {
      break;
    }
    }
    break;
  }

  default: {
    SWD_printf("Unhandled id: %d, event: %d\n", ucInputId, ucEventType);
    break;
  }
  }

  return 0;
}

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 * @param xInputQueue The input queue
 */
static void exitAppTask(char *error, QueueHandle_t xInputQueue) {
  ErrorHandler(error);

  // Release the input handling
  Input_Free();

  // Free the input queue
  if (xInputQueue != NULL) {
    vQueueDelete(xInputQueue);
  }

  vTaskDelete(NULL);
}

#define INPUT_QUEUE_SIZE  8

/*
 * @brief:  The task function
 *
 * @param pvParameters Task parameters
 */
static void vTaskFunction(void *pvParameters) {
  // Create the input queue
  QueueHandle_t xInputQueue = xQueueCreate(INPUT_QUEUE_SIZE, sizeof(uint8_t));
  if (xInputQueue == NULL) {
    return exitAppTask("Cannot create input queue.\n", xInputQueue);
  }

  // Initialize the input
  if (App_Input_Init(xInputQueue) != HAL_OK) {
    return exitAppTask("App_Input_Init failed.\n", xInputQueue);
  }

  // Register the ECC error callback
  hal_flash_handle_t *pFLASH = mx_flash_gethandle();
  if (HAL_FLASH_RegisterECCErrorCallback(pFLASH, ECCErrorCallback) != HAL_OK) {
    return exitAppTask("HAL_FLASH_RegisterECCErrorCallback failed\n",
        xInputQueue);
  }

  // Unlock write access to registers
  if (HAL_FLASH_ITF_Unlock(HAL_FLASH) != HAL_OK) {
    return exitAppTask("HAL_FLASH_ITF_Unlock failed\n", xInputQueue);
  }

  // Check if the EDATA area is enabled
  if (HAL_FLASH_ITF_OB_IsEnabledEDATAArea(HAL_FLASH) !=
      HAL_FLASH_ITF_OB_EDATA_AREA_ENABLED) {
    return exitAppTask("HAL_FLASH_ITF_OB_IsEnabledEDATAArea failed\n",
        xInputQueue);
  }

  SWD_printf("--> Press the user button to start the flash EDATA test.\n");

  uint8_t ucEvent;
  while (1) {
    if (xQueueReceive(xInputQueue, &ucEvent, portMAX_DELAY) == pdPASS) {
      processInputEvent(ucEvent);
    }
  }
}

/*
 * @brief  Initialize the application main task.
 *    When this function is called, buttons must not be pressed.
 *
 * @retval HAL_OK if the method succeeds.
 */
hal_status_t App_Init() {
  if (xTaskCreate(
      vTaskFunction,  // Function that implements the task
      "App_Task",         // Text name for the task
      256,                // Stack size in words
      NULL,               // Parameter passed into the task
      10,                 // Priority
      NULL                // Used to pass out the task's handle
      ) == pdPASS) {
    return HAL_OK;
  } else {
    return HAL_ERROR;
  }
}

