/*******************************************************************************
 * @file    m33_it.c
 * @brief   Interrupt Service Routines.
 ******************************************************************************/
#include "m33_it.h"
#include "../shared_def.h"
#include "../Debug/swd_printf.h"

/*
 * @brief This function enable specific faults for MemManage, BusFault and
 * UsageFault.
 */
void Fault_Config() {
  /* Enable specific faults for the MemManage_Handler */
  SCB->CCR |=
      SCB_CFSR_MMARVALID_Msk |
      SCB_CFSR_MSTKERR_Msk |
      SCB_CFSR_MUNSTKERR_Msk |
      SCB_CFSR_DACCVIOL_Msk |
      SCB_CFSR_IACCVIOL_Msk;

  /* Enable specific faults for the UsageFault_Handler */
  SCB->CCR |=
      SCB_CCR_DIV_0_TRP_Msk |
      SCB_CFSR_UNALIGNED_Msk |
      SCB_CFSR_NOCP_Msk |
      SCB_CFSR_INVPC_Msk |
      SCB_CFSR_INVSTATE_Msk |
      SCB_CFSR_UNDEFINSTR_Msk |
      SCB_CFSR_STKOF_Msk;

  /* Enable specific faults for the BusFault_Handler */
  SCB->CCR |=
      SCB_CFSR_BFARVALID_Msk |
      SCB_CFSR_STKERR_Msk |
      SCB_CFSR_UNSTKERR_Msk |
      SCB_CFSR_IMPRECISERR_Msk |
      SCB_CFSR_PRECISERR_Msk |
      SCB_CFSR_IBUSERR_Msk;
}

/*
 * @brief This function blinks an LED a specified number of times followed
 *   by a longer pause.
 *
 * @param ucCount The number of LED flashes (followed by a longer pause)
 */
void Flash_Fault_LED(uint8_t ucCount) {
  // Turn the LED OFF
  HAL_GPIO_WritePin(LD1_PORT, LD1_PIN, HAL_GPIO_PIN_RESET);

  for (uint8_t i = 0; i < ucCount; i++) {
    // Turn the LED ON
    HAL_GPIO_TogglePin(LD1_PORT, LD1_PIN);
    for (volatile uint32_t j = 0; j < 5000000; j++);

    // Turn the LED OFF
    HAL_GPIO_TogglePin(LD1_PORT, LD1_PIN);
    for (volatile uint32_t j = 0; j < 5000000; j++);
  }

  // Pause
  for (volatile uint32_t j = 0; j < 10000000; j++);
}

/*
 * @brief This function handles Hard fault interrupt.
 */
void HardFault_Handler(void) {
  /* Check if forced (escalated from another fault).
   * Note that if escalation is turned off, this case is not possible. */
  if (SCB->HFSR & SCB_HFSR_FORCED_Msk) {
    /* Identify which fault escalated */
    if (SCB->CFSR & 0xFF) {
      SWD_printf("HardFault_Handler: MemManage fault escalation.\n");
    } else if (SCB->CFSR & 0xFF00) {
      SWD_printf("HardFault_Handler: Bus fault escalation.\n");
    } else if (SCB->CFSR & 0xFFFF0000) {
      SWD_printf("HardFault_Handler: Usage fault escalation.\n");
    } else {
      SWD_printf("HardFault_Handler: UsageFault fault escalation %08lx.\n",
          SCB->CFSR);
    }
  } else if (SCB->HFSR & SCB_HFSR_VECTTBL_Msk) {
    SWD_printf("HardFault_Handler: Vector table read error.\n");
  } else if (SCB->HFSR & SCB_HFSR_DEBUGEVT_Msk) {
    SWD_printf("HardFault_Handler: Debug event.\n");
  } else {
    SWD_printf("HardFault_Handler: SCB->HFSR = %08lx.\n", SCB->HFSR);
  }

  while (1) {
    Flash_Fault_LED(1);
  }
}

/*
 * @brief This function handles Memory management faults.
 */
void MemManage_Handler(void) {
  /* Check specific fault causes */
  if (SCB->CFSR & SCB_CFSR_MMARVALID_Msk) {
    SWD_printf("MemManage_Handler: invalid address @0x%08lx.\n", SCB->MMFAR);
  }

  if (SCB->CFSR & SCB_CFSR_MSTKERR_Msk) {
    SWD_printf("MemManage_Handler: Stacking error.\n");
  } else if (SCB->CFSR & SCB_CFSR_MUNSTKERR_Msk) {
    SWD_printf("MemManage_Handler: Unstacking error.\n");
  } else if (SCB->CFSR & SCB_CFSR_DACCVIOL_Msk) {
    SWD_printf("MemManage_Handler: Data access violation.\n");
  } else if (SCB->CFSR & SCB_CFSR_IACCVIOL_Msk) {
    SWD_printf("MemManage_Handler: Instr. access violation.\n");
  } else {
    SWD_printf("MemManage_Handler: SCB->CFSR = %08lx.\n", SCB->CFSR);
  }

  // Clear the MemManage flags
  SCB->CFSR &= 0xFFFFFF00;

  while (1) {
    Flash_Fault_LED(2);
  }
}

/*
 * @brief This function handles prefetch, memory access and other bus faults.
 */
void BusFault_Handler(void) {
  /* Check specific fault causes */
  if (SCB->CFSR & SCB_CFSR_BFARVALID_Msk) {
    SWD_printf("BusFault_Handler: Bus fault @0x%08lx.\n", SCB->BFAR);
  }

  if (SCB->CFSR & SCB_CFSR_STKERR_Msk) {
    SWD_printf("BusFault_Handler: Bus error on stacking.\n");
  } else if (SCB->CFSR & SCB_CFSR_UNSTKERR_Msk) {
    SWD_printf("BusFault_Handler: Bus error on unstacking.\n");
  } else if (SCB->CFSR & SCB_CFSR_IMPRECISERR_Msk) {
    SWD_printf("BusFault_Handler: Imprecise bus error.\n");
  } else if (SCB->CFSR & SCB_CFSR_PRECISERR_Msk) {
    SWD_printf("BusFault_Handler: Precise bus error.\n");
  } else if (SCB->CFSR & SCB_CFSR_IBUSERR_Msk) {
    SWD_printf("BusFault_Handler: Instruction bus error.\n");
  } else {
    SWD_printf("BusFault_Handler: SCB->CFSR = %08lx.\n", SCB->CFSR);
  }

  // Clear the BusFault flags
  SCB->CFSR &= 0xFFFF00FF;

  while (1) {
    Flash_Fault_LED(3);
  }
}

/*
 * @brief This function handles undefined instructions and other illegal states
 */
void UsageFault_Handler(void) {
  /* Check specific fault causes */
  if (SCB->CFSR & SCB_CFSR_DIVBYZERO_Msk) {
    SWD_printf("UsageFault_Handler: Divide-by-zero.\n");
  } else if (SCB->CFSR & SCB_CFSR_UNALIGNED_Msk) {
    SWD_printf("UsageFault_Handler: Unaligned memory access.\n");
  } else if (SCB->CFSR & SCB_CFSR_NOCP_Msk) {
    SWD_printf("UsageFault_Handler: No co-processor.\n");
  } else if (SCB->CFSR & SCB_CFSR_INVPC_Msk) {
    SWD_printf("UsageFault_Handler: Invalid PC load.\n");
  } else if (SCB->CFSR & SCB_CFSR_INVSTATE_Msk) {
    SWD_printf("UsageFault_Handler: Invalid processor state.\n");
  } else if (SCB->CFSR & SCB_CFSR_UNDEFINSTR_Msk) {
    SWD_printf("UsageFault_Handler: Undefined instruction.\n");
  } else if (SCB->CFSR & SCB_CFSR_STKOF_Msk) {
    SWD_printf("UsageFault_Handler: Stack overflow.\n");
  } else {
    SWD_printf("UsageFault_Handler: SCB->CFSR = %08lx.\n", SCB->CFSR);
  }

  /* Clear UsageFault flags */
  SCB->CFSR &= 0x0000FFFF;

  while (1) {
    Flash_Fault_LED(4);
  }
}

/*
 * @brief This function handles Non maskable interrupt.
 */
__weak void NMI_Handler(void) {
  SWD_printf("NMI_Handler.\n");
  // Turn the LED ON.
  HAL_GPIO_WritePin(LD1_PORT, LD1_PIN, HAL_GPIO_PIN_SET);

  while (1) {
  }
}

