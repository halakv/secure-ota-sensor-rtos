/*
 * cli_handler.c
 *
 *  Created on: Jul 8, 2025
 *      Author: Halak Vyas
 */
#include "cli_handler.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "app_tasks.h"


#define LOG_QUEUE_LEN    10
#define LOG_MSG_LEN      100

static char log_pool[LOG_QUEUE_LEN][LOG_MSG_LEN];
static uint8_t log_write_index = 0;

void log_enqueue_fmt(const char *fmt, ...) {
	char msg[LOG_MSG_LEN];
	va_list args;
	va_start(args, fmt);
	vsnprintf(msg, LOG_MSG_LEN, fmt, args);
	va_end(args);

	osStatus_t status = osMessageQueuePut(loggerQueue, &msg, 0, 100); // 100ms timeout
    if (status == osErrorTimeout) {
        // Queue full, try to clear it by getting one message and discarding
        char dummy[LOG_MSG_LEN];
        osMessageQueueGet(loggerQueue, &dummy, NULL, 0);
        // Try again
        osMessageQueuePut(loggerQueue, &msg, 0, 0);
    } else if (status != osOK) {
        // Silent failure to prevent recursive logging
    }
}


static uint32_t command_count = 0;

void handle_command(const char *cmd){
	// Check string validity
	if (cmd == NULL) {
		log_printf("Error: NULL command\r\n");
		return;
	}
	
	// Manual comparison to bypass strcmp issue
	if(strcmp(cmd, "version") == 0){
		log_printf("Firmware v1.0.2\r\n");
	}
	else if(strcmp(cmd, "reboot") == 0){
		log_printf("Rebooting...\r\n");
	}
	else if(strcmp(cmd, "otastart") == 0){
		if (otaQueue == NULL) {
			log_printf("ERROR: otaQueue is NULL!\r\n");
			return;
		}
		
		OTAMessage_t otaMsg = {0};
		otaMsg.command = OTA_CMD_START;
		
		osStatus_t status = osMessageQueuePut(otaQueue, &otaMsg, 0, 100);
		
		if (status != osOK) {
			log_printf("Failed to send OTA start, error: %d\r\n", status);
		} else {
			osDelay(10); // Give OTA task time to process
		}
	}
	else if(strcmp(cmd, "otadata") == 0){
		// Send test data: 64 bytes at offset 0
		OTAMessage_t otaMsg = {0};
		otaMsg.command = OTA_CMD_DATA;
		otaMsg.offset = 0;
		otaMsg.length = 64;
		
		// Fill with test pattern
		for (uint32_t i = 0; i < 64; i++) {
			otaMsg.data[i] = 0xAA + i;
		}
		
		osStatus_t status = osMessageQueuePut(otaQueue, &otaMsg, 0, 100);
		if (status == osOK) {
			uint32_t count = osMessageQueueGetCount(otaQueue);
			log_printf("OTA data sent, queue count: %lu\r\n", count);
			osDelay(10); // Give OTA task time to process
		} else {
			log_printf("Failed to send OTA data, error: %d\r\n", status);
		}
		log_printf("otadata command completed\r\n");
	}
	else if(strcmp(cmd, "otafinish") == 0){
		OTAMessage_t otaMsg = {0};
		otaMsg.command = OTA_CMD_FINISH;
		
		if (osMessageQueuePut(otaQueue, &otaMsg, 0, 0) == osOK) {
			log_printf("OTA finish command sent\r\n");
		} else {
			log_printf("Failed to send OTA finish command\r\n");
		}
	}
	else if(strcmp(cmd, "queuetest") == 0){
		log_printf("Testing queue: %p\r\n", (void*)otaQueue);
		if (otaQueue != NULL) {
			uint32_t count = osMessageQueueGetCount(otaQueue);
			uint32_t space = osMessageQueueGetSpace(otaQueue);
			log_printf("Queue msgs: %lu, space: %lu\r\n", count, space);
		} else {
			log_printf("Queue is NULL\r\n");
		}
	}
	else if(strcmp(cmd, "data") == 0){
		SensorMessage_t snapshot;

		if(osMutexAcquire(sensor_data_mutex, osWaitForever) == osOK){
			snapshot = g_sensor_data;
			osMutexRelease(sensor_data_mutex);
			log_printf("Temp: %.2f C, Pressure: %.2f%%, Time: %d ms\r\n", snapshot.temperature, snapshot.pressure, snapshot.timestamp_ms);

		}else{
			log_printf("Sensor data access timeout\r\n");
		}
	}
	else{
		log_printf("invalid command: '%s'\r\n", cmd);
	}
}
