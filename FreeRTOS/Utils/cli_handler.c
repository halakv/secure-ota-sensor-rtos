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
#include <stdio.h>
#include "app_tasks.h"
#include "boot_metadata.h"
#include "ota.h"


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
	
	if(strcmp(cmd, "version") == 0){
		log_printf("Firmware v1.0.2\r\n");
	}
	else if(strcmp(cmd, "reboot") == 0){
		log_printf("Rebooting...\r\n");
	}
	else if(strncmp(cmd, "otastart ", 9) == 0){
		// Parse firmware size: "otastart 12345"
		uint32_t firmware_size = 0;
		if (sscanf(cmd + 9, "%lu", &firmware_size) == 1 && firmware_size > 0) {
			if (otaQueue == NULL) {
				log_printf("ERROR: otaQueue is NULL!\r\n");
				return;
			}
			
			// Set expected size for OTA transfer
			ota_expected_size = firmware_size;
			ota_received_size = 0;
			
			OTAMessage_t otaMsg = {0};
			otaMsg.command = OTA_CMD_START;
			
			osStatus_t status = osMessageQueuePut(otaQueue, &otaMsg, 0, 100);
			
			if (status != osOK) {
				log_printf("Failed to send OTA start, error: %d\r\n", status);
			} else {
				log_printf("OTA started, expecting %lu bytes\r\n", firmware_size);
				log_printf("Ready to receive firmware binary data...\r\n");
				
				// Wait for OTA task to complete initialization and set state
				osDelay(100); // Give OTA task time to erase and set state
				
				// Wait for clean state transition
				osDelay(50); // Allow any pending UART data to arrive and be cleared
			}
		} else {
			log_printf("Usage: otastart <firmware_size_bytes>\r\n");
			log_printf("Example: otastart 49152\r\n");
		}
	}
	else if(strcmp(cmd, "otastatus") == 0){
		const char* state_str = "UNKNOWN";
		switch(ota_state) {
			case OTA_STATE_IDLE: state_str = "IDLE"; break;
			case OTA_STATE_RECEIVING: state_str = "RECEIVING"; break;
			case OTA_STATE_COMPLETE: state_str = "COMPLETE"; break;
		}
		log_printf("OTA State: %s\r\n", state_str);
		log_printf("Expected: %lu bytes, Received: %lu bytes\r\n", ota_expected_size, ota_received_size);
		if (ota_expected_size > 0) {
			uint32_t progress = (ota_received_size * 100) / ota_expected_size;
			log_printf("Progress: %lu%%\r\n", progress);
		}
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
	else if(strcmp(cmd, "crc") == 0){
		// Calculate and display CRC of current active slot
		uint32_t current_slot_addr = (boot_metadata->active_slot == SLOT_A) ? SLOT_A_ADDRESS : SLOT_B_ADDRESS;
		uint32_t image_size = (boot_metadata->image_size > 0) ? boot_metadata->image_size : (192 * 1024);
		
		log_printf("Calculating CRC for active slot (%s)...\r\n", 
		           (boot_metadata->active_slot == SLOT_A) ? "SLOT_A" : "SLOT_B");
		
		uint32_t calculated_crc = calculate_flash_crc_ota(current_slot_addr, image_size);
		
		log_printf("Flash CRC32: 0x%08X\r\n", (unsigned int)calculated_crc);
		log_printf("Expected CRC: 0x%08X\r\n", (unsigned int)boot_metadata->crc);
		
		if (boot_metadata->crc != 0xFFFFFFFF) {
			if (calculated_crc == boot_metadata->crc) {
				log_printf("CRC verification: PASS\r\n");
			} else {
				log_printf("CRC verification: FAIL\r\n");
			}
		} else {
			log_printf("CRC verification: SKIPPED (no expected CRC)\r\n");
		}
	}
	else if(strcmp(cmd, "initmetadata") == 0){
		log_printf("Initializing metadata...\r\n");
		HAL_StatusTypeDef status = initialize_metadata();
		if (status == HAL_OK) {
			log_printf("Metadata initialized successfully\r\n");
		} else {
			log_printf("Metadata initialization failed: %d\r\n", status);
		}
	}
	else if(strcmp(cmd, "clearprotection") == 0){
		log_printf("Clearing flash protection...\r\n");
		HAL_StatusTypeDef status = clear_flash_protection();
		if (status == HAL_OK) {
			log_printf("Flash protection cleared successfully\r\n");
			log_printf("Please reset the device for changes to take effect\r\n");
		} else {
			log_printf("Flash protection clear failed: %d\r\n", status);
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
