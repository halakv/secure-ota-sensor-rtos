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

HAL_StatusTypeDef clear_flash_protection(void)
{
    log_printf("Clearing flash protection...\r\n");

    // Clear all flash error flags
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                          FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    // Unlock flash and option bytes
    HAL_StatusTypeDef status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        log_printf("Flash unlock failed: %d\r\n", status);
        return status;
    }

    status = HAL_FLASH_OB_Unlock();
    if (status != HAL_OK) {
        log_printf("Option bytes unlock failed: %d\r\n", status);
        HAL_FLASH_Lock();
        return status;
    }

    // Check current RDP level
    uint8_t rdp_level = (FLASH->OPTCR & FLASH_OPTCR_RDP) >> FLASH_OPTCR_RDP_Pos;
    log_printf("Current RDP level: 0x%02X\r\n", rdp_level);

    // Only modify if not already at level 0 (no protection)
    if (rdp_level != 0xAA) {
        log_printf("Setting RDP to level 0 (no protection)...\r\n");

        FLASH_OBProgramInitTypeDef ob_config;
        ob_config.OptionType = OPTIONBYTE_RDP;
        ob_config.RDPLevel = OB_RDP_LEVEL_0;

        status = HAL_FLASHEx_OBProgram(&ob_config);
        if (status != HAL_OK) {
            log_printf("RDP clear failed: %d\r\n", status);
        } else {
            log_printf("RDP cleared successfully\r\n");
        }
    }

    // Clear write protection on all sectors
    log_printf("Clearing write protection on sectors...\r\n");
    FLASH_OBProgramInitTypeDef ob_config_wrp;
    ob_config_wrp.OptionType = OPTIONBYTE_WRP;
    ob_config_wrp.WRPState = OB_WRPSTATE_DISABLE;
    ob_config_wrp.WRPSector = OB_WRP_SECTOR_All;

    status = HAL_FLASHEx_OBProgram(&ob_config_wrp);
    if (status != HAL_OK) {
        log_printf("WRP clear failed: %d\r\n", status);
    } else {
        log_printf("Write protection cleared successfully\r\n");
    }

    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();

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
    
    // Clear flash protection to prevent erase failures
    clear_flash_protection();
    
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
    
//To-Do: slot_to_erase_addr inplace of boot_metadata->active_slot
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

    // Clear any pending flash errors and ensure no protection
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                          FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    HAL_StatusTypeDef status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        log_printf("Flash unlock failed during write: %d\r\n", status);
        return status;
    }

    for (uint32_t i = 0; i < len; i += 4) {
        uint32_t word = 0xFFFFFFFF;
        
        // Always copy exactly 4 bytes or remaining bytes, ensuring proper alignment
        uint32_t bytes_to_copy = (len - i >= 4) ? 4 : (len - i);
        memcpy(&word, data + i, bytes_to_copy);
        
        // Fix vector table addresses if this is the beginning of firmware and writing to different slot
        if (offset + i < 256 && target_slot_addr != SLOT_A_ADDRESS) { // Vector table is first 256 bytes
            uint32_t vector_index = (offset + i) / 4;
            
            // Skip stack pointer (vector 0), fix reset handler and other vectors (1-63)
            // Only adjust addresses that are specifically in Slot A range (0x08010000-0x0803FFFF)
            if (vector_index > 0 && word >= SLOT_A_ADDRESS && word < (SLOT_A_ADDRESS + 0x30000)) {
                uint32_t adjusted_word = word - SLOT_A_ADDRESS + target_slot_addr;
                word = adjusted_word;
            }
        }
        
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

uint32_t calculate_crc32_ota(uint32_t *data, uint32_t length_words)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint32_t i = 0; i < length_words; i++) {
        uint32_t word = data[i];
        
        // Process each byte in the word
        for (int byte = 0; byte < 4; byte++) {
            uint8_t current_byte = (word >> (byte * 8)) & 0xFF;
            crc ^= current_byte;
            
            for (int bit = 0; bit < 8; bit++) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ 0xEDB88320; // CRC32 polynomial
                } else {
                    crc >>= 1;
                }
            }
        }
    }
    
    return ~crc;
}

uint32_t calculate_flash_crc_ota(uint32_t start_addr, uint32_t size_bytes)
{
    // Ensure size is word-aligned
    uint32_t size_words = (size_bytes + 3) / 4;
    uint32_t *flash_ptr = (uint32_t *)start_addr;
    
    log_printf("CRC calc: addr=0x%08X, size=%lu bytes (%lu words)\r\n", 
               (unsigned int)start_addr, size_bytes, size_words);
    
    uint32_t crc = calculate_crc32_ota(flash_ptr, size_words);
    
    log_printf("Flash CRC32 result: 0x%08X\r\n", (unsigned int)crc);
    return crc;
}

HAL_StatusTypeDef update_boot_metadata(uint32_t new_slot, uint32_t firmware_crc, uint32_t firmware_size)
{
    // Create new metadata structure
    BootMetadata_t new_metadata;
    new_metadata.is_valid = VALID_MARKER;
    new_metadata.version = boot_metadata->version + 1; // Increment version
    new_metadata.active_slot = new_slot;
    new_metadata.crc = firmware_crc;
    new_metadata.image_size = firmware_size;
    new_metadata.reserved[0] = 0;
    new_metadata.reserved[1] = 0;
    
    log_printf("Updating metadata: slot=%lu, crc=0x%08X, size=%lu\r\n", 
               new_slot, (unsigned int)firmware_crc, firmware_size);
    
    // Clear any pending flash errors
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                          FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    
    HAL_StatusTypeDef status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        log_printf("Flash unlock failed for metadata update: %d\r\n", status);
        return status;
    }
    
    // Erase metadata sector
    // STM32F446RE sector mapping: 
    // Sector 3: 0x0800C000-0x0800FFFF (16KB) - contains metadata at 0x0800C000
    FLASH_EraseInitTypeDef eraseInit = {
        .TypeErase = FLASH_TYPEERASE_SECTORS,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
        .Sector = FLASH_SECTOR_3,  // Correct sector for 0x0800C000
        .NbSectors = 1
    };
    
    uint32_t sectorError;
    status = HAL_FLASHEx_Erase(&eraseInit, &sectorError);
    if (status != HAL_OK) {
        log_printf("Metadata sector erase failed: %d, error: 0x%08X\r\n", status, sectorError);
        HAL_FLASH_Lock();
        return status;
    }
    
    // Write new metadata word by word
    uint32_t *metadata_ptr = (uint32_t *)&new_metadata;
    uint32_t metadata_words = sizeof(BootMetadata_t) / 4;
    
    for (uint32_t i = 0; i < metadata_words; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, 
                                   METADATA_ADDRESS + (i * 4), 
                                   metadata_ptr[i]);
        if (status != HAL_OK) {
            log_printf("Metadata write failed at word %lu, status: %d\r\n", i, status);
            HAL_FLASH_Lock();
            return status;
        }
    }
    
    HAL_FLASH_Lock();
    
    // Verify the write was successful
    BootMetadata_t *verify_metadata = (BootMetadata_t *)METADATA_ADDRESS;
    if (verify_metadata->is_valid == VALID_MARKER && 
        verify_metadata->active_slot == new_slot &&
        verify_metadata->crc == firmware_crc) {
        log_printf("Metadata updated and verified successfully\r\n");
        return HAL_OK;
    } else {
        log_printf("Metadata verification failed after write!\r\n");
        log_printf("  Written - valid:0x%08X slot:%lu crc:0x%08X\r\n", 
                   VALID_MARKER, new_slot, (unsigned int)firmware_crc);
        log_printf("  Read    - valid:0x%08X slot:%lu crc:0x%08X\r\n", 
                   (unsigned int)verify_metadata->is_valid, verify_metadata->active_slot, 
                   (unsigned int)verify_metadata->crc);
        return HAL_ERROR;
    }
}

HAL_StatusTypeDef initialize_metadata()
{
    log_printf("Initializing default metadata...\r\n");
    
    BootMetadata_t default_metadata;
    default_metadata.is_valid = VALID_MARKER;
    default_metadata.version = 0x00010001;
    default_metadata.active_slot = SLOT_A;
    default_metadata.crc = 0xFFFFFFFF;    // No CRC check for initial firmware
    default_metadata.image_size = 0;      // Unknown size
    default_metadata.reserved[0] = 0;
    default_metadata.reserved[1] = 0;
    
    return update_boot_metadata(default_metadata.active_slot, 
                               default_metadata.crc, 
                               default_metadata.image_size);
}

HAL_StatusTypeDef ota_complete_and_switch(uint32_t firmware_size)
{
    // Check if metadata is valid, initialize if needed
    if (boot_metadata->is_valid != VALID_MARKER) {
        log_printf("Invalid metadata detected during OTA completion!\r\n");
        log_printf("Metadata: valid=0x%08X, slot=%lu, crc=0x%08X\r\n", 
                   (unsigned int)boot_metadata->is_valid, boot_metadata->active_slot, 
                   (unsigned int)boot_metadata->crc);
        return HAL_ERROR;
    }
    
    // Determine the target slot (opposite of current active slot)
    uint32_t new_slot = (boot_metadata->active_slot == SLOT_A) ? SLOT_B : SLOT_A;
    uint32_t new_slot_addr = (new_slot == SLOT_A) ? SLOT_A_ADDRESS : SLOT_B_ADDRESS;
    
    log_printf("OTA completion: switching from slot %lu to slot %lu\r\n", 
               boot_metadata->active_slot, new_slot);
    
    // Calculate CRC of the newly written firmware
    uint32_t firmware_crc = calculate_flash_crc_ota(new_slot_addr, firmware_size);
    log_printf("Calculated CRC for new firmware: 0x%08X\r\n", (unsigned int)firmware_crc);
    
    // Update boot metadata with new slot information
    HAL_StatusTypeDef status = update_boot_metadata(new_slot, firmware_crc, firmware_size);
    if (status != HAL_OK) {
        log_printf("Failed to update boot metadata: %d\r\n", status);
        return status;
    }
    
    log_printf("OTA complete! Next boot will use slot %lu\r\n", new_slot);
    return HAL_OK;
}

void ota_erase_slot(){
	HAL_StatusTypeDef status = erase_slot();
	if (status != HAL_OK) {
		log_printf("Slot erase failed.\r\n");
		return;
	}
	// log_printf("Slot erased successfully. Ready to receive firmware.\r\n");
}


