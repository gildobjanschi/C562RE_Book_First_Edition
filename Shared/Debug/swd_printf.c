/*******************************************************************************
 * @file    swd_printf.c
 * @brief   printf related code
 ******************************************************************************/

#ifdef DEBUG

#if SWD_PRINTF == SWO_PRINTF
#include "swd_printf.h"
#include "../shared_def.h"

int _write(int file, char *ptr, int len) {
  (void)file;

  for (int DataIdx = 0; DataIdx < len; DataIdx++) {
  	ITM_SendChar(*ptr++);
  }
  return len;
}


#endif // SWD_PRINTF
#endif // DEBUG
