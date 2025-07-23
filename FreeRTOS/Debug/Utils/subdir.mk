################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Utils/cli_handler.c \
../Utils/uart_logger.c 

OBJS += \
./Utils/cli_handler.o \
./Utils/uart_logger.o 

C_DEPS += \
./Utils/cli_handler.d \
./Utils/uart_logger.d 


# Each subdirectory must supply rules for building sources it contributes
Utils/%.o Utils/%.su Utils/%.cyclo: ../Utils/%.c Utils/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g1 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F446xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Halak Vyas/STM32CubeIDE/workspace_1.16.0/Secure_OTA_RTOS/FreeRTOS/Utils" -I"C:/Users/Halak Vyas/STM32CubeIDE/workspace_1.16.0/Secure_OTA_RTOS/FreeRTOS/Tasks" -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Utils

clean-Utils:
	-$(RM) ./Utils/cli_handler.cyclo ./Utils/cli_handler.d ./Utils/cli_handler.o ./Utils/cli_handler.su ./Utils/uart_logger.cyclo ./Utils/uart_logger.d ./Utils/uart_logger.o ./Utils/uart_logger.su

.PHONY: clean-Utils

