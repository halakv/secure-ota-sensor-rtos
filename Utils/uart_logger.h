#ifndef UART_LOGGER_H
#define UART_LOGGER_H

#include "main.h"
#include <stdio.h>


void uart_logger_init(UART_HandleTypeDef *huart);
void log_printf(const char *fmt, ...);

#endif
