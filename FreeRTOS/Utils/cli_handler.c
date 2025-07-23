/*
 * cli_handler.c
 *
 *  Created on: Jul 8, 2025
 *      Author: Halak Vyas
 */
#include "cli_handler.h"
#include <stdarg.h>
#include <string.h>


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

	osStatus_t status = osMessageQueuePut(loggerQueue, &msg, 0, 0);
    if (status != osOK) {
        log_printf("Put failed: %d\r\n", status);
    }

}


void handle_command(const char *cmd){
	if(strcmp(cmd, "version") == 0){
		log_enqueue_fmt("Firmware v1.0.2\r\n");
	}
	else if(strcmp(cmd, "reboot") == 0){
		log_enqueue_fmt("Rebooting...\r\n");
	}
	else if(strcmp(cmd, "data") == 0){
		SensorMessage_t snapshot;

		if(osMutexAcquire(sensor_data_mutex, osWaitForever) == osOK){
			snapshot = g_sensor_data;
			osMutexRelease(sensor_data_mutex);
			log_enqueue_fmt("Temp: %.2f C, Pressure: %.2f%%, Time: %d ms\r\n", snapshot.temperature, snapshot.pressure, snapshot.timestamp_ms);

		}else{
			log_enqueue_fmt("Sensor data access timeout\r\n");
		}

	}
	else{
		log_enqueue_fmt("invalid command \r\n");
	}
}
