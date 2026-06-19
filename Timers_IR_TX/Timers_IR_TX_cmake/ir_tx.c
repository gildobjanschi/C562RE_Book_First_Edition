/*******************************************************************************
 * file           : ir_tx.c
 * brief          : IR Tx implementation
 ******************************************************************************/
#include "mx_hal_def.h"
#include "../../Shared/Debug/swd_printf.h"
#include "ir_tx.h"

// TIM1 counter values expressed in number of 38KHz pulses
#define COUNTER_562US		21
#define COUNTER_1687US	64
#define COUNTER_9000US	342
#define COUNTER_4500US	171

// States for the IR encoder state machine
typedef enum {
  IR_IDLE = 0,
  IR_START_FRAME = 1,
  IR_START_FRAME_PAUSE = 2,
  IR_ADDRESS = 3,
  IR_ADDRESS_PAUSE = 4,
  IR_ADDRESS_COMPL = 5,
  IR_ADDRESS_COMPL_PAUSE = 6,
  IR_COMMAND = 7,
  IR_COMMAND_PAUSE = 8,
  IR_COMMAND_COMPL = 9,
  IR_COMMAND_COMPL_PAUSE = 10,
  IR_END = 11,
  IR_END_PAUSE = 12
} TX_IR_STATE;

// The state of the state machine
static volatile TX_IR_STATE ulTxIrState = IR_IDLE;
// Number of address, address complement, command and command complement
// bits sent.
static volatile uint8_t ucTxBitCounter = 0;
// The address to send
static uint8_t ucTxAddress = 0;
// The command to send
static uint8_t ucTxCommand = 0;

// TIM1 interrupt routine
static void Tim1UpdateCallback(hal_tim_handle_t*);

// The state machine function
static void TxStateMachine();

/*
 * @brief  Initialize the IR TX
 *
 * @retval HAL_OK if it succeeds
 */
hal_status_t IrTx_Init() {
  hal_status_t status;
  // Register TIM1 interrupt handler
  status = HAL_TIM_RegisterUpdateCallback(mx_tim1_gethandle(),
		  Tim1UpdateCallback);

  return status;
}

/*
 * @brief  Send an IR frame
 *
 * @param address the IR address
 * @param command the IR command
 *
 * @retval HAL_OK if the command will be sent.
 */
hal_status_t IrTxFrame(uint8_t address, uint8_t command) {
  SWD_printf("IrTxFrame.\n");

  if (ulTxIrState != IR_IDLE) {
    SWD_printf("TX is busy. State = %d.\n", ulTxIrState);
    return HAL_BUSY;
  }

  // Store the address and command to send.
  ucTxAddress = address;
  ucTxCommand = command;

  // Start the state machine
  ulTxIrState = IR_START_FRAME;
  TxStateMachine();

  return HAL_OK;
}

#define GPIO_GET_INSTANCE(instance)  ((GPIO_TypeDef *)((uint32_t)(instance)))

/*
 * @brief  Modulate 38KHz for the specified duration.
 *
 * @param ulDuration the duration expressed in 38KHz pulses
 * @param ucStart The first time the function is called start == 1
 */
void EnableModulation(uint32_t ulDuration, uint8_t ucStart) {
  hal_tim_handle_t *htim1 = mx_tim1_gethandle();
  if (ucStart == 1) {
    // Start TIM1 which counts the number of 38KHz pulses.
    if (HAL_TIM_Start_IT(htim1) != HAL_OK) {
      SWD_printf("HAL_TIM_Start_IT failed.\n");
      return;
    }

    hal_tim_handle_t *htim5 = mx_tim5_gethandle();
    // Start TIM5 channel which operates on pin PA6.
    if (HAL_TIM_OC_StartChannel(htim5, HAL_TIM_CHANNEL_1) != HAL_OK) {
      SWD_printf("HAL_TIM_OC_StartChannel failed.\n");
      return;
    }

    // Start TIM5 which generates the 38KHz modulation.
    if (HAL_TIM_Start(htim5) != HAL_OK) {
      SWD_printf("HAL_TIM_Start failed.\n");
      return;
    }
  }

  // Set the timer counter.
  // After this many 38KHz TIM5 pulses TIM1 interrupt occurs.
  HAL_TIM_SetCounter(htim1, ulDuration);

  // Enable the output pin.
  LL_GPIO_SetPinOutputType(GPIO_GET_INSTANCE(HAL_GPIOA), HAL_GPIO_PIN_6,
      HAL_GPIO_OUTPUT_PUSHPULL);
}

/*
 * @brief  Stop the modulation for the specified duration.
 *
 * @param ulDuration the duration
 */
void DisableModulation(uint32_t ulDuration) {
  // Turn off the output pin.
  LL_GPIO_SetPinOutputType(GPIO_GET_INSTANCE(HAL_GPIOA), HAL_GPIO_PIN_6,
      HAL_GPIO_OUTPUT_OPENDRAIN);

  hal_tim_handle_t *htim1 = mx_tim1_gethandle();
  if (ulDuration == 0) {
    // Stop TIM1 that counts the 38KHz pulses
    if (HAL_TIM_Stop_IT(htim1) != HAL_OK) {
      SWD_printf("HAL_TIM_Stop_IT failed.\n");
      return;
    }

    hal_tim_handle_t *htim5 = mx_tim5_gethandle();
    // Stop the 38KHz modulation timer
    if (HAL_TIM_OC_StopChannel(htim5, HAL_TIM_CHANNEL_1) != HAL_OK) {
      SWD_printf("HAL_TIM_OC_StopChannel failed.\n");
      return;
    }

    if (HAL_TIM_Stop(htim5) != HAL_OK) {
      SWD_printf("HAL_TIM_Stop failed.\n");
      return;
    }
  } else {
    // Set the timer counter.
    // After this many 38KHz TIM5 pulses TIM1 interrupt occurs.
    HAL_TIM_SetCounter(htim1, ulDuration);
  }
}

/*
 * @brief  The IR Tx state machine.
 */
void TxStateMachine() {
  switch (ulTxIrState) {
  case IR_IDLE: {
    SWD_printf("IRTX: IR_IDLE\n");
    break;
  }

  case IR_START_FRAME: {
    EnableModulation(COUNTER_9000US, 1);

    ulTxIrState = IR_START_FRAME_PAUSE;
    SWD_printf("IRTX: IR_START_FRAME -> IR_START_FRAME_PAUSE\n");
    break;
  }

  case IR_START_FRAME_PAUSE: {
    DisableModulation(COUNTER_4500US);

    ulTxIrState = IR_ADDRESS;
    ucTxBitCounter = 0;
    SWD_printf("IRTX: IR_START_FRAME_PAUSE -> IR_ADDRESS\n");
    break;
  }

  case IR_ADDRESS: {
    EnableModulation(COUNTER_562US, 0);

    ulTxIrState = IR_ADDRESS_PAUSE;
    SWD_printf("IRTX: IR_ADDRESS -> IR_ADDRESS_PAUSE\n");
    break;
  }

  case IR_ADDRESS_PAUSE: {
    DisableModulation((ucTxAddress & (1 << ucTxBitCounter)) == 0 ?
        COUNTER_562US : COUNTER_1687US);

    ucTxBitCounter++;
    if (ucTxBitCounter >= 8) {
      ulTxIrState = IR_ADDRESS_COMPL;
      ucTxBitCounter = 0;
      SWD_printf("IRTX: IR_ADDRESS_PAUSE -> IR_ADDRESS_COMPL\n");
    } else {
      ulTxIrState = IR_ADDRESS;
      SWD_printf("IRTX: IR_ADDRESS_PAUSE -> IR_ADDRESS\n");
    }
    break;
  }

  case IR_ADDRESS_COMPL: {
    EnableModulation(COUNTER_562US, 0);

    ulTxIrState = IR_ADDRESS_COMPL_PAUSE;
    SWD_printf("IRTX: IR_ADDRESS_COMPL -> IR_ADDRESS_COMPL_PAUSE\n");
    break;
  }

  case IR_ADDRESS_COMPL_PAUSE: {
    DisableModulation((ucTxAddress & (1 << ucTxBitCounter)) == 0 ?
        COUNTER_1687US : COUNTER_562US);

    ucTxBitCounter++;
    if (ucTxBitCounter >= 8) {
      ulTxIrState = IR_COMMAND;
      ucTxBitCounter = 0;
      SWD_printf("IRTX: IR_ADDRESS_COMPL_PAUSE -> IR_COMMAND\n");
    } else {
      ulTxIrState = IR_ADDRESS_COMPL;
      SWD_printf("IRTX: IR_ADDRESS_COMPL_PAUSE -> IR_ADDRESS_COMPL\n");
    }
    break;
  }

  case IR_COMMAND: {
    EnableModulation(COUNTER_562US, 0);
    ulTxIrState = IR_COMMAND_PAUSE;
    SWD_printf("IRTX: IR_COMMAND -> IR_COMMAND_PAUSE\n");
    break;
  }

  case IR_COMMAND_PAUSE: {
    DisableModulation((ucTxCommand & (1 << ucTxBitCounter)) == 0 ?
        COUNTER_562US : COUNTER_1687US);

    ucTxBitCounter++;
    if (ucTxBitCounter >= 8) {
      ulTxIrState = IR_COMMAND_COMPL;
      ucTxBitCounter = 0;
      SWD_printf("IRTX: IR_COMMAND_PAUSE -> IR_COMMAND_COMPL\n");
    } else {
      ulTxIrState = IR_COMMAND;
      SWD_printf("IRTX: IR_COMMAND_PAUSE -> IR_COMMAND\n");
    }
    break;
  }

  case IR_COMMAND_COMPL: {
    EnableModulation(COUNTER_562US, 0);

    ulTxIrState = IR_COMMAND_COMPL_PAUSE;
    SWD_printf("IRTX: IR_COMMAND_COMPL -> IR_COMMAND_COMPL_PAUSE\n");
    break;
  }

  case IR_COMMAND_COMPL_PAUSE: {
    DisableModulation((ucTxCommand & (1 << ucTxBitCounter)) == 0 ?
        COUNTER_1687US : COUNTER_562US);

    ucTxBitCounter++;
    if (ucTxBitCounter >= 8) {
      ulTxIrState = IR_END;
      ucTxBitCounter = 0;
      SWD_printf("IRTX: IR_COMMAND_COMPL_PAUSE -> IR_END\n");
    } else {
      ulTxIrState = IR_COMMAND_COMPL;
      SWD_printf("IRTX: IR_COMMAND_COMPL_PAUSE -> IR_COMMAND_COMPL\n");
    }
    break;
  }

  case IR_END: {
    EnableModulation(COUNTER_562US, 0);
    ulTxIrState = IR_END_PAUSE;
    SWD_printf("IRTX: IR_END -> IR_END_PAUSE\n");
    break;
  }

  case IR_END_PAUSE: {
    DisableModulation(0);
    ulTxIrState = IR_IDLE;
    SWD_printf("IRTX: IR_END_PAUSE\n");
    break;
  }

  default: {
    break;
  }
  }
}

/*
 * @brief  TIM1 interrupt routine.
 * 	When the TIM1 counter reaches 0 this interrupt handler is activated.
 *
 * @param htim The handle to TIM1 timer
 */
void Tim1UpdateCallback(hal_tim_handle_t*) {
  TxStateMachine();
}
