/*
 * ota.c
 *
 *  Created on: Jul 23, 2025
 *      Author: Halak Vyas
 */
#include "stm32f4xx_hal.h"
#include "boot_metadata.h"

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
        log_printf("Erasing sector %lu\r\n", eraseInit.Sector);

        int status = HAL_FLASHEx_Erase(&eraseInit, &sectorError);
        if(status != HAL_OK){
        	HAL_FLASH_Lock();
        	return status;
        }
    }


    HAL_FLASH_Lock();
    return HAL_OK;
}

void ota_write(){
	HAL_StatusTypeDef status = erase_slot();
	log_printf("ota write status %d \r\n",status);
}




//HAL_StatusTypeDef write_image_to_slot_b(uint32_t offset, uint8_t *data, uint32_t len)
//{
//    if ((SLOT_B_START_ADDR + offset + len) > SLOT_B_END_ADDR)
//        return HAL_ERROR;
//
//    HAL_StatusTypeDef status = HAL_OK;
//
//    HAL_FLASH_Unlock();
//
//    for (uint32_t i = 0; i < len; i += 4) {
//        uint32_t word = 0xFFFFFFFF;
//
//        // Ensure 4-byte alignment
//        memcpy(&word, data + i, (len - i >= 4) ? 4 : len - i);
//
//        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, SLOT_B_START_ADDR + offset + i, word);
//        if (status != HAL_OK) {
//            break;
//        }
//    }
//
//    HAL_FLASH_Lock();
//    return status;
//}


