/*******************************************************************************
 * file           : main.c
 * brief          : Main program body
 ******************************************************************************/
#include "mx_hal_def.h"
#include "mx_system.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "../../Shared/Faults/m33_it.h"
#include "app_task.h"

static uint8_t *pHeap1;
static uint8_t *pHeap2;

static uint32_t initializedData1[] = {
    0x00000010, 0x00000020, 0x00000030, 0x00000040
};
static uint32_t initializedData2[] = {
    0x00000011, 0x00000021, 0x00000031, 0x00000041
};

const static uint32_t rodataData1[] = {
    0x00000010, 0x00000020, 0x00000030, 0x00000040
};
const static uint32_t rodataData2[] = {
    0x00000011, 0x00000021, 0x00000031, 0x00000041
};

static uint32_t nonInitializedData1[4];
static uint32_t nonInitializedData2[4];

/*
 * @brief:  Print the memory map
 */
static void PrintMemoryMap() {
  /* Symbols defined in the linker script */
  extern uint8_t __end__;
  extern uint8_t __stack;
  extern uint8_t __end__;
  extern uint8_t __stack;
  extern uint32_t STACK_SIZE;
  extern uint32_t HEAP_SIZE;
  extern uint8_t __bss_start__;
  extern uint8_t __bss_end__;
  extern uint8_t __data_start__;
  extern uint8_t _edata;
  extern uint8_t __rom_start__;
  extern uint8_t __rom_end__;
  extern uint8_t __rom_start__;
  extern uint8_t __vectors_end__;
  extern uint8_t __text_start__;
  extern uint8_t _etext;
  extern uint8_t __rodata_start__;
  extern uint8_t __rodata_end__;
  const uint32_t stack_limit = (uint32_t)&__stack - (uint32_t)&STACK_SIZE;

  pHeap1 = malloc(16);
  pHeap2 = malloc(16);

  SWD_printf("\n");
  SWD_printf("|------------------ RAM ------------------|\n");
  SWD_printf("|---------- Stack begin: %lx --------| v\n", stack_limit);
  const uint32_t ulStackVar1 = 1;
  SWD_printf("|--- 1st stack variable addr: %lx ---| v\n", &ulStackVar1);
  const uint32_t ulStackVar2 = 2;
  SWD_printf("|--- 2nd stack variable addr: %lx ---| v\n", &ulStackVar2);
  SWD_printf("|                    ...                  | v Stack size: %ld\n",
      &STACK_SIZE);
  SWD_printf("|--------- Stack end: %lx -----------| v\n", stack_limit);

  SWD_printf("|---------- Heap end: %lx -----------| ^\n", stack_limit);
  SWD_printf("|                    ...                  | ^ Heap size: %ld\n",
      &HEAP_SIZE);
  SWD_printf("|----- 2nd heap allocation: %lx -----| ^\n", pHeap2);
  SWD_printf("|----- 1st heap allocation: %lx -----| ^\n", pHeap1);
  SWD_printf("|---------- Heap begin: %lx ---------| ^\n", &__end__);

  SWD_printf("|----------- .bss end: %lx ----------|\n", &__bss_end__);
  SWD_printf("|                    ...                  | .bss size: %ld\n",
      ((uint32_t)&__bss_end__) - ((uint32_t)&__bss_start__));
  SWD_printf("|-- 2nd non-initialized block: %lx --|\n", nonInitializedData2);
  SWD_printf("|-- 1st non-initialized block: %lx --|\n", nonInitializedData1);
  SWD_printf("|--------- .bss begin: %lx ----------|\n", &__bss_start__);

  SWD_printf("|---------- .data end: %lx ----------|\n", &_edata);
  SWD_printf("|                    ...                  | .data size: %ld\n",
      ((uint32_t)&_edata) - ((uint32_t)&__data_start__));
  SWD_printf("|---- 2nd initialized block: %lx ----|\n", initializedData2);
  SWD_printf("|---- 1st initialized block: %lx ----|\n", initializedData1);
  SWD_printf("|--------- .data begin: %lx ---------|\n", &__data_start__);

  SWD_printf("\n");
  SWD_printf("|------------------ ROM ------------------|\n");
  SWD_printf("|----------- ROM begin: %lx ----------|\n", &__rom_start__);
  SWD_printf("|-------- .vectors begin: %lx --------|\n", &__rom_start__);
  SWD_printf("|                    ...                  | .vectors size: %ld\n",
      ((uint32_t)&__vectors_end__) - ((uint32_t)&__rom_start__));
  SWD_printf("|--------- .vectors end: %lx ---------|\n", &__vectors_end__);
  SWD_printf("|---------- .text begin: %lx ---------|\n", &__text_start__);
  SWD_printf("|                    ...                  | .text size: %ld\n",
      ((uint32_t)&_etext) - ((uint32_t)&__text_start__));
  SWD_printf("|----------- .text end: %lx ----------|\n", &_etext);
  SWD_printf("|--------- .rodata begin: %lx --------|\n", &__rodata_start__);
  SWD_printf("|------- 1st rodata block: %lx -------|\n", rodataData1);
  SWD_printf("|------- 2nd rodata block: %lx -------|\n", rodataData2);
  SWD_printf("|                    ...                  | .rodata size: %ld\n",
      ((uint32_t)&__rodata_end__) - ((uint32_t)&__rodata_start__));
  SWD_printf("|---------- .rodata end: %lx ---------|\n", &__rodata_end__);

  SWD_printf("|                    ...                  | "
      "(.preinit_array, .init_array, ...) ROM size: %ld\n",
      ((uint32_t)&__rom_end__) - ((uint32_t)&__rom_start__));
  SWD_printf("|------------ ROM end: %lx -----------|\n", &__rom_end__);
}

/*
 * @brief:  Print FreeRTOS memory stats
 */
static void PrintFreeRTOSMemoryStats() {
  uint8_t *pHeap1 = pvPortMalloc(16);
  uint8_t *pHeap2 = pvPortMalloc(16);

  SWD_printf("\n");
  SWD_printf("|--------------- FreeRTOS ----------------|\n");
  SWD_printf("|---------- Heap end: %lx -----------| ^\n", pHeap1 + 8192);
  SWD_printf("|                    ...                  | ^ Heap size: %ld\n",
      configTOTAL_HEAP_SIZE);
  SWD_printf("|----- 2nd heap allocation: %lx -----| ^\n", pHeap2);
  SWD_printf("|---------- Heap begin: %lx ---------| ^\n", pHeap1);

  // Get the FreeRTOS heap information
  HeapStats_t xHeapStats;
  vPortGetHeapStats(&xHeapStats);

  SWD_printf("\n");
  SWD_printf("Heap allocations: %ld\n",
      xHeapStats.xNumberOfSuccessfulAllocations);
  SWD_printf("Heap frees: %ld\n", xHeapStats.xNumberOfSuccessfulFrees);
  SWD_printf("Heap available: %ld\n", xHeapStats.xAvailableHeapSpaceInBytes);
  SWD_printf("Heap largest block: %ld\n",
      xHeapStats.xSizeOfLargestFreeBlockInBytes);
  SWD_printf("Heap smallest block: %ld\n",
      xHeapStats.xSizeOfSmallestFreeBlockInBytes);
}

/*
 * brief:  The application entry point.
 *
 * @retval: none but we specify int to comply with C99 standard
 */
int main(void) {
  SWD_Init();
#if SWD_PRINTF == RTT_PRINTF
  // SWO is configured at 144MHz and at this point the clock is set to 48MHz.
  // RTT does not require the MCU clock and therefore prints correctly
  // the message below.
  SWD_printf("---- Memory started at %lu[Hz] ----\n",
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

  PrintMemoryMap();

  PrintFreeRTOSMemoryStats();

  // Initialize the application
  if (App_Init() != HAL_OK) {
    ErrorHandler("App_Init failed.");
    return (-1);
  }

  // Start the scheduler
  vTaskStartScheduler();
}

