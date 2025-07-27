/*
 * boot_metadata.h
 *
 *  Created on: Jul 25, 2025
 *      Author: Halak Vyas
 */

#ifndef BOOT_METADATA_H_
#define BOOT_METADATA_H_

#include <stdbool.h>
#include "main.h"

#define METADATA_ADDRESS  0x08007800
#define VALID_MARKER	  0xA5A5A5A5

#define SLOT_A_ADDRESS  0x08010000
#define SLOT_B_ADDRESS  0x08040000

#define SLOT_A_SECTORS    {FLASH_SECTOR_4, FLASH_SECTOR_5}
#define SLOT_B_SECTORS    {FLASH_SECTOR_6, FLASH_SECTOR_7}

typedef enum {
    SLOT_A = 0,
    SLOT_B = 1
} Slot_t;

typedef struct {
    uint32_t is_valid;       // 0xA5A5A5A5 if valid
    uint32_t version;
    Slot_t active_slot;
    uint32_t crc;            // Expected CRC of active image
} BootMetadata_t;

extern BootMetadata_t *boot_metadata;
extern uint32_t slot_to_erase_addr;
extern uint32_t sectors_to_erase[2];

bool ota_slot_check();

#endif /* BOOT_METADATA_H_ */
