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

/* Buffers used for transactions */
static uint32_t ControlBuffer[20];
static uint8_t I3C_RxBuffer[RX_BUFFER_SIZE];
static uint8_t I3C_TxBuffer[TX_BUFFER_SIZE];

/* Target device address for I3C communication. */
#define DEVICE_TARGET_ADDR        0x32U

/* Direct Command codes */
#define I3C_DIRECT_SETMWL_CCC     0x89
#define I3C_DIRECT_SETMRL_CCC     0x8A
#define I3C_DIRECT_GETMWL_CCC     0x8B
#define I3C_DIRECT_GETMRL_CCC     0x8C

// ---------------- CCC ----------------------
/* Descriptor array for direct I3C CCC transactions write and
   read to the target device.
 */
static hal_i3c_ccc_desc_t Direct_CCC_Descriptor[] = {
  {DEVICE_TARGET_ADDR, I3C_DIRECT_SETMWL_CCC,   2U, HAL_I3C_DIRECTION_WRITE},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_GETMWL_CCC,   2U, HAL_I3C_DIRECTION_READ},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_SETMRL_CCC,   2U, HAL_I3C_DIRECTION_WRITE},
  {DEVICE_TARGET_ADDR, I3C_DIRECT_GETMRL_CCC,   2U, HAL_I3C_DIRECTION_READ}
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
#define DIRECT_READ_DATA_SIZE     (2+2)

// Custom Command codes are in the range 0xC0 to 0xDF
#define I3C_STORE_CMD             0xC0
#define I3C_LOAD_CMD              0xC1

// ---------------- Store command ----------------------
#define STORE_CMD_TX_BYTES 4U

static hal_i3c_private_desc_t Store_CMD_Descriptor[] = {
    {DEVICE_TARGET_ADDR, STORE_CMD_TX_BYTES, HAL_I3C_DIRECTION_WRITE},
};

struct {
  uint8_t STORE_CMD_associated_data[STORE_CMD_TX_BYTES];
}

static Store_CMD = {
  .STORE_CMD_associated_data = {I3C_STORE_CMD, 0x00, 0x00, 0x00},
};

// ---------------- Store payload ----------------------
static hal_i3c_private_desc_t Store_Payload_Descriptor[] = {
    {DEVICE_TARGET_ADDR, 0, HAL_I3C_DIRECTION_WRITE},
};

// ---------------- Load command ----------------------
#define LOAD_CMD_TX_BYTES 4U

static hal_i3c_private_desc_t Load_CMD_Descriptor[] = {
    {DEVICE_TARGET_ADDR, LOAD_CMD_TX_BYTES, HAL_I3C_DIRECTION_WRITE},
};

struct {
  uint8_t LOAD_CMD_associated_data[LOAD_CMD_TX_BYTES];
}

static Load_CMD = {
  .LOAD_CMD_associated_data = {I3C_LOAD_CMD, 0x00, 0x00, 0x00},
};

// ---------------- Load payload ----------------------
static hal_i3c_private_desc_t Load_Payload_Descriptor[] = {
    {DEVICE_TARGET_ADDR, 0, HAL_I3C_DIRECTION_READ},
};
// ----------------------------------------------------

static hal_i3c_transfer_ctx_t ContextBuffers;

// Flags for I3C_State
typedef enum {
  I3C_IDLE                = 0X00000000,
  DAA_PENDING             = 0x00000001,
  DAA_COMPLETE            = 0x00000002,
  CCC_PENDING             = 0x00000004,
  STORE_CMD_PENDING       = 0x00000008,
  STORE_PAYLOAD_PENDING   = 0x00000010,
  LOAD_CMD_PENDING        = 0x00000020,
  LOAD_PAYLOAD_PENDING    = 0x00000040,
} I3C_STATE;

static I3C_STATE I3C_State;

static volatile uint64_t ulTargetProvisionedID;
static uint16_t I3C_uwLastAddress;
static uint32_t I3C_ulLastLength;

static volatile QueueHandle_t sI3CIntQueue;

/* Size of the Tx Buffer in bytes. */
#define COUNTOF(arr) (sizeof(arr) / sizeof((arr)[0]))

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

  sI3CIntQueue = I3CIntQueue;

  // Reset the state
  I3C_State = I3C_IDLE;

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
  ulTargetProvisionedID = targetPayload;
  // Start the I3C transaction that assigns the address to the target.
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
 * @brief: Perform the Direct CCC transactions.
 */
hal_status_t I3C_DirectCCCTransact() {
  hal_status_t status;
  // Reset a controller transfer context.
  status = HAL_I3C_CTRL_ResetTransferCtx(&ContextBuffers);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_ResetTransferCtx failed.\n");
    return status;
  }

  // Initialize the transfer context.
  status = HAL_I3C_CTRL_InitTransferCtxTc(&ContextBuffers, ControlBuffer,
      HAL_I3C_GET_CTRL_BUFFER_SIZE_WORD(COUNTOF(Direct_CCC_Descriptor),
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

  status = HAL_I3C_CTRL_BuildTransferCtxCCC(&ContextBuffers,
      Direct_CCC_Descriptor, COUNTOF(Direct_CCC_Descriptor),
      HAL_I3C_CCC_DIRECT_WITHOUT_DEFBYTE_RESTART);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_BuildTransferCtxCCC failed.\n");
    return status;
  }

  hal_i3c_handle_t *hI3C = mx_i3c1_gethandle();
  status = HAL_I3C_CTRL_Transfer_IT(hI3C, &ContextBuffers);
  if (status != HAL_OK) {
    SWD_printf("HAL_I3C_CTRL_Transfer_IT failed.\n");
    return status;
  }

  I3C_State |= CCC_PENDING;

  return HAL_OK;
}

/*
 * @brief: Dynamic Address Assignment complete handler
 */
hal_status_t I3C_DAAComplete() {
  SWD_printf("DAA completed.\n");

  I3C_State &= ~DAA_PENDING;
  I3C_State |= DAA_COMPLETE;
  // Configure the controller device table entry for this target.
  hal_i3c_ctrl_device_config_t DeviceConf;

  // Application target identifier.
  DeviceConf.device_index = 0;

  // Target dynamic address (assigned during ENTDAA).
  DeviceConf.tgt_dynamic_addr = DEVICE_TARGET_ADDR;

  uint32_t ulBCR = HAL_I3C_GET_BCR(ulTargetProvisionedID);
  // Determine whether to accept (ACK) its IBI requests
  DeviceConf.ibi_ack =
        (HAL_I3C_GET_IBI_CAPABLE(ulBCR) == HAL_I3C_IBI_REQ_ENABLED)
        ? HAL_I3C_CTRL_IBI_ACK_ENABLED : HAL_I3C_CTRL_IBI_ACK_DISABLED;

  // Indicate whether an IBI payload is supported/expected
  DeviceConf.ibi_payload =
        (HAL_I3C_GET_IBI_PAYLOAD(ulBCR) == HAL_I3C_IBI_PAYLOAD_ENABLED)
        ? HAL_I3C_CTRL_IBI_PAYLOAD_ENABLED : HAL_I3C_CTRL_IBI_PAYLOAD_DISABLED;

  // Controller role request capability.
  DeviceConf.ctrl_role_req_ack =
        (HAL_I3C_GET_CTRL_ROLE_CAPABLE(ulBCR) == HAL_I3C_CTRL_ROLE_ENABLED)
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

  return I3C_DirectCCCTransact();
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
 * @param type the type of transaction
 */
static hal_status_t I3C_PrivateTransact(
    hal_i3c_private_desc_t *pDesc, uint32_t ulDescCount,
    uint8_t *pTxData, uint32_t ulTxDataSize,
    uint8_t *pRxData, uint32_t ulRxDataSize,
    hal_i3c_transfer_mode_t mode, uint32_t type) {
  if(I3C_State != DAA_COMPLETE) {
    SWD_printf("I3C_PrivateTransact: %x.\n", I3C_State);
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

  I3C_State |= type;
  return HAL_OK;
}

/*
 * @brief: Send Store command
 *
 * @param uwAddress The address where data is stored
 * @param pBuf Pointer to data to send
 * @param ulLength The length of data
 */
hal_status_t I3C_StoreCmd(uint16_t uwAddress, uint8_t* pBuf,
    uint32_t ulLength) {
  SWD_printf("STORE CMD [%d bytes] @%02x: ", ulLength, uwAddress);

  // Set the address
  Store_CMD.STORE_CMD_associated_data[1] = uwAddress >> 8;
  Store_CMD.STORE_CMD_associated_data[2] = (uint8_t)uwAddress;
  // Set the length of the payload
  Store_CMD.STORE_CMD_associated_data[3] = (uint8_t)ulLength;

  memcpy(I3C_TxBuffer, pBuf, ulLength);

  I3C_uwLastAddress = uwAddress;
  I3C_ulLastLength = ulLength;

  return I3C_PrivateTransact(
      Store_CMD_Descriptor, COUNTOF(Store_CMD_Descriptor),
      (uint8_t *)&Store_CMD, STORE_CMD_TX_BYTES,
      NULL, 0, HAL_I3C_PRIVATE_WITH_ARB_STOP, STORE_CMD_PENDING);
}

/*
 * @brief: Send the Load command
 *
 * @param uwAddress The address where data is loaded from
 * @param ulLength The payload length
 */
hal_status_t I3C_LoadCmd(uint16_t uwAddress, uint32_t ulLength) {
  SWD_printf("I3C_Load_Cmd [%d bytes] @%02x\n", ulLength, uwAddress);

  // Set the address
  Load_CMD.LOAD_CMD_associated_data[1] = uwAddress >> 8;
  Load_CMD.LOAD_CMD_associated_data[2] = (uint8_t)uwAddress;
  // Set the length of the payload
  Load_CMD.LOAD_CMD_associated_data[3] = (uint8_t)ulLength;

  I3C_uwLastAddress = uwAddress;
  I3C_ulLastLength = ulLength;

  return I3C_PrivateTransact(
      Load_CMD_Descriptor, COUNTOF(Load_CMD_Descriptor),
      (uint8_t *)&Load_CMD, LOAD_CMD_TX_BYTES,
      NULL, 0, HAL_I3C_PRIVATE_WITH_ARB_STOP, LOAD_CMD_PENDING);
}

/*
 * @brief: Write payload
 */
hal_status_t I3C_WritePayload() {
  // Set the number of bytes to write
  Store_Payload_Descriptor[0].data_size_byte = I3C_ulLastLength;

  for (uint32_t i = 0; i < I3C_ulLastLength; i++) {
      SWD_printf("%02x ", I3C_TxBuffer[i]);
  }
  SWD_printf("\n");

  return I3C_PrivateTransact(
      Store_Payload_Descriptor, COUNTOF(Store_Payload_Descriptor),
      I3C_TxBuffer, I3C_ulLastLength, NULL, 0,
      HAL_I3C_PRIVATE_WITH_ARB_STOP, STORE_PAYLOAD_PENDING);
}

/*
 * @brief: Read a payload
 */
hal_status_t I3C_ReadPayload() {
  SWD_printf("I3C_ReadPayload [%d bytes] @%02x: ",
      I3C_ulLastLength, I3C_uwLastAddress);

  // Set the number of bytes to read
  Load_Payload_Descriptor[0].data_size_byte = I3C_ulLastLength;

  return I3C_PrivateTransact(
      Load_Payload_Descriptor, COUNTOF(Load_Payload_Descriptor),
      NULL, 0, I3C_RxBuffer, I3C_ulLastLength,
      HAL_I3C_PRIVATE_WITH_ARB_STOP, LOAD_PAYLOAD_PENDING);
}

/*
 * @brief: Transfer complete handler
 *
 * @param pRxBuffer
 */
hal_status_t I3C_TransferComplete(uint8_t** ppRxBuffer,
    uint32_t* pulRxByteCount) {
  *ppRxBuffer = I3C_RxBuffer;
  *pulRxByteCount = 0;

  if ((I3C_State & CCC_PENDING) == CCC_PENDING) {
    I3C_State &= ~CCC_PENDING;

    /* CCC names used by PrintCCCResults. */
    char *CommandCode[] = {
      "GETMWL",
      "GETMRL",
    };

    /* Array of bytes received for each GET* CCC above. */
    uint8_t CommandCodeSize[] = {2U, 2U};
    uint8_t ucNumCommands = (uint8_t)COUNTOF(CommandCodeSize);

    SWD_printf("Direct CCC results: ");
    uint8_t ucOffset = 0;
    for (uint8_t i = 0; i < ucNumCommands; i++) {
      SWD_printf("%s = ", CommandCode[i]);

      for (uint8_t j = 0; j < CommandCodeSize[i]; j++) {
        SWD_printf("%x", I3C_RxBuffer[ucOffset + j]);
      }
      SWD_printf("h");
      ucOffset += CommandCodeSize[i];

      if (i < (uint8_t)(ucNumCommands - 1U)) {
        SWD_printf(", ");
      }
    }

    SWD_printf(".\n");
  } else if ((I3C_State & STORE_CMD_PENDING) == STORE_CMD_PENDING) {
    I3C_State &= ~STORE_CMD_PENDING;

    I3C_WritePayload();
  } else if ((I3C_State & STORE_PAYLOAD_PENDING) == STORE_PAYLOAD_PENDING) {
    I3C_State &= ~STORE_PAYLOAD_PENDING;

    SWD_printf("STORE Payload complete\n");
  } else if ((I3C_State & LOAD_CMD_PENDING) == LOAD_CMD_PENDING) {
    I3C_State &= ~LOAD_CMD_PENDING;

    I3C_ReadPayload();
  } else if ((I3C_State & LOAD_PAYLOAD_PENDING) == LOAD_PAYLOAD_PENDING) {
    I3C_State &= ~LOAD_PAYLOAD_PENDING;

    for (uint32_t i = 0; i < I3C_ulLastLength; i++) {
        SWD_printf("%02x ", I3C_RxBuffer[i]);
    }
    SWD_printf("\n");

    *pulRxByteCount = I3C_ulLastLength;
  }

  return HAL_OK;
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

