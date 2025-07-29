/*
 * ota.c
 *
 *  Created on: Jul 23, 2025
 *      Author: Halak Vyas
 */
#include "stm32f4xx_hal.h"
#include "boot_metadata.h"
#include "uart_logger.h"
#include <string.h>

// Function to wait for flash operations to complete
static HAL_StatusTypeDef wait_for_flash_ready(uint32_t timeout_ms)
{
    uint32_t start_tick = HAL_GetTick();
    
    while ((FLASH->SR & FLASH_SR_BSY) != 0) {
        if ((HAL_GetTick() - start_tick) > timeout_ms) {
            log_printf("Flash timeout waiting for ready\r\n");
            return HAL_TIMEOUT;
        }
    }
    return HAL_OK;
}

//extern uint32_t slot_to_erase_addr;
//extern uint32_t sectors_to_erase[2];
FLASH_EraseInitTypeDef eraseInit;

HAL_StatusTypeDef erase_slot()
{
    // Wait for any ongoing flash operations to complete
    HAL_StatusTypeDef ready_status = wait_for_flash_ready(5000);
    if (ready_status != HAL_OK) {
        return ready_status;
    }
    
    // Clear any pending flash errors
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                          FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    
    HAL_StatusTypeDef unlock_status = HAL_FLASH_Unlock();
    if (unlock_status != HAL_OK) {
        log_printf("Flash unlock failed: %d\r\n", unlock_status);
        return unlock_status;
    }

    uint32_t sectorError;
    if (!ota_slot_check()) {
    	log_printf("Invalid metadata! Aborting erase.\r\n");
        HAL_FLASH_Lock();
        return HAL_ERROR;
    }

    // Disable interrupts during critical flash operations
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    for (int i = 0; i < 2; i++) {
        eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
        eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        eraseInit.Sector = sectors_to_erase[i];
        eraseInit.NbSectors = 1;

        log_printf("Erasing sector %d...\r\n", sectors_to_erase[i]);
        HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&eraseInit, &sectorError);
        if(status != HAL_OK){
            log_printf("Erase failed for sector %d, status: %d, error: 0x%08X\r\n", 
                      sectors_to_erase[i], status, sectorError);
            
            // Check for specific error flags
            uint32_t flash_sr = FLASH->SR;
            log_printf("FLASH_SR register: 0x%08X\r\n", flash_sr);
            
            // Restore interrupts
            if (!primask) {
                __enable_irq();
            }
            
            HAL_FLASH_Lock();
        	return status;
        }
        log_printf("Sector %d erased successfully\r\n", sectors_to_erase[i]);
    }

    // Restore interrupts
    if (!primask) {
        __enable_irq();
    }

    HAL_FLASH_Lock();
    return HAL_OK;
}

HAL_StatusTypeDef ota_write_firmware(uint32_t offset, uint8_t *data, uint32_t len)
{
    if (!ota_slot_check()) {
        log_printf("Invalid metadata! Aborting write.\r\n");
        return HAL_ERROR;
    }

    uint32_t target_slot_addr;
    uint32_t slot_end_addr;
    
    if (boot_metadata->active_slot == SLOT_A) {
        target_slot_addr = SLOT_B_ADDRESS;
        slot_end_addr = SLOT_B_ADDRESS + 0x30000;
    } else {
        target_slot_addr = SLOT_A_ADDRESS;
        slot_end_addr = SLOT_A_ADDRESS + 0x30000;
    }

    if ((target_slot_addr + offset + len) > slot_end_addr) {
        log_printf("Write would exceed slot boundary.\r\n");
        return HAL_ERROR;
    }

    // Clear any pending flash errors
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                          FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    HAL_StatusTypeDef status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        log_printf("Flash unlock failed during write: %d\r\n", status);
        return status;
    }

    for (uint32_t i = 0; i < len; i += 4) {
        uint32_t word = 0xFFFFFFFF;
        
        memcpy(&word, data + i, (len - i >= 4) ? 4 : len - i);
        
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, target_slot_addr + offset + i, word);
        if (status != HAL_OK) {
            log_printf("Flash write failed at offset 0x%08X, status: %d\r\n", offset + i, status);
            
            // Check for specific error flags
            uint32_t flash_sr = FLASH->SR;
            log_printf("FLASH_SR register: 0x%08X\r\n", flash_sr);
            
            break;
        }
    }

    HAL_FLASH_Lock();
    return status;
}

void ota_erase_slot(){
	HAL_StatusTypeDef status = erase_slot();
	if (status != HAL_OK) {
		log_printf("Slot erase failed.\r\n");
		return;
	}
	// log_printf("Slot erased successfully. Ready to receive firmware.\r\n");
}


