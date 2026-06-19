/*******************************************************************************
 * file           : input_capture_dma.h
 * brief          : Input capture using DMA
 ******************************************************************************/
#ifndef INPUT_CAPTURE_DMA_H
#define INPUT_CAPTURE_DMA_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// The DMA buffer size
#define DMA_BUFFER_SIZE 5U

// This value is sent to the interrupt queue when an error occurs.
#define EVENT_ERROR         0xffff

hal_status_t Input_Capture_DMA_Init(QueueHandle_t intQueue);
hal_status_t Start_Input_Capture_DMA();
hal_status_t Stop_Input_Capture_DMA();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* INPUT_CAPTURE_DMA_H */
