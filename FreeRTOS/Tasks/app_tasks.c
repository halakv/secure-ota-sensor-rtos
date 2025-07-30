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

// OTA state management
volatile OTAState_t ota_state = OTA_STATE_IDLE;
volatile uint32_t ota_expected_size = 0;
volatile uint32_t ota_received_size = 0;

/* creation of DataQueue */
void CreateQueue(void) {
  //Logger Queue Creation
	loggerQueue = osMessageQueueNew (QUEUE_SIZE, 100, NULL);
    if (loggerQueue == NULL) {
    	log_printf("Logger Queue creation failed\r\n");
    }
    
    //OTA Queue Creation
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
	uint8_t ota_buffer[256];
	uint32_t ota_buffer_index = 0;
	
	log_printf("[CLI] Task started\r\n");

	for(;;){
		if (osMessageQueueGet(cliRxQueueHandle, &bytes, NULL, osWaitForever) == osOK){
			
			// Check if we're in OTA data receiving mode
			if (ota_state == OTA_STATE_RECEIVING) {
				// Skip any ASCII characters or control characters that might be leftover from commands
				// OTA binary data should not contain these characters in the first bytes
				if (ota_received_size == 0 && ota_buffer_index == 0 && 
				    (bytes >= 0x07 && bytes <= 0x7E)) {  // Extended range to catch control chars
					continue; // Skip this byte silently
				}
				
				// Collect binary data for OTA
				ota_buffer[ota_buffer_index++] = bytes;
				
				// When buffer is full or we've received all expected data
				if (ota_buffer_index >= 256 || ota_received_size + ota_buffer_index >= ota_expected_size) {
					// Send data to OTA task
					OTAMessage_t otaMsg = {0};
					otaMsg.command = OTA_CMD_DATA;
					otaMsg.offset = ota_received_size;
					otaMsg.length = ota_buffer_index;
					
					// Copy data to message
					for (uint32_t j = 0; j < ota_buffer_index; j++) {
						otaMsg.data[j] = ota_buffer[j];
					}
					
					
					if (osMessageQueuePut(otaQueue, &otaMsg, 0, 100) == osOK) {
						ota_received_size += ota_buffer_index;
						
						// Check if transfer is complete
						if (ota_received_size >= ota_expected_size) {
							log_printf("[CLI] OTA transfer complete (%lu bytes)\r\n", ota_received_size);
							ota_state = OTA_STATE_COMPLETE;
							
							// Send finish command
							OTAMessage_t finishMsg = {0};
							finishMsg.command = OTA_CMD_FINISH;
							osMessageQueuePut(otaQueue, &finishMsg, 0, 100);
						}
					} else {
						log_printf("[CLI] Failed to send OTA data chunk\r\n");
					}
					
					ota_buffer_index = 0;
				}
			} else {
				// Normal command mode
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
	}
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
  
  log_printf("[OTA] Queue handle: %p\r\n", (void*)otaQueue);
  
  for (;;) {
    osStatus_t status = osMessageQueueGet(otaQueue, &otaMsg, NULL, osWaitForever);
    
    if (status == osOK) {
      log_printf("[OTA] Received message, command: %d\r\n", otaMsg.command);
      
      switch(otaMsg.command) {
        case OTA_CMD_START:
          log_printf("[OTA] Starting firmware update to Slot B...\r\n");
          ota_erase_slot();
          ota_state = OTA_STATE_RECEIVING;
          ota_received_size = 0;
          totalBytesReceived = 0;
          log_printf("[OTA] Ready to receive firmware data\r\n");
          
          // Yield after erase operation to allow other tasks to run
          osThreadYield();
          log_printf("[OTA] START command completed, back to waiting\r\n");
          break;
          
        case OTA_CMD_DATA:
          if (ota_state == OTA_STATE_RECEIVING) {
            HAL_StatusTypeDef hal_status = ota_write_firmware(otaMsg.offset, otaMsg.data, otaMsg.length);
            if (hal_status == HAL_OK) {
              totalBytesReceived += otaMsg.length;
              log_printf("[OTA] Written %lu bytes at offset 0x%08lX (Total: %lu bytes)\r\n", 
                        otaMsg.length, otaMsg.offset, totalBytesReceived);
              
              // Yield after flash write to allow other tasks to run
              osThreadYield();
            } else {
              log_printf("[OTA] Write failed at offset 0x%08lX, status: %d\r\n", otaMsg.offset, hal_status);
              ota_state = OTA_STATE_IDLE;
            }
          } else {
            log_printf("[OTA] Data received but OTA not in RECEIVING state\r\n");
          }
          break;
          
        case OTA_CMD_FINISH:
          if (ota_state == OTA_STATE_RECEIVING || ota_state == OTA_STATE_COMPLETE) {
            log_printf("[OTA] Firmware update completed. Total bytes: %lu\r\n", totalBytesReceived);
            
            // Complete OTA process and switch boot slot
            HAL_StatusTypeDef switch_status = ota_complete_and_switch(totalBytesReceived);
            if (switch_status == HAL_OK) {
              log_printf("[OTA] Boot slot switched successfully\r\n");
              log_printf("[OTA] System will boot from new firmware after reset\r\n");
              
              // Optional: Trigger system reset to boot new firmware
              log_printf("[OTA] Triggering system reset in 3 seconds...\r\n");
              osDelay(3000);
              NVIC_SystemReset();
            } else {
              log_printf("[OTA] Failed to switch boot slot: %d\r\n", switch_status);
              log_printf("[OTA] OTA completed but system will boot from old firmware\r\n");
            }
            
            ota_state = OTA_STATE_IDLE;
          } else {
            log_printf("[OTA] Finish command received but OTA was not in progress\r\n");
          }
          break;
          
        default:
          log_printf("[OTA] Unknown command received: %d\r\n", otaMsg.command);
          break;
      }
    }
  }
}
