/*******************************************************************************
 * @file    assert.c
 * @brief   Assert functions
 ******************************************************************************/
#include <stdint.h>
#include "swd_printf.h"

#if defined(USE_ASSERT_DBG_PARAM)
/*
 * @brief  Reports the name of the source file and the source line number
 *         where the ASSERT_DBG_PARAM error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: ASSERT_DBG_PARAM error line source number
 * @retval None
 */
void assert_dbg_param_failed(uint8_t *file, uint32_t line) {
  SWD_printf("ASSERT param: file %s on line %ld\n", file, line);

  /* Infinite loop */
  while (1) {
  }
}
#endif /* USE_ASSERT_DBG_PARAM */

#if defined(USE_ASSERT_DBG_STATE)
/*
 * @brief  Reports the name of the source file and the source line number
 *         where the ASSERT_DBG_STATE error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: ASSERT_DBG_STATE error line source number
 * @retval None
 */
void assert_dbg_state_failed(uint8_t *file, uint32_t line) {
  SWD_printf("ASSERT state: file %s on line %ld\n", file, line);

  /* Infinite loop */
  while (1) {
  }
}
#endif /* USE_ASSERT_DBG_STATE  */
