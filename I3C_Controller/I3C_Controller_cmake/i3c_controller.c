/*******************************************************************************
 * file           : i3c_controller.c
 * brief          : I3C controller implementation
 ******************************************************************************/
#include <string.h>
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/queue.h"
#include "../../Shared/Debug/swd_printf.h"
#include "i3c_controller.h"

// Interrupt callback
static void I3C_DynAddrCompleteCallback(hal_i3c_handle_t *hi3c);
static void I3C_DynAddrRequestCallback(hal_i3c_handle_t *hi3c,
    uint64_t targetPayload);
static void I3C_ErrorCallback(hal_i3c_handle_t *hi3c);
static void I3C_TransferCompleteCallback(hal_i3c_handle_t *hi3c);

/* @user: The maximum data bus width used by DMA in STM32 devices is 64 bits.
   Therefore, 8-byte alignment is the minimum recommended alignment
   for DMA buffers across STM32 devices.
*/
#define DMA_ALIGNMENT      (8U)

__attribute__((section("non_cacheable_area"), aligned(DMA_ALIGNMENT)))
uint32_t ControlBuffer[20];

/* Size of the Rx Buffer in bytes. */
#define RX_BUFFER_SIZE             31U
__attribute__((section("non_cacheable_area"), aligned(DMA_ALIGNMENT)))
uint8_t RxBuffer[RX_BUFFER_SIZE];

/* Target device address for I3C communication. */
#define DEVICE_TARGET_ADDR        0x32U

/* Direct Command code */
#define I3C_DIRECT_SETMWL_CCC     0x89
#define I3C_DIRECT_SETMRL_CCC     0x8A
#define I3C_DIRECT_GETMWL_CCC     0x8B
#define I3C_DIRECT_GETMRL_CCC     0x8C
#define I3C_DIRECT_GETPID_CCC     0x8D
#define I3C_DIRECT_GETBCR_CCC     0x8E
#define I3C_DIRECT_GETDCR_CCC     0x8F
#define I3C_DIRECT_GETSTATUS_CCC  0x90

// ---------------- CCC ----------------------
/* Descriptor array for direct I3C CCC transactions write and read to the
 * target device */
static hal_i3c_ccc_desc_t DirectWriteRead_CCC_Descriptor[8] = {
  {DEVICE_TARGET_ADDR, I3C_DIRECT_SETMWL_CCC,   2U, HAL_I3C_DIRECTION_WRITE},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_GETMWL_CCC,   2U, HAL_I3C_DIRECTION_READ},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_SETMRL_CCC,   2U, HAL_I3C_DIRECTION_WRITE},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_GETMRL_CCC,   2U, HAL_I3C_DIRECTION_READ},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_GETPID_CCC,   6U, HAL_I3C_DIRECTION_READ},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_GETBCR_CCC,   1U, HAL_I3C_DIRECTION_READ},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_GETDCR_CCC,   1U, HAL_I3C_DIRECTION_READ},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_GETSTATUS_CCC,   1U, HAL_I3C_DIRECTION_READ}
};

// Structure holding associated data for SETMRL and SETMWL CCC write command
struct {
  uint8_t SETMRL_associated_data[2];
  uint8_t SETMWL_associated_data[2];
}

DirectWriteCCC = {
  .SETMRL_associated_data = {0x0, 0x8},
  .SETMWL_associated_data = {0x0, 0x8}
};

// DirectWrite CCC payload size: 2 bytes (SETMRL data) + 2 bytes (SETMWL data)
#define DIRECT_WRITE_CCC_SIZE     (2+2)
// Size of the data to be received from the target device.
// Sum up all the read bytes from the DirectWriteRead_CCC_Descriptor.
#define DIRECT_READ_DATA_SIZE       13U

static hal_i3c_transfer_ctx_t ContextBuffers;

// Flags for I3C_State
#define DAA_PENDING           0x00000001
#define DAA_COMPLETE          0x00000002
#define TRANSACT_CCC_PENDING  0x00000004

static volatile uint64_t ulTarget_bcr_dcr_pid;
static uint32_t I3C_State;

static volatile QueueHandle_t sI3CIntQueue;

/* Size of the Tx Buffer in bytes. */
#define COUNTOF(arr) (sizeof(arr) / sizeof((arr)[0]))

static void PrintCCCResults(uint8_t *RxBuffer);
/*
 * @brief:  Initialize I3C
 *
 * @param I3CIntQueue The queue for notifying of interrupt events
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t I3C_Init(QueueHandle_t I3CIntQueue) {
  hal_status_t status;
  hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();

  // Register the controller transfer complete callback.
  status = HAL_I3C_CTRL_RegisterTransferCpltCallback(hI3C,
      I3C_TransferCompleteCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_RegisterTransferCpltCallback failed.\n");
    return status;
  }

  // Register the controller dynamic address assignment complete callback.
  status = HAL_I3C_CTRL_RegisterDAACpltCallback(hI3C,
      I3C_DynAddrCompleteCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_RegisterDAACpltCallback failed.\n");
    return status;
  }

  // Register the target request dynamic address callback.
  status = HAL_I3C_CTRL_RegisterTgtReqDynAddrCallback(hI3C,
      I3C_DynAddrRequestCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_RegisterTgtReqDynAddrCallback failed.\n");
    return status;
  }

  // Register the error callback.
  status = HAL_I3C_RegisterErrorCallback(hI3C, I3C_ErrorCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_RegisterErrorCallback failed.\n");
    return status;
  }

  // Prepare the context

  // Reset a controller transfer context.
  status = HAL_I3C_CTRL_ResetTransferCtx(&ContextBuffers);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_ResetTransferCtx failed.\n");
    return status;
  }

  // Initialize the transfer context.
  status = HAL_I3C_CTRL_InitTransferCtxTc(&ContextBuffers, ControlBuffer,
      HAL_I3C_GET_CTRL_BUFFER_SIZE_WORD(COUNTOF(DirectWriteRead_CCC_Descriptor),
      HAL_I3C_CCC_DIRECT_WITHOUT_DEFBYTE_RESTART));
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_InitTransferCtxTc failed.\n");
    return status;
  }

  // Initialize the transfer context with Tx data.
  status = HAL_I3C_CTRL_InitTransferCtxTx(&ContextBuffers,
      (uint8_t *)&DirectWriteCCC, DIRECT_WRITE_CCC_SIZE);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_InitTransferCtxTx failed.\n");
    return status;
  }

  // Initialize the transfer context with Rx data.
  status = HAL_I3C_CTRL_InitTransferCtxRx(&ContextBuffers,
      RxBuffer, DIRECT_READ_DATA_SIZE);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_InitTransferCtxRx failed.\n");
    return status;
  }

  sI3CIntQueue = I3CIntQueue;

  // Reset the state
  I3C_State = 0;

  return HAL_OK;
}

/*
 * brief: I3C start operation
 */
hal_status_t I3C_StartDAA() {
  if ((I3C_State & DAA_PENDING) == DAA_PENDING) {
    SWD_printf("I3C_StartDAA: DAA already pending.\n");
    return HAL_BUSY;
  }

  /* Initiate Dynamic Address Assignment (DAA) process for the controller */
  hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();
  hal_status_t status;
  status = HAL_I3C_CTRL_DynAddrAssign_IT(hI3C,
      HAL_I3C_DYN_ADDR_RSTDAA_THEN_ENTDAA);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_DynAddrAssign_IT failed.\n");
    return status;
  }

  I3C_State |= DAA_PENDING;
  return HAL_OK;
}

/*
 * @brief: Dynamic Address Assignment target request IRQ callback
 *
 * @param hi3c The I3C handle
 * @param targetPayload The target payload
 */
static void I3C_DynAddrRequestCallback(hal_i3c_handle_t *hi3c,
    uint64_t targetPayload) {
  SWD_printf("-- Target requested DA; payload: %x\n", targetPayload);
  ulTarget_bcr_dcr_pid = targetPayload;
  HAL_I3C_CTRL_SetDynAddr(hi3c, DEVICE_TARGET_ADDR);
}

/*
 * @brief: Dynamic Address Assignment complete IRQ callback
 *
 * @param hi3c The I3C handle
 */
static void I3C_DynAddrCompleteCallback(hal_i3c_handle_t *hi3c) {
  uint8_t ucEvent = EVENT_DAA_COMPLETE;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sI3CIntQueue, &ucEvent,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * @brief: Dynamic Address Assignment complete handler
 */
hal_status_t I3C_DAAComplete() {
  SWD_printf("DAA completed.\n");

  I3C_State &= ~DAA_PENDING;
  I3C_State |= DAA_COMPLETE;
  // Configure the controller device table entry for this target.
  // This per-target configuration is required to handle IBI correctly:
  // - identify the target (device index + dynamic address)
  // - determine whether to accept (ACK) its IBI requests
  // - indicate whether an IBI payload is supported/expected
  uint32_t bcr = HAL_I3C_GET_BCR(ulTarget_bcr_dcr_pid);
  hal_i3c_ctrl_device_config_t DeviceConf;

  // Application target identifier.
  DeviceConf.device_index = 0;

  // Target dynamic address (assigned during ENTDAA).
  DeviceConf.tgt_dynamic_addr = DEVICE_TARGET_ADDR;

  // IBI capability (from BCR).
  DeviceConf.ibi_ack =
        (HAL_I3C_GET_IBI_CAPABLE(bcr) == HAL_I3C_IBI_REQ_ENABLED)
        ? HAL_I3C_CTRL_IBI_ACK_ENABLED : HAL_I3C_CTRL_IBI_ACK_DISABLED;

  // IBI payload capability (from BCR).
  DeviceConf.ibi_payload =
        (HAL_I3C_GET_IBI_PAYLOAD(bcr) == HAL_I3C_IBI_PAYLOAD_ENABLED)
        ? HAL_I3C_CTRL_IBI_PAYLOAD_ENABLED : HAL_I3C_CTRL_IBI_PAYLOAD_DISABLED;

  // Controller role request capability (from BCR).
  DeviceConf.ctrl_role_req_ack =
        (HAL_I3C_GET_CTRL_ROLE_CAPABLE(bcr) == HAL_I3C_CTRL_ROLE_ENABLED)
        ? HAL_I3C_CTRL_ROLE_ACK_ENABLED : HAL_I3C_CTRL_ROLE_ACK_DISABLED;

  // No forced STOP for this target.
  DeviceConf.ctrl_stop_transfer = HAL_I3C_CTRL_STOP_TRANSFER_DISABLED;

  hal_status_t status;
  hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();
  status = HAL_I3C_CTRL_SetConfigBusDevices(hI3C, &DeviceConf, 1U);
  if (status != HAL_OK) {
    SWD_printf("I3C_DAAComplete: HAL_I3C_CTRL_SetConfigBusDevices failed.\n");
    return status;
  }

  // Transmits in DMA mode the DirectWriteCCC, which contains fixed-length
  // data arrays, and receives another fixed-length data array buffer,
  // both using I3C in DMA mode.
  status = HAL_I3C_CTRL_BuildTransferCtxCCC(&ContextBuffers,
      DirectWriteRead_CCC_Descriptor, COUNTOF(DirectWriteRead_CCC_Descriptor),
      HAL_I3C_CCC_DIRECT_WITHOUT_DEFBYTE_RESTART);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_BuildTransferCtxCCC failed.\n");
    return status;
  }

  status = HAL_I3C_CTRL_Transfer_DMA(hI3C, &ContextBuffers);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_Transfer_DMA failed.\n");
    return status;
  }

  I3C_State |= TRANSACT_CCC_PENDING;

  return HAL_OK;
}

/*
 * @brief: Transfer complete IRQ callback
 *
 * @param hi3c The I3C handle
 */
static void I3C_TransferCompleteCallback(hal_i3c_handle_t *hi3c) {
  SWD_printf("-- Transfer complete: %d bytes.\n", hi3c->data_size_byte);
  uint8_t ucEvent = EVENT_TRANSFER_COMPLETE;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sI3CIntQueue, &ucEvent,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * @brief: Transfer complete handler
 */
hal_status_t I3C_TransferComplete() {
  if ((I3C_State & TRANSACT_CCC_PENDING) == TRANSACT_CCC_PENDING) {
    I3C_State &= ~TRANSACT_CCC_PENDING;

    PrintCCCResults(RxBuffer);
  }

  return HAL_OK;
}

/*
 * @brief: I3C error callback.
 *
 * @param hi3c The I3C handle
 */
static void I3C_ErrorCallback(hal_i3c_handle_t *hi3c) {
  SWD_printf("-- Error callback codes: %01x\n", hi3c->last_error_codes);

  uint8_t ucEvent = EVENT_ERROR;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sI3CIntQueue, &ucEvent,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * @brief: Prints the results of received CCC command responses.
 *
 * @param RxBuffer Pointer to the buffer containing received data.
 */
static void PrintCCCResults(uint8_t *RxBuffer) {
  /* CCC names used by PrintCCCResults() (trace only). */
  char *CommandCode[] = {
    "GETMWL",
    "GETMRL",
    "GETPID",
    "GETBCR",
    "GETDCR",
    "GETSTATUS"
  };

  /* Bytes received for each GET* CCC above (for debug message only).
   * Sum must match DATA_SIZE. */
  uint8_t CommandCodeSize[] = {2U, 2U, 6U, 1U, 1U, 1U};
  uint8_t numCommands = (uint8_t)COUNTOF(CommandCodeSize);

  uint8_t offset = 0;
  for (uint8_t i = 0; i < numCommands; i++) {
    SWD_printf("%s = 0x", CommandCode[i]);

    for (uint8_t j = 0; j < CommandCodeSize[i]; j++) {
      SWD_printf("%01x", RxBuffer[offset + j]);
    }

    offset += CommandCodeSize[i];

    if (i < (uint8_t)(numCommands - 1U)) {
      SWD_printf(", ");
    }
  }

  SWD_printf(".\n");
}
