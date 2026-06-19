/*******************************************************************************
 * file           : app_input.h
 * brief          : Header input definitions.
 ******************************************************************************/
#ifndef APP_INPUT_H
#define APP_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

hal_status_t App_Input_Init(QueueHandle_t inputQueue);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* APP_INPUT_H */
