/*******************************************************************************
 * file           : app_task.c
 * brief          : The application task implementation.
 ******************************************************************************/
#include "mx_hal_def.h"
#include "middleware/freertos/include/FreeRTOS.h"
#include "middleware/freertos/include/task.h"
#include "middleware/freertos/include/queue.h"
#include "../../Shared/Debug/swd_printf.h"
#include "../../Shared/Utils/error_handler.h"
#include "app_task.h"

// The task handle
static TaskHandle_t xTaskToNotify;

// Notification flags
#define ALARM_A       0x1UL
#define ALARM_B       0x2UL
#define RTC_TIMESTAMP 0x4UL

// The timestamp of PC13 going HIGH
volatile hal_rtc_time_t RtcTimeStamp;
volatile hal_rtc_date_t RtcDateStamp;
volatile hal_rtc_timestamp_information_t TimeStampInfo;

/*
 * @brief: RTC timestamp event callback
 */
void HAL_RTC_TimestampEventCallback() {
  TimeStampInfo.flag = HAL_RTC_TIMESTAMP_NO_EVENT;
  hal_status_t status = HAL_RTC_TIMESTAMP_GetDateTime(
      (hal_rtc_time_t *)&RtcTimeStamp, (hal_rtc_date_t *)&RtcDateStamp,
      (hal_rtc_timestamp_information_t *)&TimeStampInfo);
  if (status == HAL_OK) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyIndexedFromISR(xTaskToNotify, 0, RTC_TIMESTAMP, eSetBits,
        &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  } else {
    SWD_printf("HAL_RTC_TIMESTAMP_GetDateTime failed\n");
  }
}

/*
 * @brief: RTC alarm A callback
 */
void HAL_RTC_AlarmAEventCallback(void) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  __asm__ volatile ("sev": : :"memory");
  xTaskNotifyIndexedFromISR(xTaskToNotify, 0, ALARM_A, eSetBits,
      &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*
 * @brief: RTC alarm B callback
 */
void HAL_RTC_AlarmBEventCallback(void) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  __asm__ volatile ("sev": : :"memory");
  xTaskNotifyIndexedFromISR(xTaskToNotify, 0, ALARM_B, eSetBits,
      &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*
 * @brief:  Exit the application task when there is an error
 *
 * @param error A description of the error that occurred
 */
static void exitAppTask(char *error) {
  ErrorHandler(error);

  vTaskDelete(NULL);
}

/*
 * @brief: Print the time and date
 *
 * @param prefix The prefix
 * @param time The RTC time
 * @param date The RTC date
 */
static void printTimeDate(char* prefix, hal_rtc_time_t *time,
    hal_rtc_date_t *date) {
  // Print the current time and date
  SWD_printf("%s: %02d:%02d:%02d, %02d/%02d/%02d\n", prefix,
      time->hour, time->min, time->sec,
      HAL_RTC_CONVERT_BCD2DEC(date->mon), date->mday, date->year);
}

/*
 * @brief: Print the alarm time and date
 *
 * @param name The name of the alarm
 * @param pAtd The alarm time and date
 */
static void printAlarmTimeDate(char* name, hal_rtc_alarm_date_time_t *pAtd) {
  if (pAtd->mday_wday_selection == HAL_RTC_ALARM_DAY_TYPE_SEL_MONTHDAY) {
    SWD_printf("%s: %02d:%02d:%02d | month day: %02d | "
        "mask: %08lx | subsec_mask: %08lx\n", name,
        pAtd->time.hour, pAtd->time.min, pAtd->time.sec, pAtd->wday_mday.mday,
        pAtd->mask, pAtd->subsec_mask);
  } else if (pAtd->mday_wday_selection == HAL_RTC_ALARM_DAY_TYPE_SEL_WEEKDAY) {
    SWD_printf("%s, %02d:%02d:%02d | week day: %d | mask: %08lx | "
        "mask: %08lx | subsec_mask: %08lx\n", name,
        pAtd->time.hour, pAtd->time.min, pAtd->time.sec, pAtd->wday_mday.wday,
        pAtd->mask, pAtd->subsec_mask);
  }
}

#define INPUT_QUEUE_SIZE  8

/*
 * @brief:  The task function
 *
 * @param pvParameters Task parameters
 */
static void vTaskFunction(void *pvParameters) {
  // Get the current task handle
  xTaskToNotify = xTaskGetCurrentTaskHandle();

  hal_rtc_time_t RtcTime;
  hal_rtc_date_t RtcDate;
  if (HAL_RTC_CALENDAR_GetDateTime(&RtcDate, &RtcTime) != HAL_OK) {
    exitAppTask("HAL_RTC_CALENDAR_GetDateTime failed.\n");
    return;
  }

  // Print the current time and date
  printTimeDate("Time now", &RtcTime, &RtcDate);

  /* Configure alarm A */
  hal_rtc_alarm_config_t alarm_a_config;
  alarm_a_config.subsec_auto_reload =
      HAL_RTC_ALARM_SUBSECONDS_AUTO_RELOAD_DISABLE;
  alarm_a_config.auto_clear = HAL_ALARM_AUTO_CLEAR_ENABLE;
  if (HAL_RTC_ALARM_SetConfig(HAL_RTC_ALARM_A, &alarm_a_config) != HAL_OK) {
    exitAppTask("HAL_RTC_ALARM_SetConfig failed.\n");
    return;
  }

  uint32_t ALARM_A_IN_MINUTES = 1UL;
  hal_rtc_alarm_date_time_t alarm_a_date_time;
  alarm_a_date_time.mday_wday_selection = HAL_RTC_ALARM_DAY_TYPE_SEL_MONTHDAY;
  alarm_a_date_time.wday_mday.mday = RtcDate.mday;
  alarm_a_date_time.time.am_pm = HAL_RTC_TIME_FORMAT_AM_24H;
  if (RtcTime.min + ALARM_A_IN_MINUTES < 60) {
    alarm_a_date_time.time.hour = RtcTime.hour;
  } else {
    alarm_a_date_time.time.hour = RtcTime.hour + 1;
    if (alarm_a_date_time.time.hour >= 24) {
      exitAppTask("Alarm is in the next day/month/year.\n");
      return;
    }
  }
  alarm_a_date_time.time.min = (RtcTime.min + ALARM_A_IN_MINUTES) % 60;
  alarm_a_date_time.time.sec = 0U;
  alarm_a_date_time.mask = HAL_RTC_ALARM_MASK_NONE;

  alarm_a_date_time.time.subsec = 0U;
  alarm_a_date_time.subsec_mask = 0U;
  if (HAL_RTC_ALARM_SetDateTime(HAL_RTC_ALARM_A, &alarm_a_date_time) != HAL_OK) {
    exitAppTask("HAL_RTC_ALARM_SetDateTime failed.\n");
    return;
  }

  // Get Alarm A time and date
  hal_rtc_alarm_date_time_t atd;
  HAL_RTC_ALARM_GetDateTime(HAL_RTC_ALARM_A, &atd);
  printAlarmTimeDate("Alarm A set", &atd);

  /* Configure alarm B */
  hal_rtc_alarm_config_t alarm_b_config;
  alarm_b_config.subsec_auto_reload = HAL_RTC_ALARM_SUBSECONDS_AUTO_RELOAD_DISABLE;
  alarm_b_config.auto_clear = HAL_ALARM_AUTO_CLEAR_ENABLE;
  if (HAL_RTC_ALARM_SetConfig(HAL_RTC_ALARM_B, &alarm_b_config) != HAL_OK) {
    exitAppTask("HAL_RTC_ALARM_SetConfig failed.\n");
    return;
  }

  uint32_t ALARM_B_IN_MINUTES = 2UL;
  hal_rtc_alarm_date_time_t alarm_b_date_time;
  alarm_b_date_time.mday_wday_selection = HAL_RTC_ALARM_DAY_TYPE_SEL_MONTHDAY;
  alarm_b_date_time.wday_mday.mday = RtcDate.mday;
  alarm_b_date_time.time.am_pm = HAL_RTC_TIME_FORMAT_AM_24H;
  if (RtcTime.min + ALARM_B_IN_MINUTES < 60) {
    alarm_b_date_time.time.hour = RtcTime.hour;
  } else {
    alarm_b_date_time.time.hour = RtcTime.hour + 1;
    if (alarm_b_date_time.time.hour >= 24) {
      exitAppTask("Alarm is in the next day/month/year.\n");
      return;
    }
  }
  alarm_b_date_time.time.min = (RtcTime.min + ALARM_B_IN_MINUTES) % 60;
  alarm_b_date_time.time.sec = 0U;
  alarm_b_date_time.mask = HAL_RTC_ALARM_MASK_NONE;

  alarm_b_date_time.time.subsec = 0U;
  alarm_b_date_time.subsec_mask = 0U;
  if (HAL_RTC_ALARM_SetDateTime(HAL_RTC_ALARM_B, &alarm_b_date_time) != HAL_OK) {
    exitAppTask("HAL_RTC_ALARM_SetDateTime failed.\n");
    return;
  }

  // Get Alarm B time and date
  HAL_RTC_ALARM_GetDateTime(HAL_RTC_ALARM_B, &atd);
  printAlarmTimeDate("Alarm B set", &atd);

  // Start Alarm A
  if (HAL_RTC_ALARM_Start(HAL_RTC_ALARM_A, HAL_RTC_ALARM_IT_ENABLE) != HAL_OK) {
    exitAppTask("HAL_RTC_ALARM_Start failed.\n");
    return;
  }

  // Start Alarm B
  if (HAL_RTC_ALARM_Start(HAL_RTC_ALARM_B, HAL_RTC_ALARM_IT_ENABLE) != HAL_OK) {
    exitAppTask("HAL_RTC_ALARM_Start failed.\n");
    return;
  }

  // Enable the timestamp interrupt
  if (HAL_RTC_TIMESTAMP_EnableIT() != HAL_OK) {
    exitAppTask("HAL_RTC_TIMESTAMP_EnableIT failed.\n");
    return;
  }

  SWD_printf("--> Wait for Alarm A and B to go off. "
      "Press the button to get a timestamp.\n");

  uint32_t ulInterruptStatus;
  while (1) {
    xTaskNotifyWaitIndexed(0, 0, 0, &ulInterruptStatus, portMAX_DELAY);
    __asm__ volatile ("sev": : :"memory");
    if ((ulInterruptStatus & ALARM_A) == ALARM_A) {
      ulTaskNotifyValueClearIndexed(xTaskToNotify, 0, ALARM_A);

      if (HAL_RTC_CALENDAR_GetDateTime(&RtcDate, &RtcTime) != HAL_OK) {
        exitAppTask("HAL_RTC_CALENDAR_GetDateTime failed.\n");
        return;
      }
      printTimeDate("Alarm A went off", &RtcTime, &RtcDate);
    }

    if ((ulInterruptStatus & ALARM_B) == ALARM_B) {
      ulTaskNotifyValueClearIndexed(xTaskToNotify, 0, ALARM_B);
      if (HAL_RTC_CALENDAR_GetDateTime(&RtcDate, &RtcTime) != HAL_OK) {
        exitAppTask("HAL_RTC_CALENDAR_GetDateTime failed.\n");
        return;
      }
      printTimeDate("Alarm B went off", &RtcTime, &RtcDate);
    }

    if ((ulInterruptStatus & RTC_TIMESTAMP) == RTC_TIMESTAMP) {
      ulTaskNotifyValueClearIndexed(xTaskToNotify, 0, RTC_TIMESTAMP);
      // Print the timestamp
      printTimeDate("Timestamp", (hal_rtc_time_t *)&RtcTimeStamp,
          (hal_rtc_date_t *)&RtcDateStamp);
    }
  }
}

/*
 * @brief  Initialize the application main task.
 *    When this function is called, buttons must not be pressed.
 *
 * @retval HAL_OK if the method succeeds.
 */
hal_status_t App_Init() {
  if (xTaskCreate(
      vTaskFunction,      // Function that implements the task
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

