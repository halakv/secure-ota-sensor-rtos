#ifndef UART_LOGGER_H
#define UART_LOGGER_H

#include "cmsis_os2.h"
#include "main.h"
#include <stdio.h>


void uart_logger_init(UART_HandleTypeDef *huart);
void log_printf(const char *fmt, ...);

#endif
