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

#endif /* OTA_H_ */
