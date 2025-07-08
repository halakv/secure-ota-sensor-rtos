#include "cmsis_os2.h"
#include "app_tasks.h"

#include "uart_logger.h"
#include "main.h"

osThreadId_t SensorTaskHandle;
const osThreadAttr_t SensorTask_attributes = {
  .name = "SensorTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t LoggerTaskHandle;
const osThreadAttr_t LoggerTask_attributes = {
  .name = "LoggerTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow1,
};

osThreadId_t OTATaskHandle;
const osThreadAttr_t OTATask_attributes = {
  .name = "OTATask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

osThreadId_t HeartbeatTaskHandle;
const osThreadAttr_t HeartbeatTask_attributes = {
  .name = "HeartbeatTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

void SensorTaskFunc(void *argument) {
  for (;;) {
    log_printf("[SENSOR] value: %d\r\n", 42);
    osDelay(2000);
  }
}

void LoggerTaskFunc(void *argument) {
    for (;;) {
        log_printf("[LOG] Logger running...\r\n");
        osDelay(1000);
    }
}

void OTATaskFunc(void *argument) {
  for (;;) {
    log_printf("[OTA] Waiting for update...\r\n");
    osDelay(5000);
  }
}


void HeartbeatTaskFunc(void *argument) {
  for (;;) {
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    osDelay(500);
  }
}
