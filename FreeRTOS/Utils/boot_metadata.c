/*
 * boot_metadata.c
 *
 *  Created on: Jul 25, 2025
 *      Author: Halak Vyas
 */

#include "boot_metadata.h"
#include "stm32f4xx_hal.h"

BootMetadata_t *boot_metadata = (BootMetadata_t *)0x0800C000;  // METADATA address
uint32_t slot_to_erase_addr = 0;
uint32_t sectors_to_erase[2] = {0};

bool ota_slot_check(){
	if(boot_metadata->is_valid != VALID_MARKER){
		return false;
	}
	if(boot_metadata->active_slot == SLOT_A){
		slot_to_erase_addr = SLOT_B_ADDRESS;
		sectors_to_erase[0] = FLASH_SECTOR_6;
		sectors_to_erase[1] = FLASH_SECTOR_7;
	}
	else{
		slot_to_erase_addr = SLOT_A_ADDRESS;
		sectors_to_erase[0] = FLASH_SECTOR_4;
		sectors_to_erase[1] = FLASH_SECTOR_5;
	}
	// log_printf("Active slot = %d, Erasing sectors %d, %d\r\n",
	//             boot_metadata->active_slot,
	//             sectors_to_erase[0], sectors_to_erase[1]);
	return true;
}
