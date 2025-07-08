#include "cmsis_os2.h"
#include "app_tasks.h"

#include "uart_logger.h"
#include "main.h"

#define QUEUE_SIZE		16

typedef struct{
	int32_t temperature;
	uint32_t pressure;
	uint8_t crc;
}SensorMessage_t;

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

osMessageQueueId_t sensorQueue;

void App_CreateTasks(void) {
  SensorTaskHandle = osThreadNew(SensorTaskFunc, NULL, &SensorTask_attributes);
  LoggerTaskHandle = osThreadNew(LoggerTaskFunc, NULL, &LoggerTask_attributes);
  OTATaskHandle = osThreadNew(OTATaskFunc, NULL, &OTATask_attributes);
}


/* creation of DataQueue */
void queue_init(void) {
	sensorQueue = osMessageQueueNew (QUEUE_SIZE, sizeof(SensorMessage_t), NULL);
    if (sensorQueue == NULL) {
        // Handle error
    }
}



void SensorTaskFunc(void *argument) {
  for (;;) {
	SensorMessage_t sdata;
	sdata.temperature = 25;
	sdata.pressure = 10;
	sdata.crc = 0x9F;
	osStatus_t status = osMessageQueuePut(sensorQueue, &sdata, 0, 1000);
	if(status != osOK){
		//handle error
	}
    osDelay(2000);
  }
}

void LoggerTaskFunc(void *argument) {
    for (;;) {
    	SensorMessage_t receiveMsg;
    	osStatus_t status = osMessageQueueGet(sensorQueue, &receiveMsg, 0, 1000);
    	if(status == osOK){
    		log_printf("Sensor Data is %d %d \n", receiveMsg.temperature, receiveMsg.pressure);
    	}
        osDelay(1000);
    }
}

void OTATaskFunc(void *argument) {
  for (;;) {
    log_printf("[OTA] Waiting for update...\r\n");
    osDelay(5000);
  }
}
