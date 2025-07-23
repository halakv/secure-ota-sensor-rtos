#include "uart_logger.h"
#include <stdarg.h>

static UART_HandleTypeDef *g_uart;

osMutexId_t UartMutexHandle;
const osMutexAttr_t UartMutex_attributes = {
  .name = "UartMutex"
};

void uart_logger_init(UART_HandleTypeDef *huart) {
    g_uart = huart;
    UartMutexHandle = osMutexNew(&UartMutex_attributes);
    if (UartMutexHandle == NULL) {
        // Handle error: mutex creation failed
    }
}

void log_printf(const char *fmt, ...) {
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (osMutexAcquire(UartMutexHandle, osWaitForever) == osOK) {
        HAL_UART_Transmit(g_uart, (uint8_t*)buffer, len, HAL_MAX_DELAY);
        osMutexRelease(UartMutexHandle);
    }
}
