#include "cmsis_os2.h"
#include "app_tasks.h"

#include "uart_logger.h"
#include "main.h"

#define QUEUE_SIZE		10

extern UART_HandleTypeDef huart2;
uint8_t rx_char;
char command_buff[64];
int i = 0;

SensorMessage_t g_sensor_data = {0};

//Mutex creating and init

const osMutexAttr_t sensor_data_mutex_attr = {
    .name = "SensorDataMutex"
};

osMutexId_t sensor_data_mutex = NULL;

void CreateMutex(){
	sensor_data_mutex = osMutexNew(&sensor_data_mutex_attr);
	if (sensor_data_mutex == NULL) {
	    log_printf("Failed to create SensorDataMutex\r\n");
	}

}

osMessageQueueId_t loggerQueue;
/* creation of DataQueue */
void CreateQueue(void) {
	loggerQueue = osMessageQueueNew (QUEUE_SIZE, 100, NULL);
    if (loggerQueue == NULL) {
    	log_printf("Queue creation failed\r\n");
    }
}

void HeartbeatTaskFunc(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
	log_printf("[HeartBeat] task alive\r\n");
  for(;;)
  {
	  HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
	  osDelay(500);
  }
  osDelay(1000);
}

void CLITaskFunc(void *argument) {
	log_printf("[CLI] task alive\r\n");
	for(;;){
		if(HAL_UART_Receive(&huart2, &rx_char, 1, HAL_MAX_DELAY) == HAL_OK){
			if(rx_char == '\r' || rx_char == '\n'){
				command_buff[i] = '\0';
				handle_command(command_buff);
				i = 0;
			}else if(i < sizeof(command_buff) - 1){
				command_buff[i++] = rx_char;
			}
		}
	}
	osDelay(5000);
}

void SensorTaskFunc(void *argument) {
	log_printf("[Sensor] task alive\r\n");
  for (;;) {

	  float temp = 25;
	  float press = 10;

//	  if(osMutexAcquire(sensor_data_mutex, osWaitForever) == osOK){
		  g_sensor_data.temperature = temp;
		  g_sensor_data.pressure = press;
		  g_sensor_data.timestamp_ms = HAL_GetTick();
//		  osMutexRelease(sensor_data_mutex);
//	  }

  }
  osDelay(5000);
}

void LoggerTaskFunc(void *argument) {
	log_printf("[Logger] task alive\r\n");
    for (;;) {
    	char msg[100];
//    	log_printf("Message from logger...\r\n");
    	if(osMessageQueueGet(loggerQueue, &msg, 0, osWaitForever) == osOK){
    		log_printf("%s \r\n",msg);
    	}
    }
    osDelay(2000);
}

void OTATaskFunc(void *argument) {
  for (;;) {
    log_printf("[OTA] Waiting for update...\r\n");

  }
  osDelay(5000);
}
