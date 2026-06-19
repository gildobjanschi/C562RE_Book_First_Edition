/*******************************************************************************
 * @file    swd_printf.h
 * @brief   printf related definitions
 ******************************************************************************/
#ifndef SWD_PRINTF_H
#define SWD_PRINTF_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef DEBUG
/*
 * SWD_PRINTF = NO_PRINTF turns off debug message printing
 * SWD_PRINTF = SWO_PRINTF outputs debug messages over SWO
 * SWD_PRINTF = RTT_PRINTF outputs debug messages over RTT
 */
#define NO_PRINTF 0
#define SWO_PRINTF 1
#define RTT_PRINTF 2

/* Select the printf type */
#define SWD_PRINTF RTT_PRINTF

#if SWD_PRINTF == SWO_PRINTF
#include <stdio.h>
#define SWD_Init()
#define SWD_printf printf

#elif SWD_PRINTF == RTT_PRINTF
#include "../RTT/SEGGER_RTT.h"
#define SWD_Init SEGGER_RTT_Init
#define SWD_printf(format, args...) SEGGER_RTT_printf(0, format, ## args)
#else // NO_PRINTF
#define SWD_Init()
#define SWD_printf(format, args...)
#endif

#else /* DEBUG */
#define SWD_Init()
#define SWD_printf(message, args...)
#endif /* DEBUG */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SWD_PRINTF_H */
