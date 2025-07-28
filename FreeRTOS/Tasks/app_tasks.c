#include "cmsis_os2.h"
#include "app_tasks.h"

#include "uart_logger.h"
#include "main.h"
#include "ota.h"
#include "cli_handler.h"
#include <stdbool.h>

#define QUEUE_SIZE		10

extern UART_HandleTypeDef huart2;

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
osMessageQueueId_t otaQueue;

/* creation of DataQueue */
void CreateQueue(void) {
  //Logger Queue Creation
	loggerQueue = osMessageQueueNew (QUEUE_SIZE, 100, NULL);
    if (loggerQueue == NULL) {
    	log_printf("Logger Queue creation failed\r\n");
    }
    
    //OTA Queue Creation
    if (sizeof(OTAMessage_t) > 100) {
        log_printf("WARNING: OTAMessage_t is too large (%d bytes)!\r\n", sizeof(OTAMessage_t));
    }
    otaQueue = osMessageQueueNew(5, sizeof(OTAMessage_t), NULL);
    if (otaQueue == NULL) {
    	log_printf("OTA Queue creation failed\r\n");
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
	uint8_t bytes;
	log_printf("[CLI] Task started\r\n");

	for(;;){
		if (osMessageQueueGet(cliRxQueueHandle, &bytes, NULL, osWaitForever) == osOK){
			if(bytes == '\r' || bytes == '\n'){
				command_buff[i] = '\0';
				// Only process non-empty commands
				if(i > 0) {
					log_printf("[CLI] Calling handle_command with: '%s'\r\n", command_buff);
					handle_command(command_buff);
					log_printf("[CLI] handle_command returned\r\n");
				}
				i = 0;
			}else if(i < sizeof(command_buff) - 1){
				command_buff[i++] = bytes;
			}
		}
	}
	osDelay(5000);
}

void SensorTaskFunc(void *argument) {

  for (;;) {

	  float temp = 25;
	  float press = 10;

	  if(osMutexAcquire(sensor_data_mutex, osWaitForever) == osOK){
		  g_sensor_data.temperature = temp;
		  g_sensor_data.pressure = press;
		  g_sensor_data.timestamp_ms = HAL_GetTick();
		  osMutexRelease(sensor_data_mutex);
	  }

  }
  osDelay(5000);
}

void LoggerTaskFunc(void *argument) {

    for (;;) {
    	char msg[100];
    	if(osMessageQueueGet(loggerQueue, &msg, 0, osWaitForever) == osOK){
    		log_printf("%s \r\n",msg);
    	}
    }
    osDelay(2000);
}

void OTATaskFunc(void *argument) {
  OTAMessage_t otaMsg;
  bool otaInProgress = false;
  uint32_t totalBytesReceived = 0;
  
  log_printf("[OTA] Task started, waiting for commands...\r\n");
  
  // Verify queue is valid
  if (otaQueue == NULL) {
    log_printf("[OTA] ERROR: Queue handle is NULL!\r\n");
    for(;;) {
      osDelay(1000);
      log_printf("[OTA] ERROR: Stuck - Queue is NULL\r\n");
    }
  }
  
  for (;;) {
    osStatus_t status = osMessageQueueGet(otaQueue, &otaMsg, NULL, osWaitForever);
    
    if (status == osOK) {
      
      switch(otaMsg.command) {
        case OTA_CMD_START:
          log_printf("[OTA] Starting firmware update to Slot B...\r\n");
          ota_erase_slot();
          otaInProgress = true;
          totalBytesReceived = 0;
          log_printf("[OTA] Ready to receive firmware data\r\n");
          
          // Yield after erase operation to allow other tasks to run
          osThreadYield();
          log_printf("[OTA] START command completed, back to waiting\r\n");
          break;
          
        case OTA_CMD_DATA:
          if (otaInProgress) {
            HAL_StatusTypeDef status = ota_write_firmware(otaMsg.offset, otaMsg.data, otaMsg.length);
            if (status == HAL_OK) {
              totalBytesReceived += otaMsg.length;
              log_printf("[OTA] Written %lu bytes at offset 0x%08lX (Total: %lu bytes)\r\n", 
                        otaMsg.length, otaMsg.offset, totalBytesReceived);
              
              // Yield after flash write to allow other tasks to run
              osThreadYield();
            } else {
              log_printf("[OTA] Write failed at offset 0x%08lX\r\n", otaMsg.offset);
              otaInProgress = false;
            }
          } else {
            log_printf("[OTA] Data received but OTA not started\r\n");
          }
          break;
          
        case OTA_CMD_FINISH:
          if (otaInProgress) {
            log_printf("[OTA] Firmware update completed. Total bytes: %lu\r\n", totalBytesReceived);
            log_printf("[OTA] Firmware written to Slot B successfully\r\n");
            otaInProgress = false;
          } else {
            log_printf("[OTA] Finish command received but OTA was not in progress\r\n");
          }
          break;
          
        default:
          log_printf("[OTA] Unknown command received\r\n");
          break;
      }
    }
  }
}
