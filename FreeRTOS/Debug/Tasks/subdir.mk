################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Tasks/app_tasks.c 

OBJS += \
./Tasks/app_tasks.o 

C_DEPS += \
./Tasks/app_tasks.d 


# Each subdirectory must supply rules for building sources it contributes
Tasks/%.o Tasks/%.su Tasks/%.cyclo: ../Tasks/%.c Tasks/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g1 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F446xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Halak Vyas/STM32CubeIDE/workspace_1.16.0/Secure_OTA_RTOS/FreeRTOS/Utils" -I"C:/Users/Halak Vyas/STM32CubeIDE/workspace_1.16.0/Secure_OTA_RTOS/FreeRTOS/Tasks" -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Tasks

clean-Tasks:
	-$(RM) ./Tasks/app_tasks.cyclo ./Tasks/app_tasks.d ./Tasks/app_tasks.o ./Tasks/app_tasks.su

.PHONY: clean-Tasks

