/*******************************************************************************
 * file           : input.h
 * brief          : Header input definitions.
 ******************************************************************************/
#ifndef INPUT_H
#define INPUT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
  GESTURE_NONE = 0,   // No gesture detection
  GESTURE_BUTTON = 1, // Button gesture detection (emit CLICK/LONG CLICK)
  GESTURE_RE = 2      // Rotary encoder gesture detection (emit rotate CW/CCW)
} GESTURE_TYPE;

typedef struct input_t {
  uint8_t ucId;     // The input id
  uint8_t ucSubId;  // The input sub id (A/B) rotary encoder
  hal_gpio_t port;  // The input port
  uint32_t ulPin;   // The input pin
  volatile uint32_t ulInputLevel; // The debounced level on the input pin
  GESTURE_TYPE ulGesture;
  uint32_t ulActiveLevel;         // Valid when ulGesture != GESTURE_NONE.
  volatile uint32_t ulActiveTimestamp; // Valid when enable gesture is enabled.
  struct input_t *associatedInput; // Associated input (A/B rotary encoder)
  hal_tim_handle_t *hTim; // The timer handle used for debouncing.
  hal_exti_handle_t *hExti; // The EXTI handle used for debouncing.
} input_t;

hal_status_t Input_Init(QueueHandle_t inputQueue);
hal_status_t Input_Add(input_t *input);
void Input_Free();

// Input id
#define INPUT_ID_MASK     (uint8_t)0xf0

#define INPUT_ID_BTN_1    (uint8_t)0x00
#define INPUT_ID_RE_BTN   (uint8_t)0x10
#define INPUT_ID_RE       (uint8_t)0x20

// Events
#define EVENT_TYPE_MASK   (uint8_t)0x0f

#define EVENT_LEVEL_LOW   (uint8_t)0x00
#define EVENT_LEVEL_HIGH  (uint8_t)0x01
#define EVENT_CLICK       (uint8_t)0x02
#define EVENT_LONG_CLICK  (uint8_t)0x03
#define EVENT_RE_CCW      (uint8_t)0x04
#define EVENT_RE_CW       (uint8_t)0x05

// Input sub id
#define INPUT_SUB_ID_INVALID    (uint8_t)0x00
#define INPUT_SUB_ID_RE_A       (uint8_t)0x01
#define INPUT_SUB_ID_RE_B       (uint8_t)0x02

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* INPUT_H */
