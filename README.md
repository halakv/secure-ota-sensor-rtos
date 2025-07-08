# Secure OTA + RTOS Sensor Firmware (STM32F446RE)

This project implements a secure boot + OTA update firmware system with FreeRTOS task management for a simulated sensor pipeline on the STM32F446RE.

## Features
- Secure Boot Simulation
- OTA Firmware Update Logic (failover + rollback)
- FreeRTOS Tasks: Sensor, Logger, OTA Handler, Watchdog
- UART Logging
- Flash Memory Management

## Tools
- STM32CubeIDE 1.16.0
- FreeRTOS CMSIS-V2
- STM32 HAL + LL

## Hardware
- STM32 Nucleo-F446RE board
- Onboard LED (PA5)
- UART2 Logging (PA2/PA3)

## Folder Structure
- `/Core/` – main app logic
- `/Tasks/` – custom RTOS task source files
- `/Drivers/` – HAL and CMSIS sources
