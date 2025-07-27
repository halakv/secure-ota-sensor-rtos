#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#include "cmsis_os2.h"

typedef struct{
	float temperature;
	float pressure;
	uint32_t timestamp_ms;
}SensorMessage_t;

extern SensorMessage_t g_sensor_data;

void CLITaskFunc(void *argument);
void LoggerTaskFunc(void *argument);
void SensorTaskFunc(void *argument);
void OTATaskFunc(void *argument);
void HeartbeatTaskFunc(void *argument);

void QueueCreate(void);

extern osMessageQueueId_t loggerQueue;
extern osMessageQueueId_t cliRxQueueHandle;

// Mutex for thread-safe access
extern osMutexId_t sensor_data_mutex;
extern const osMutexAttr_t sensor_data_mutex_attr;

#endif /* __APP_TASKS_H */
