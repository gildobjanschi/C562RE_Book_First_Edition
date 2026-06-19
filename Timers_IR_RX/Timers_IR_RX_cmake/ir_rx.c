/*******************************************************************************
 * file           : ir_rx.c
 * brief          : IR Rx implementation
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/queue.h"
#include "../../Shared/Debug/swd_printf.h"
#include "ir_rx.h"

// Burst and pauses durations
#define START_FRAME_MIN 89
#define START_FRAME_MAX 91
#define START_FRAME_PAUSE_MIN 44
#define START_FRAME_PAUSE_MAX 46
#define START_REPEAT_PAUSE_MIN 21
#define START_REPEAT_PAUSE_MAX 23
#define START_BIT_MIN 4
#define START_BIT_MAX 6
#define START_BIT_PAUSE_MIN 15
#define START_BIT_PAUSE_MAX 17

// States for the IR decoder state machine
typedef enum {
  IR_IDLE = 0,
  IR_START_FRAME = 1,
  IR_START_REPEAT_FRAME = 2,
  IR_ADDRESS = 3,
  IR_ADDRESS_PAUSE = 4,
  IR_ADDRESS_COMPL = 5,
  IR_ADDRESS_COMPL_PAUSE = 6,
  IR_COMMAND = 7,
  IR_COMMAND_PAUSE = 8,
  IR_COMMAND_COMPL = 9,
  IR_COMMAND_COMPL_PAUSE = 10
} RX_IR_STATE;

// The state of the state machine
static RX_IR_STATE ulRxIrState = IR_IDLE;

// Number of address, address complement, command and command complement
// bits received.
static uint8_t ucRxBitCounter = 0;
// The address received
static uint8_t ucRxAddress = 0;
// The address complement received
static uint8_t ucRxAddressCompl = 0;
// The command received
static uint8_t ucRxCommand = 0;
// The command complement received
static uint8_t ucRxCommandCompl = 0;

// Timer interrupt handler callback
static void Tim2UpdateCallback(hal_tim_handle_t *htim,
    hal_tim_channel_t channel);

static volatile QueueHandle_t sIrRxQueue;

/*
 * brief:  Initialize the IR receiver
 *
 * @param rxQueue The pointer to the IR Rx queue
 *
 * @retval HAL_OK if the method succeeds.
 */
hal_status_t IrRx_Init(QueueHandle_t rxQueue) {
  sIrRxQueue = rxQueue;

  hal_status_t status;
  hal_tim_handle_t *htim2 = mx_tim2_gethandle();
  status = HAL_TIM_RegisterInputCaptureCallback(htim2, Tim2UpdateCallback);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_RegisterUpdateCallback failed.\n");
    return status;
  }

  status = HAL_TIM_IC_StartChannel_IT(htim2, HAL_TIM_CHANNEL_4);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_IC_StartChannel_IT failed.\n");
    return status;
  }

  // Start the timer that performs the input capture
  status = HAL_TIM_Start(htim2);
  if (status != HAL_OK) {
    SWD_printf("HAL_TIM_Start failed.\n");
    return status;
  }

  return HAL_OK;
}

/*
 * brief:  Decode time intervals from IR receiver
 *
 * @param ucEvent The time measurement
 * @param pucAddress The pointer to the decoded address
 * @param pucCommand The pointer to the decoded command
 *
 * @retval: 1 if the decode is complete, 0 if it is in progress.
 */
uint8_t IrRx_Decode(uint8_t ucEvent, uint8_t *pucAddress, uint8_t *pucCommand) {
  SWD_printf("IRRX: %d\n", ucEvent);

  switch (ulRxIrState) {
  case IR_IDLE: {
    if (ucEvent >= START_FRAME_MIN && ucEvent <= START_FRAME_MAX) {
      ulRxIrState = IR_START_FRAME;
      ucRxBitCounter = 0;
      ucRxAddress = 0;
      ucRxAddressCompl = 0;
      ucRxCommand = 0;
      ucRxCommandCompl = 0;
      SWD_printf("IRRX: IR_IDLE -> IR_START_FRAME\n");
    } else if (ucEvent >= START_BIT_MIN && ucEvent <= START_BIT_MAX) {
      SWD_printf("IRRX: IR_IDLE [END OF MESSAGE]\n");
    } else {
      SWD_printf("IRRX: IR_IDLE -> IR_IDLE [invalid value]\n");
    }
    break;
  }

  case IR_START_FRAME: {
    if (ucEvent >= START_FRAME_MIN && ucEvent <= START_FRAME_MAX) {
      // If somehow we receive another start frame stay in this state machine.
    } else if (ucEvent >= START_FRAME_PAUSE_MIN && ucEvent <=
        START_FRAME_PAUSE_MAX) {
      ulRxIrState = IR_ADDRESS;
      SWD_printf("IRRX: IR_START_FRAME -> IR_ADDRESS\n");
    } else if (ucEvent >= START_REPEAT_PAUSE_MIN
        && ucEvent <= START_REPEAT_PAUSE_MAX) {
      ulRxIrState = IR_START_REPEAT_FRAME;
      SWD_printf("IRRX: IR_START_FRAME -> IR_START_REPEAT_FRAME\n");
    } else {
      ulRxIrState = IR_IDLE;
      SWD_printf("IRRX: IR_START_FRAME -> IR_IDLE [invalid value]\n");
    }
    break;
  }

  case IR_START_REPEAT_FRAME: {
    if (ucEvent >= START_BIT_MIN && ucEvent <= START_BIT_MAX) {
      ulRxIrState = IR_IDLE;
      SWD_printf("IRRX: IR_START_REPEAT_FRAME -> IR_IDLE [REPEAT decoded]\n");
    } else {
      ulRxIrState = IR_IDLE;
      SWD_printf("IRRX: IR_START_REPEAT_FRAME -> IR_IDLE [invalid value]\n");
    }
    break;
  }

  case IR_ADDRESS: {
    if (ucEvent >= START_BIT_MIN && ucEvent <= START_BIT_MAX) {
      ulRxIrState = IR_ADDRESS_PAUSE;
      SWD_printf("IRRX: IR_ADDRESS -> IR_ADDRESS_PAUSE\n");
    } else {
      ulRxIrState = IR_IDLE;
      SWD_printf("IRRX: IR_ADDRESS -> IR_IDLE [invalid value]\n");
    }
    break;
  }

  case IR_ADDRESS_PAUSE: {
    if (ucEvent >= START_BIT_MIN && ucEvent <= START_BIT_MAX) {
      SWD_printf("IRRX: [Bit[%d] = 0]\n", ucRxBitCounter);
    } else if (ucEvent >= START_BIT_PAUSE_MIN &&
        ucEvent <= START_BIT_PAUSE_MAX) {
      ucRxAddress |= (1 << ucRxBitCounter);
      SWD_printf("IRRX: [Bit[%d] = 1]\n", ucRxBitCounter);
    } else {
      ulRxIrState = IR_IDLE;
      SWD_printf("IRRX: IR_ADDRESS_PAUSE -> IR_IDLE [invalid value]\n");
    }

    if (ulRxIrState != IR_IDLE) {
      ucRxBitCounter++;
      if (ucRxBitCounter < 8) {
        ulRxIrState = IR_ADDRESS;
        SWD_printf("IRRX: IR_ADDRESS_PAUSE -> IR_ADDRESS\n");
      } else {
        ulRxIrState = IR_ADDRESS_COMPL;
        ucRxBitCounter = 0;
        SWD_printf("IRRX: IR_ADDRESS_PAUSE -> IR_ADDRESS_COMPL\n");
      }
    }
    break;
  }

  case IR_ADDRESS_COMPL: {
    if (ucEvent >= START_BIT_MIN && ucEvent <= START_BIT_MAX) {
      ulRxIrState = IR_ADDRESS_COMPL_PAUSE;
      SWD_printf("IRRX: IR_ADDRESS_COMPL -> IR_ADDRESS_COMPL_PAUSE\n");
    } else {
      ulRxIrState = IR_IDLE;
      SWD_printf("IRRX: IR_ADDRESS_COMPL -> IR_IDLE [invalid value]\n");
    }
    break;
  }

  case IR_ADDRESS_COMPL_PAUSE: {
    if (ucEvent >= START_BIT_MIN && ucEvent <= START_BIT_MAX) {
      SWD_printf("IRRX: [Bit[%d] = 0]\n", ucRxBitCounter);
    } else if (ucEvent >= START_BIT_PAUSE_MIN &&
        ucEvent <= START_BIT_PAUSE_MAX) {
      ucRxAddressCompl |= (1 << ucRxBitCounter);
      SWD_printf("IRRX: [Bit[%d] = 1]\n", ucRxBitCounter);
    } else {
      ulRxIrState = IR_IDLE;
      SWD_printf("IRRX: IR_ADDRESS_COMPL_PAUSE -> IR_IDLE [invalid value]\n");
    }

    if (ulRxIrState != IR_IDLE) {
      ucRxBitCounter++;
      if (ucRxBitCounter < 8) {
        ulRxIrState = IR_ADDRESS_COMPL;
        SWD_printf("IRRX: IR_ADDRESS_COMPL_PAUSE -> IR_ADDRESS_COMPL\n");
      } else {
        if ((ucRxAddress ^ ucRxAddressCompl) == 0xff) {
          ucRxBitCounter = 0;
          ulRxIrState = IR_COMMAND;
          SWD_printf("IRRX: IR_ADDRESS_COMPL_PAUSE -> IR_COMMAND\n");
        } else {
          ulRxIrState = IR_IDLE;
          SWD_printf("IRRX: IR_ADDRESS_COMPL_PAUSE -> IR_IDLE "
              "[address mismatch %x %x]\n", ucRxAddress, ucRxAddressCompl);
        }
      }
    }
    break;
  }

  case IR_COMMAND: {
    if (ucEvent >= START_BIT_MIN && ucEvent <= START_BIT_MAX) {
      ulRxIrState = IR_COMMAND_PAUSE;
      SWD_printf("IRRX: IR_COMMAND -> IR_COMMAND_PAUSE\n");
    } else {
      ulRxIrState = IR_IDLE;
      SWD_printf("IRRX: IR_COMMAND -> IR_IDLE [invalid value]\n");
    }
    break;
  }

  case IR_COMMAND_PAUSE: {
    if (ucEvent >= START_BIT_MIN && ucEvent <= START_BIT_MAX) {
      SWD_printf("IRRX: [Bit[%d] = 0]\n", ucRxBitCounter);
    } else if (ucEvent >= START_BIT_PAUSE_MIN &&
        ucEvent <= START_BIT_PAUSE_MAX) {
      ucRxCommand |= (1 << ucRxBitCounter);
      SWD_printf("IRRX: [Bit[%d] = 1]\n", ucRxBitCounter);
    } else {
      ulRxIrState = IR_IDLE;
      SWD_printf("IRRX: IR_COMMAND_PAUSE -> IR_IDLE [invalid value]\n");
    }

    if (ulRxIrState != IR_IDLE) {
      ucRxBitCounter++;
      if (ucRxBitCounter < 8) {
        ulRxIrState = IR_COMMAND;
        SWD_printf("IRRX: IR_COMMAND_PAUSE -> IR_COMMAND\n");
      } else {
        ulRxIrState = IR_COMMAND_COMPL;
        ucRxBitCounter = 0;
        SWD_printf("IRRX: IR_COMMAND_PAUSE -> IR_COMMAND_COMPL\n");
      }
    }
    break;
  }

  case IR_COMMAND_COMPL: {
    if (ucEvent >= START_BIT_MIN && ucEvent <= START_BIT_MAX) {
      ulRxIrState = IR_COMMAND_COMPL_PAUSE;
      SWD_printf("IRRX: IR_COMMAND_COMPL -> IR_COMMAND_COMPL_PAUSE\n");
    } else {
      ulRxIrState = IR_IDLE;
      SWD_printf("IRRX: IR_COMMAND_COMPL -> IR_IDLE [invalid value]\n");
    }
    break;
  }

  case IR_COMMAND_COMPL_PAUSE: {
    if (ucEvent >= START_BIT_MIN && ucEvent <= START_BIT_MAX) {
      SWD_printf("IRRX: [Bit[%d] = 0]\n", ucRxBitCounter);
    } else if (ucEvent >= START_BIT_PAUSE_MIN &&
        ucEvent <= START_BIT_PAUSE_MAX) {
      ucRxCommandCompl |= (1 << ucRxBitCounter);
      SWD_printf("IRRX: [Bit[%d] = 1]\n", ucRxBitCounter);
    } else {
      ulRxIrState = IR_IDLE;
      SWD_printf("IRRX: IR_COMMAND_COMPL_PAUSE -> IR_IDLE [invalid value]\n");
    }

    if (ulRxIrState != IR_IDLE) {
      ucRxBitCounter++;
      if (ucRxBitCounter < 8) {
        ulRxIrState = IR_COMMAND_COMPL;
        SWD_printf("IRRX: IR_COMMAND_COMPL_PAUSE -> IR_COMMAND_COMPL\n");
      } else {
        if ((ucRxCommand ^ ucRxCommandCompl) == 0xff) {
          ulRxIrState = IR_IDLE;
          SWD_printf("IRRX: IR_COMMAND_COMPL_PAUSE -> IR_IDLE "
              "[address = %x; command = %x]\n", ucRxAddress, ucRxCommand);
          *pucAddress = ucRxAddress;
          *pucCommand = ucRxCommand;
          // Decode is complete
          return 1;
        } else {
          ulRxIrState = IR_IDLE;
          SWD_printf("IRRX: IR_COMMAND_COMPL_PAUSE -> IR_IDLE "
              "[command mismatch %x %x]\n", ucRxCommand, ucRxCommandCompl);
        }
      }
    }
    break;
  }
  }

  return 0;
}

// The previous input capture value
volatile uint32_t ulIcPrevValue = 0;
// 1 if this is the first input capture value
volatile uint8_t ucIcFirstValue = 1;

/*
 * @brief  Input capture timer interrupt callback.
 *
 * @param htim Pointer to timer handle.
 * @param channel The timer channel
 */
void Tim2UpdateCallback(hal_tim_handle_t *htim, hal_tim_channel_t channel) {
  uint32_t ulIcValue =
      HAL_TIM_IC_ReadChannelCapturedValue(htim, HAL_TIM_CHANNEL_4);

  if (ucIcFirstValue == 0) {
    uint32_t ulIcDiffValue;
    if (ulIcValue > ulIcPrevValue) {
      ulIcDiffValue = ulIcValue - ulIcPrevValue;
    } else {
      ulIcDiffValue = 1000 - ulIcPrevValue + ulIcValue;
    }

    uint8_t ucEvent = (uint8_t) (ulIcDiffValue / 10);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(sIrRxQueue, &ucEvent,
        &xHigherPriorityTaskWoken) == pdPASS) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  } else {
    ucIcFirstValue = 0;
  }

  ulIcPrevValue = ulIcValue;
}

