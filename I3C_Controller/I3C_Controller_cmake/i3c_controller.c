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

uint32_t ControlBuffer[20];

/* Size of the Rx Buffer in bytes. */
#define RX_BUFFER_SIZE  32U
uint8_t I3C_RxBuffer[RX_BUFFER_SIZE];

/* Size of the Tx Buffer in bytes. */
#define TX_BUFFER_SIZE  32U

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
/* Descriptor array for direct I3C CCC transactions write and
   read to the target device
 */
static hal_i3c_ccc_desc_t DirectWriteRead_CCC_Descriptor[8] = {
  {DEVICE_TARGET_ADDR, I3C_DIRECT_SETMWL_CCC,   2U, HAL_I3C_DIRECTION_WRITE},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_GETMWL_CCC,   2U, HAL_I3C_DIRECTION_READ},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_SETMRL_CCC,   2U, HAL_I3C_DIRECTION_WRITE},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_GETMRL_CCC,   2U, HAL_I3C_DIRECTION_READ},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_GETPID_CCC,   6U, HAL_I3C_DIRECTION_READ},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_GETBCR_CCC,   1U, HAL_I3C_DIRECTION_READ},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_GETDCR_CCC,   1U, HAL_I3C_DIRECTION_READ},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_GETSTATUS_CCC,1U, HAL_I3C_DIRECTION_READ}
};

// Structure holding associated data for SETMRL and SETMWL CCC write command
struct {
  uint8_t SETMRL_associated_data[2];
  uint8_t SETMWL_associated_data[2];
}

DirectWriteCCC = {
  .SETMRL_associated_data = {0x0, RX_BUFFER_SIZE},
  .SETMWL_associated_data = {0x0, TX_BUFFER_SIZE}
};

// DirectWrite CCC payload size: 2 bytes (SETMRL data) + 2 bytes (SETMWL data)
#define DIRECT_WRITE_CCC_SIZE     (2+2)
// Size of the data to be received from the target device.
// Sum up all the read bytes from the DirectWriteRead_CCC_Descriptor.
#define DIRECT_READ_DATA_SIZE     13U

// Custom Command codes are in the range 0xC0 to 0xDF
#define I3C_STORE_CMD             0xC0
#define I3C_LOAD_CMD              0xC1
#define I3C_STALL_CMD             0xC2

// ---------------- Store command ----------------------
#define STORE_CMD_TX_BYTES 4U

static hal_i3c_private_desc_t Store_Private_Descriptor[2] = {
    {DEVICE_TARGET_ADDR, STORE_CMD_TX_BYTES, HAL_I3C_DIRECTION_WRITE},
    {DEVICE_TARGET_ADDR, 0U, HAL_I3C_DIRECTION_WRITE},
};

struct {
  uint8_t STORE_CMD_associated_data[STORE_CMD_TX_BYTES];
  uint8_t STORE_PAYLOAD_associated_data[TX_BUFFER_SIZE];
}

static Store_CMD = {
  .STORE_CMD_associated_data = {I3C_STORE_CMD, 0x00, 0x00, 0x00},
  .STORE_PAYLOAD_associated_data = {0x00}
};

// ---------------- Load command ----------------------
#define LOAD_CMD_TX_BYTES 4U
#define LOAD_STALL_TX_BYTES 16U // Maximum value: TX_BUFFER_SIZE

static hal_i3c_private_desc_t Load_Private_Descriptor[3] = {
    {DEVICE_TARGET_ADDR, LOAD_CMD_TX_BYTES, HAL_I3C_DIRECTION_WRITE},
    {DEVICE_TARGET_ADDR, LOAD_STALL_TX_BYTES, HAL_I3C_DIRECTION_WRITE},
    {DEVICE_TARGET_ADDR, 0, HAL_I3C_DIRECTION_READ},
};

struct {
  uint8_t LOAD_CMD_associated_data[LOAD_CMD_TX_BYTES];
  uint8_t STALL_CMD_associated_data[TX_BUFFER_SIZE];
}

static Load_CMD = {
  .LOAD_CMD_associated_data = {I3C_LOAD_CMD, 0x00, 0x00, 0x00},
  .STALL_CMD_associated_data = {0x00},
};

static hal_i3c_transfer_ctx_t ContextBuffers;

// Flags for I3C_State
#define DAA_PENDING               0x00000001
#define DAA_COMPLETE              0x00000002
#define TRANSACT_CCC_PENDING      0x00000004
#define TRANSACT_PRIVATE_PENDING  0x00000008

static volatile uint64_t ulTarget_bcr_dcr_pid;
static uint32_t I3C_State;
static uint8_t I3C_ucLastCommand;
static uint16_t I3C_uwLastAddress;
static uint32_t I3C_ulLastLength;

static volatile QueueHandle_t sI3CIntQueue;

/* Size of the Tx Buffer in bytes. */
#define COUNTOF(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Forward function declarations */
static void PrintCCCResults();
static void PrintCommandResults();
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
      I3C_RxBuffer, DIRECT_READ_DATA_SIZE);
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
 * @brief: Check if Dynamic Address Assignment has completed
 */
hal_status_t I3C_IsDAACompleted() {
  return (I3C_State & DAA_COMPLETE) == DAA_COMPLETE ? HAL_OK :  HAL_ERROR;
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

  status = HAL_I3C_CTRL_BuildTransferCtxCCC(&ContextBuffers,
      DirectWriteRead_CCC_Descriptor, COUNTOF(DirectWriteRead_CCC_Descriptor),
      HAL_I3C_CCC_DIRECT_WITHOUT_DEFBYTE_RESTART);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_BuildTransferCtxCCC failed.\n");
    return status;
  }

  status = HAL_I3C_CTRL_Transfer_IT(hI3C, &ContextBuffers);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_Transfer_IT failed.\n");
    return status;
  }

  I3C_State |= TRANSACT_CCC_PENDING;

  return HAL_OK;
}

/*
 * @brief: Start I3C transaction
 *
 * @param pDesc The descriptor array
 * @param ulDescCount Number of descriptors in the array
 * @param pTxData Tx Data
 * @param ulTxDataSize TxData size
 * @param pRxData Rx Data
 * @param ulRxDataSize RxData size
 * @param mode The I3C mode
 */
static hal_status_t I3C_Private_Transact(
    hal_i3c_private_desc_t *pDesc, uint32_t ulDescCount,
    uint8_t *pTxData, uint32_t ulTxDataSize,
    uint8_t *pRxData, uint32_t ulRxDataSize,
    hal_i3c_transfer_mode_t mode) {
  if((I3C_State & DAA_COMPLETE) != DAA_COMPLETE) {
    SWD_printf("I3C_Private_Transact: DAA not complete.\n");
    return HAL_ERROR;
  } else if ((I3C_State & TRANSACT_CCC_PENDING) == TRANSACT_CCC_PENDING) {
    SWD_printf("I3C_Private_Transact: CCC transaction pending.\n");
    return HAL_BUSY;
  } else if ((I3C_State & TRANSACT_PRIVATE_PENDING) ==
      TRANSACT_PRIVATE_PENDING) {
    SWD_printf("I3C_Private_Transact: Private transaction pending.\n");
    return HAL_BUSY;
  }

  hal_status_t status;

  // Reset a controller transfer context.
  status = HAL_I3C_CTRL_ResetTransferCtx(&ContextBuffers);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_ResetTransferCtx failed.\n");
    return status;
  }

  // Initialize the transfer context with pointer to
  // Transmit Control (TC) descriptor words buffer.
  status = HAL_I3C_CTRL_InitTransferCtxTc(&ContextBuffers, ControlBuffer,
      HAL_I3C_GET_CTRL_BUFFER_SIZE_WORD(ulDescCount, mode));
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_InitTransferCtxTc failed.\n");
    return status;
  }

  if (ulTxDataSize > 0) {
    // Initialize the transfer context with Tx data.
    status = HAL_I3C_CTRL_InitTransferCtxTx(&ContextBuffers, pTxData,
        ulTxDataSize);
    if (status != HAL_OK) {
      SWD_printf("HAL_I3C_CTRL_InitTransferCtxTx failed.\n");
      return status;
    }
  }

  if (ulRxDataSize > 0) {
    // Initialize the transfer context with Rx data.
    status = HAL_I3C_CTRL_InitTransferCtxRx(&ContextBuffers, pRxData,
        ulRxDataSize);
    if (status != HAL_OK) {
      SWD_printf("HAL_I3C_CTRL_InitTransferCtxRx failed.\n");
      return status;
    }
  }

  status = HAL_I3C_CTRL_BuildTransferCtxPrivate(&ContextBuffers,
      pDesc, ulDescCount, mode);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_BuildTransferCtxPrivate failed.\n");
    return status;
  }

  hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();
  status = HAL_I3C_CTRL_Transfer_IT(hI3C, &ContextBuffers);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_Transfer_IT failed.\n");
    return status;
  }

  I3C_State |= TRANSACT_PRIVATE_PENDING;
  return HAL_OK;
}

/*
 * @brief: Transfer complete handler
 */
hal_status_t I3C_TransferComplete() {
  if ((I3C_State & TRANSACT_CCC_PENDING) == TRANSACT_CCC_PENDING) {
    I3C_State &= ~TRANSACT_CCC_PENDING;

    PrintCCCResults();
  } else if ((I3C_State & TRANSACT_PRIVATE_PENDING) ==
      TRANSACT_PRIVATE_PENDING) {
    I3C_State &= ~TRANSACT_PRIVATE_PENDING;

    PrintCommandResults();
  }

  return HAL_OK;
}

/*
 * @brief: Store data from Store_CMD.STORE_PAYLOAD_associated_data
 *
 * @param uwAddress The address where data is stored
 * @param ulLength The length of data
 */
hal_status_t I3C_StoreData(uint16_t uwAddress, uint32_t ulLength) {
  if((I3C_State & DAA_COMPLETE) != DAA_COMPLETE) {
    SWD_printf("I3C_StoreData: DAA not complete.\n");
    return HAL_ERROR;
  } else if ((I3C_State & TRANSACT_CCC_PENDING) == TRANSACT_CCC_PENDING) {
    SWD_printf("I3C_StoreData: CCC transaction pending.\n");
    return HAL_BUSY;
  } else if ((I3C_State & TRANSACT_PRIVATE_PENDING) ==
      TRANSACT_PRIVATE_PENDING) {
    SWD_printf("I3C_StoreData: Private transaction pending.\n");
    return HAL_BUSY;
  }

  // Set the address
  Store_CMD.STORE_CMD_associated_data[1] = uwAddress >> 8;
  Store_CMD.STORE_CMD_associated_data[2] = (uint8_t)uwAddress;
  // Set the length of the payload
  Store_CMD.STORE_CMD_associated_data[3] = (uint8_t)ulLength;

  // Specify how many bytes will be stored.
  Store_Private_Descriptor[1].data_size_byte = ulLength;

  I3C_ucLastCommand = I3C_STORE_CMD;
  I3C_uwLastAddress = uwAddress;
  I3C_ulLastLength = ulLength;

  SWD_printf("STORE CMD [%d bytes] @%02x: ",
      I3C_ulLastLength, I3C_uwLastAddress);
  for (uint32_t i = 0; i < I3C_ulLastLength; i++) {
    SWD_printf("%02x ", Store_CMD.STORE_PAYLOAD_associated_data[i]);
  }
  SWD_printf("\n");

  return I3C_Private_Transact(
      Store_Private_Descriptor, COUNTOF(Store_Private_Descriptor),
      (uint8_t *)&Store_CMD, STORE_CMD_TX_BYTES + ulLength,
      NULL, 0, HAL_I3C_PRIVATE_WITH_ARB_STOP);
}

/*
 * @brief: Load data into RxBuffer (maximum RX_BUFFER_SIZE)
 *
 * @param uwAddress The address where data is loaded from
 * @param ulLength The length of data
 */
hal_status_t I3C_LoadData(uint16_t uwAddress, uint32_t ulLength) {
  if((I3C_State & DAA_COMPLETE) != DAA_COMPLETE) {
    SWD_printf("I3C_LoadData: DAA not complete.\n");
    return HAL_ERROR;
  } else if ((I3C_State & TRANSACT_CCC_PENDING) == TRANSACT_CCC_PENDING) {
    SWD_printf("I3C_LoadData: CCC transaction pending.\n");
    return HAL_BUSY;
  } else if ((I3C_State & TRANSACT_PRIVATE_PENDING) ==
      TRANSACT_PRIVATE_PENDING) {
    SWD_printf("I3C_LoadData: Private transaction pending.\n");
    return HAL_BUSY;
  }

  SWD_printf("I3C_LoadData %d bytes @%02x\n", ulLength, uwAddress);

  // Set the address
  Load_CMD.LOAD_CMD_associated_data[1] = uwAddress >> 8;
  Load_CMD.LOAD_CMD_associated_data[2] = (uint8_t)uwAddress;
  // Set the length of the payload
  Load_CMD.LOAD_CMD_associated_data[3] = (uint8_t)ulLength;

  // Specify how many bytes will be loaded.
  Load_Private_Descriptor[2].data_size_byte = ulLength;

  I3C_ucLastCommand = I3C_LOAD_CMD;
  I3C_uwLastAddress = uwAddress;
  I3C_ulLastLength = ulLength;

  return I3C_Private_Transact(
      Load_Private_Descriptor, COUNTOF(Load_Private_Descriptor),
      (uint8_t *)&Load_CMD, LOAD_CMD_TX_BYTES + LOAD_STALL_TX_BYTES,
      I3C_RxBuffer, ulLength, HAL_I3C_PRIVATE_WITH_ARB_STOP);
}

/*
 * @brief: Transfer complete IRQ callback
 *
 * @param hi3c The I3C handle
 */
static void I3C_TransferCompleteCallback(hal_i3c_handle_t *hi3c) {
  //SWD_printf("-- Transfer complete.\n");
  uint8_t ucEvent = EVENT_TRANSFER_COMPLETE;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sI3CIntQueue, &ucEvent,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * @brief: I3C error callback.
 *
 * @param hi3c The I3C handle
 */
static void I3C_ErrorCallback(hal_i3c_handle_t *hi3c) {
  SWD_printf("-- Error callback codes: %x\n", hi3c->last_error_codes);

  uint8_t ucEvent = EVENT_ERROR;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(sI3CIntQueue, &ucEvent,
      &xHigherPriorityTaskWoken) == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/*
 * @brief: Prints the results of received CCC command responses.
 */
static void PrintCCCResults() {
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
      SWD_printf("%01x", I3C_RxBuffer[offset + j]);
    }

    offset += CommandCodeSize[i];

    if (i < (uint8_t)(numCommands - 1U)) {
      SWD_printf(", ");
    }
  }

  SWD_printf(".\n");
}

/*
 * @brief: Prints the command results
 */
static void PrintCommandResults() {
  switch (I3C_ucLastCommand) {
  case I3C_STORE_CMD: {
    SWD_printf("STORE CMD complete\n");
    break;
  }

  case I3C_LOAD_CMD: {
    SWD_printf("LOAD CMD [%d bytes] @%02x: ",
        I3C_ulLastLength, I3C_uwLastAddress);
    for (uint32_t i = 0; i < I3C_ulLastLength; i++) {
      SWD_printf("%02x ", I3C_RxBuffer[i]);
    }
    SWD_printf("\n");
    break;
  }

  default: {
    SWD_printf("Unhandled CMD: %02x, address: %02x, length: %d\n",
        I3C_ucLastCommand, I3C_uwLastAddress, I3C_ulLastLength);
    break;
  }
  }
}
