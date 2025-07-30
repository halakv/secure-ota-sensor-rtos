/*
 * ota.h
 *
 *  Created on: Jul 23, 2025
 *      Author: Halak Vyas
 */

#ifndef OTA_H_
#define OTA_H_

void ota_erase_slot();
HAL_StatusTypeDef ota_write_firmware(uint32_t offset, uint8_t *data, uint32_t len);
uint32_t calculate_crc32_ota(uint32_t *data, uint32_t length_words);
uint32_t calculate_flash_crc_ota(uint32_t start_addr, uint32_t size_bytes);
HAL_StatusTypeDef update_boot_metadata(uint32_t new_slot, uint32_t firmware_crc, uint32_t firmware_size);
HAL_StatusTypeDef initialize_metadata();
HAL_StatusTypeDef ota_complete_and_switch(uint32_t firmware_size);
HAL_StatusTypeDef clear_flash_protection(void);

#endif /* OTA_H_ */
