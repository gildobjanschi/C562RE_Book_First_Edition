/*******************************************************************************
 * file           : main.c
 * brief          : Main program body
 ******************************************************************************/
#include "mx_hal_def.h"
#include "mx_system.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/semphr.h"
#include "middleware/freertos/include/event_groups.h"
#include "middleware/freertos/include/stream_buffer.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "../../Shared/Faults/m33_it.h"
#include "aes_cbc_enc_task.h"
#include "rng_task.h"
#include "cordic_task.h"
#include "crc_task.h"
#include "sha256_integrity_task.h"
#include "aes_cbc_enc_task.h"
#include "aes_cbc_dec_task.h"

// FreeRTOS handles used by tasks
static EventGroupHandle_t xTasksEventGroup;
static SemaphoreHandle_t xPrintMutex;
static StreamBufferHandle_t xAESStreamBuffer;
// Parameter structures used by tasks
static RNG_PARAMETERS rngParams;
static CORDIC_PARAMETERS cordicParams;
static CRC_PARAMETERS crcParams;
static SHA256_INTEGRITY_PARAMETERS sha256IntegrityParams;
static AES_CBC_ENC_PARAMETERS aescbcencParams;
static AES_CBC_DEC_PARAMETERS aescbcdecParams;

/*
 * brief:  The application entry point.
 *
 * @retval: none but we specify int to comply with C99 standard
 */
int main(void) {
  SWD_Init();
#if SWD_DEBUG == RTT_DEBUG
  // SWO is configured at 144MHz and at this point the clock is set to 48MHz.
  // RTT does not require the MCU clock and therefore prints correctly
  // the message below.
  SWD_printf("---- Compute started at %lu[Hz] ----\n",
      HAL_RCC_GetHCLKFreq());
#endif

  /*
   * System Init: this code placed in targets folder initializes your system.
   * It calls the initialization (and sets the initial configuration) of the
   * peripherals. You can use STM32CubeMX to generate and call this code or
   * not in this project. It also contains the HAL initialization and the
   * initial clock configuration.
   */
  if (mx_system_init() != SYSTEM_OK) {
    ErrorHandler("mx_system_init failed.");
    return (-1);
  }

  // Configure fault handling
  Fault_Config();

  SWD_printf("---- MCU configured at %lu[Hz] ----\n", HAL_RCC_GetHCLKFreq());

  // Print the 96-bit UID of the MCU
  hal_device_uid_t deviceUID;
  if (HAL_GetDeviceUniqueID(&deviceUID) == HAL_OK) {
    SWD_printf("MCU UID:  %08x-%08x-%08x\n",
        deviceUID.uid_0, deviceUID.uid_1, deviceUID.uid_2);
    SWD_printf("-------------------\n");
  }

  // Create the print mutex
  xPrintMutex = xSemaphoreCreateMutex();
  if (xPrintMutex == NULL) {
    ErrorHandler("Cannot create mutex.\n");
    return (-1);
  }

  // Uncomment the line below and comment out the mutex creation above,
  // to turn off debug output in all the tasks
  //xPrintMutex = NULL;

  // Create the event group
  xTasksEventGroup = xEventGroupCreate();
  if (xTasksEventGroup == NULL) {
    ErrorHandler("Cannot create event group.\n");
    return (-1);
  }

  // Initialize the CRC task
  crcParams.xTasksEventGroup = xTasksEventGroup;
  crcParams.xPrintMutex = xPrintMutex;
  if (CRC_Init(&crcParams) != HAL_OK) {
    ErrorHandler("CRC_Init failed.");
    return (-1);
  }

  // Initialize the CORDIC task
  cordicParams.xTasksEventGroup = xTasksEventGroup;
  cordicParams.xPrintMutex = xPrintMutex;
  if (CORDIC_Init(&cordicParams) != HAL_OK) {
    ErrorHandler("CORDIC_Init failed.");
    return (-1);
  }

  // Initialize the RNG task
  rngParams.xTasksEventGroup = xTasksEventGroup;
  rngParams.xPrintMutex = xPrintMutex;
  if (RNG_Init(&rngParams) != HAL_OK) {
    ErrorHandler("RNG_Init failed.");
    return (-1);
  }

  // Initialize the integrity SHA256 hash task
  sha256IntegrityParams.xTasksEventGroup = xTasksEventGroup;
  sha256IntegrityParams.xPrintMutex = xPrintMutex;
  if (SHA256_Integrity_Init(&sha256IntegrityParams) != HAL_OK) {
    ErrorHandler("SHA256_Integrity_Init failed.");
    return (-1);
  }

  // Create the stream buffer that is used to send encrypted data to the decrypt
  // task. The decrypt task will be unblocked after all the 64 bytes from the
  // encrypt task have been received.
  xAESStreamBuffer = xStreamBufferCreate(64, 64);

  // Initialize the AES CBC encrypt task
  aescbcencParams.xTasksEventGroup = xTasksEventGroup;
  aescbcencParams.xAESStreamBuffer = xAESStreamBuffer;
  aescbcencParams.xPrintMutex = xPrintMutex;
  if (AES_CBC_Enc_Init(&aescbcencParams) != HAL_OK) {
    ErrorHandler("AES_CBC_Enc_Init failed.");
    return (-1);
  }

  // Initialize the AES CBC decrypt task
  aescbcdecParams.xAESStreamBuffer = xAESStreamBuffer;
  aescbcdecParams.xPrintMutex = xPrintMutex;
  if (AES_CBC_Dec_Init(&aescbcdecParams) != HAL_OK) {
    ErrorHandler("AES_CBC_Dec_Init failed.");
    return (-1);
  }

  // Set the event bits for all tasks
  xEventGroupSetBits(xTasksEventGroup, RNG_EV_GROUP_BIT | CORDIC_EV_GROUP_BIT |
      CRC_EV_GROUP_BIT | SHA256_INTEGRITY_EV_GROUP_BIT |
      AES_CBC_ENC_EV_GROUP_BIT);

  // Start the scheduler
  vTaskStartScheduler();
}
