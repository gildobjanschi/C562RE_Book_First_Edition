/*******************************************************************************
 * file           : app_task.c
 * brief          : The application task implementation.
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/queue.h"
#include "middleware/freertos/include/semphr.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "../../Shared/Input/input.h"
#include "app_input.h"
#include "app_task.h"

// ----- WDG modes ------
#define MODE_IWDG   1
#define MODE_WWDG   2

// Select which WDG mode to use
#define MODE_SEL MODE_WWDG

// ----- Interrupt signal modes ------
#define MODE_SEMAPHORE   1
#define MODE_QUEUE       2

// Select which interrupt signaling mode to use
#define INT_SIGNAL MODE_SEMAPHORE

#if (INT_SIGNAL == MODE_SEMAPHORE)
static SemaphoreHandle_t xWDG_Semaphore;
#elif (INT_SIGNAL == MODE_QUEUE)
static QueueHandle_t xIntQueue;
#endif

// ----- Interrupt callbacks ------
#if (MODE_SEL == MODE_IWDG)
#define EARLY_INT_TIME  8000UL
#define MIN_TIME        2000UL
#define MAX_TIME        10000UL

/*
 * @brief:  The early IWDG wakeup callback
 *
 * @param hiwdg The handle of the IWDG
 */
static void IWDG_EarlyWakeupCallback(hal_iwdg_handle_t *hiwdg) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  __asm__ volatile ("sev": : :"memory");
#if (INT_SIGNAL == MODE_SEMAPHORE)
  xSemaphoreGiveFromISR(xWDG_Semaphore, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#elif (INT_SIGNAL == MODE_QUEUE)
  uint8_t ucEvent = 1;
  if (xQueueSendFromISR(xIntQueue, &ucEvent, &xHigherPriorityTaskWoken)
      == pdPASS) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
#endif // INT_SIGNAL
}
#elif (MODE_SEL == MODE_WWDG)
/*
 * @brief:  The early WWDG wakeup callback
 *
 * @param hwwdg The handle of the WWDG
 */
static void WWDG_EarlyWakeupCallback(hal_wwdg_handle_t *hwwdg) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(xWDG_Semaphore, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
#endif

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 * @param xInputQueue The input queue
 * @param xIntSignal The WDG semaphore or queue
 */
static void exitAppTask(char *error, QueueHandle_t xInputQueue,
    QueueSetMemberHandle_t xIntSignal) {
  ErrorHandler(error);

  // Release the input handling
  Input_Free();

  // Free the input queue
  if (xInputQueue != NULL) {
    vQueueDelete(xInputQueue);
  }

  // Free the semaphore or the queue
  if (xIntSignal != NULL) {
    if (uxQueueGetQueueItemSize(xIntSignal) == 0) {
      vSemaphoreDelete(xIntSignal);
    } else {
      vQueueDelete(xIntSignal);
    }
  }

  vTaskDelete(NULL);
}

#define INPUT_QUEUE_SIZE  8
#if (INT_SIGNAL == MODE_SEMAPHORE)
  #define INT_SIGNAL_SIZE    1
#elif (INT_SIGNAL == MODE_QUEUE)
  #define INT_SIGNAL_SIZE    4
#endif

#if (MODE_SEL == MODE_IWDG)
static uint32_t ulSecondRemaining;

/*
 * @brief:  The Independent Watch Dog task function
 *
 * @param pvParameters Task parameters
 */
static void vIWDGTaskFunction(void *pvParameters) {
  // Create the input queue
  QueueHandle_t xInputQueue = xQueueCreate(INPUT_QUEUE_SIZE, sizeof(uint8_t));
  if (xInputQueue == NULL) {
    return exitAppTask("Cannot create input queue.\n", xInputQueue, NULL);
  }

  // Initialize the input
  if (App_Input_Init(xInputQueue) != HAL_OK) {
    return exitAppTask("App_Input_Init failed.\n", xInputQueue, NULL);
  }

  // Determine if the system was reset due to the IWDG
  if ((HAL_RCC_GetResetSource() & HAL_RCC_RESET_FLAG_IWDG) ==
      HAL_RCC_RESET_FLAG_IWDG) {
    HAL_RCC_ClearResetFlags();
    SWD_printf("System was reset due to IWDG\n");
  }

  hal_iwdg_handle_t *pIWDG = mx_iwdg_gethandle();

  // Register the Early Wakeup Callback
  hal_status_t status = HAL_IWDG_RegisterEarlyWakeupCallback(pIWDG,
      IWDG_EarlyWakeupCallback);
  if (status != HAL_OK) {
    return exitAppTask("HAL_IWDG_RegisterEarlyWakeupCallback failed.\n",
        xInputQueue, NULL);
  }

  // Start the IWDG
  if (mx_iwdg_start() != HAL_OK) {
    return exitAppTask("mx_iwdg_start failed.\n", xInputQueue, NULL);
  }
  ulSecondRemaining = MAX_TIME / 1000;

#if (INT_SIGNAL == MODE_SEMAPHORE)
  // Create the semaphore for early IWDG interrupts
  xWDG_Semaphore = xSemaphoreCreateBinary();
  if (xWDG_Semaphore == NULL) {
    return exitAppTask("xSemaphoreCreateBinary failed.\n", xInputQueue, NULL);
  }
  QueueSetMemberHandle_t xIntSignal = xWDG_Semaphore;
#elif (INT_SIGNAL == MODE_QUEUE)
  // Create the input queue
  xIntQueue = xQueueCreate(INT_SIGNAL, sizeof(uint8_t));
  if (xIntQueue == NULL) {
    return exitAppTask("Cannot create interrupt queue.\n", xInputQueue, NULL);
  }
  QueueSetMemberHandle_t xIntSignal = xIntQueue;
#endif

  // Prepare the queue set
  QueueSetHandle_t xQueueSet = xQueueCreateSet(INPUT_QUEUE_SIZE +
      INT_SIGNAL_SIZE);
  xQueueAddToSet(xInputQueue, xQueueSet);
  xQueueAddToSet(xIntSignal, xQueueSet);

  if (xQueueSet == NULL) {
    return exitAppTask("xQueueCreateSet failed.\n", xInputQueue, xIntSignal);
  }

  SWD_printf("--> Press the user button after the count reaches 5 and "
      "before the count reaches 0 to prevent the system from resetting.\n");

  uint8_t ucEvent;
  QueueSetMemberHandle_t xActivatedMember;
  while (1) {
    xActivatedMember = xQueueSelectFromSet(xQueueSet, pdMS_TO_TICKS(1000));
    if (xActivatedMember == xInputQueue) {
      // Process input events
      xQueueReceive(xActivatedMember, &ucEvent, 0);
      SWD_printf("--> BTN_1 CLICK\n");
      if (HAL_IWDG_Refresh(pIWDG) != HAL_OK) {
        SWD_printf("HAL_IWDG_Refresh failed\n");
      }

      ulSecondRemaining = MAX_TIME / 1000;
    } else if (xActivatedMember == xIntSignal) {
#if (INT_SIGNAL == MODE_SEMAPHORE)
      xSemaphoreTake(xWDG_Semaphore, 0);
#elif (INT_SIGNAL == MODE_QUEUE)
      xQueueReceive(xActivatedMember, &ucEvent, 0);
#endif
      __asm__ volatile ("sev": : :"memory");
      SWD_printf("Remaining seconds: %ld (INT).\n",
          (MAX_TIME - EARLY_INT_TIME) / 1000);
      --ulSecondRemaining;
    } else {
      SWD_printf("Remaining seconds: %ld.\n", --ulSecondRemaining);
    }
  }
}

#elif (MODE_SEL == MODE_WWDG)
/*
 * @brief:  The Window Watch Dog task function
 *
 * @param pvParameters Task parameters
 */
static void vWWDGTaskFunction(void *pvParameters) {
  // Create the input queue
  QueueHandle_t xInputQueue = xQueueCreate(INPUT_QUEUE_SIZE, sizeof(uint8_t));
  if (xInputQueue == NULL) {
    return exitAppTask("Cannot create input queue.\n", xInputQueue, NULL);
  }

  // Initialize the input
  if (App_Input_Init(xInputQueue) != HAL_OK) {
    return exitAppTask("App_Input_Init failed.\n", xInputQueue, NULL);
  }

  // Determine if the system was reset due to the WWDG
  if ((HAL_RCC_GetResetSource() & HAL_RCC_RESET_FLAG_WWDG) ==
      HAL_RCC_RESET_FLAG_WWDG) {
    HAL_RCC_ClearResetFlags();
    SWD_printf("System was reset due to WWDG\n");
  }

  hal_wwdg_handle_t *pWWDG = mx_wwdg_gethandle();
  // Register the Early Wakeup Callback
  hal_status_t status = HAL_WWDG_RegisterEarlyWakeupCallback(pWWDG,
      WWDG_EarlyWakeupCallback);
  if (status != HAL_OK) {
    return exitAppTask("HAL_WWDG_RegisterEarlyWakeupCallback failed.\n",
        xInputQueue, NULL);
  }

  // Start the WWDG
  if (mx_wwdg_start() != HAL_OK) {
    return exitAppTask("mx_wwdg_start failed.\n", xInputQueue, NULL);
  }

  // Create the semaphore for early WWDG interrupts
  xWDG_Semaphore = xSemaphoreCreateBinary();
  if (xWDG_Semaphore == NULL) {
    return exitAppTask("xSemaphoreCreateBinary failed.\n", xInputQueue, NULL);
  }

  // Prepare the queue set
  QueueSetHandle_t xQueueSet = xQueueCreateSet(INPUT_QUEUE_SIZE + 1);
  xQueueAddToSet(xInputQueue, xQueueSet);
  xQueueAddToSet(xWDG_Semaphore, xQueueSet);
  if (xQueueSet == NULL) {
    return exitAppTask("xQueueCreateSet failed.\n", xInputQueue,
        xWDG_Semaphore);
  }

  SWD_printf("--> WWDG app running. If all goes well, nothing happens. "
      "If you press the user button, the system *may* reset "
      "(WWDG refresh outside the window).\n");

  uint8_t ucEvent;
  QueueSetMemberHandle_t xActivatedMember;
  while (1) {
    xActivatedMember = xQueueSelectFromSet(xQueueSet, pdMS_TO_TICKS(100));
    if (xActivatedMember == xInputQueue) {
      // Process input events
      xQueueReceive(xActivatedMember, &ucEvent, 0);
      SWD_printf("--> BTN_1 CLICK\n");
      if (HAL_WWDG_Refresh(pWWDG) != HAL_OK) {
        SWD_printf("HAL_WWDG_Refresh failed\n");
      }
    } else if (xActivatedMember == xWDG_Semaphore) {
      xSemaphoreTake(xWDG_Semaphore, 0);
      SWD_printf("Early WWG interrupt.\n");
    } else {
      // Timeout
      if (HAL_WWDG_Refresh(pWWDG) != HAL_OK) {
        SWD_printf("HAL_WWDG_Refresh failed\n");
      }
    }
  }
}
#endif

/*
 * @brief  Initialize the application main task.
 *    When this function is called, buttons must not be pressed.
 *
 * @retval HAL_OK if the method succeeds.
 */
hal_status_t App_Init() {
  if (xTaskCreate(
#if (MODE_SEL == MODE_IWDG)
      vIWDGTaskFunction,  // Function that implements the task
#elif (MODE_SEL == MODE_WWDG)
      vWWDGTaskFunction,  // Function that implements the task
#endif
      "App_Task",         // Text name for the task
      256,                // Stack size in words
      NULL,               // Parameter passed into the task
      10,                 // Priority
      NULL                // Used to pass out the task's handle
      ) == pdPASS) {
    return HAL_OK;
  } else {
    return HAL_ERROR;
  }
}

