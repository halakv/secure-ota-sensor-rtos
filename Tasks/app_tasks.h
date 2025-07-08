#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#include "cmsis_os2.h"

void LoggerTaskFunc(void *argument);
void SensorTaskFunc(void *argument);
void OTATaskFunc(void *argument);
void HeartbeatTaskFunc(void *argument);

void queue_init(void);

// Task handles (used in freertos.c)
extern osThreadId_t SensorTaskHandle;
extern osThreadId_t LoggerTaskHandle;
extern osThreadId_t OTATaskHandle;

// Attributes for CMSIS-V2 osThreadNew()
extern const osThreadAttr_t SensorTask_attributes;
extern const osThreadAttr_t LoggerTask_attributes;
extern const osThreadAttr_t OTATask_attributes;

#endif /* __APP_TASKS_H */
