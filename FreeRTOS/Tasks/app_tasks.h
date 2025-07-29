#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#include "cmsis_os2.h"

typedef struct{
	float temperature;
	float pressure;
	uint32_t timestamp_ms;
}SensorMessage_t;

typedef enum {
	OTA_CMD_START,
	OTA_CMD_DATA,
	OTA_CMD_FINISH
} OTACommand_t;

typedef struct {
	OTACommand_t command;
	uint32_t offset;
	uint32_t length;
	uint8_t data[256];  // Increased buffer size for larger chunks
} OTAMessage_t;

// OTA state management
typedef enum {
	OTA_STATE_IDLE,
	OTA_STATE_RECEIVING,
	OTA_STATE_COMPLETE
} OTAState_t;

extern volatile OTAState_t ota_state;
extern volatile uint32_t ota_expected_size;
extern volatile uint32_t ota_received_size;

extern SensorMessage_t g_sensor_data;

void CLITaskFunc(void *argument);
void LoggerTaskFunc(void *argument);
void SensorTaskFunc(void *argument);
void OTATaskFunc(void *argument);
void HeartbeatTaskFunc(void *argument);

void QueueCreate(void);

extern osMessageQueueId_t loggerQueue;
extern osMessageQueueId_t cliRxQueueHandle;
extern osMessageQueueId_t otaQueue;

// Mutex for thread-safe access
extern osMutexId_t sensor_data_mutex;
extern const osMutexAttr_t sensor_data_mutex_attr;

#endif /* __APP_TASKS_H */
