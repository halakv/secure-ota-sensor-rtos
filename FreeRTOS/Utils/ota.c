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

//extern uint32_t slot_to_erase_addr;
//extern uint32_t sectors_to_erase[2];
FLASH_EraseInitTypeDef eraseInit;

HAL_StatusTypeDef erase_slot()
{
    HAL_FLASH_Unlock();

    uint32_t sectorError;
    if (!ota_slot_check()) {
    	log_printf("Invalid metadata! Aborting erase.\r\n");
        HAL_FLASH_Lock();
        return HAL_ERROR;
    }

    for (int i = 0; i < 2; i++) {
        eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
        eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        eraseInit.Sector = sectors_to_erase[i];
        eraseInit.NbSectors = 1;

        int status = HAL_FLASHEx_Erase(&eraseInit, &sectorError);
        if(status != HAL_OK){
        	HAL_FLASH_Lock();
        	return status;
        }
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

    HAL_StatusTypeDef status = HAL_OK;
    HAL_FLASH_Unlock();

    for (uint32_t i = 0; i < len; i += 4) {
        uint32_t word = 0xFFFFFFFF;
        
        memcpy(&word, data + i, (len - i >= 4) ? 4 : len - i);
        
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, target_slot_addr + offset + i, word);
        if (status != HAL_OK) {
            log_printf("Flash write failed at offset 0x%08X\r\n", offset + i);
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


