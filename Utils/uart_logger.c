#include "uart_logger.h"
#include <stdarg.h>

static UART_HandleTypeDef *g_uart;

void uart_logger_init(UART_HandleTypeDef *huart) {
    g_uart = huart;
}

void log_printf(const char *fmt, ...) {
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (g_uart && len > 0) {
        HAL_UART_Transmit(g_uart, (uint8_t*)buffer, len, HAL_MAX_DELAY);
    }
}
