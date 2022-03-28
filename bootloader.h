/* bootloader.h
 * A simple bootloader for MSP430F5529.
 *
 * Copyright (c) 2014, Tomek Lorek <tlorek@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright notice
 * and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
 * OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdint.h>
#include <stdbool.h>

#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

#define ROM_CODE_SIZE 					0x04FF						// size of ROM to be copied to RAM
#define FLASH_PROGRAM_REGION_START		0x5400						// Bank A, the application code starts here
#define FLASH_PROGRAM_VECTTBL_START		0xFF80

#define FLASH_DOWNLOAD_REGION_START		0x14400						// Bank C, the application drops the new firmware here
#define FLASH_DOWNLOAD_VECTTBL_START	0x1C380

#define FLASH_BACKUP_REGION_START		0x1C400						// Bank D, backup, the bootloader copies existing image from FLASH_PROGRAM_REGION_START
#define FLASH_BACKUP_VECTTBL_START		0x24380

#define BL_IMAGE_INFO_SEG_ADDR			0x1900
#define BL_IMAGE_STATUS_FLAG_OFFSET		0							// image status flag is stored at address: BL_IMAGE_STATUS_FLAG_SEG_ADDR + BL_IMAGE_STATUS_FLAG_OFFSET

#define APP_RESET_VECTOR_ADDR			0xFF7E						// application reset vector to be stored here instead of 0xFFFE (0xFFFE is reserved for bootloader's reset vector)
#define IMAGE_APP_SIZE					32640						// bytes of the application without reset vector
#define IMAGE_VECTTBL_SIZE				128
#define IMAGE_TOTAL_SIZE				32768						// IMAGE_APP_SIZE + IMAGE_VECTTBL_SIZE; bytes of the application with reset vector, max image size - since image is stored per bank it cannot exceed bank's size

#define STATUS_FAIL 	1
#define STATUS_SUCCESS	0

typedef enum {
	BL_IMAGE_NONE,													// no image activity, nothing to be done
	BL_IMAGE_DOWNLOAD,												// new image in flash download area
	BL_IMAGE_PENDING_VALIDATION,									// image flashed by bootloader, waiting to be validated by application. If the bootloader reads this value upon startup it performs image recovery
	BL_IMAGE_VALIDATED,												// image validated by application
	BL_IMAGE_RECOVERED,												// image recovered from backup region as a result of non-validated image
	BL_IMAGE_FLASHING_ERROR											// image flashing could not be completed
} bl_image_status_t;

void SetImageStatusFlag(bl_image_status_t status);
bl_image_status_t GetImageStatusFlag();
void McuReset();

#endif /* BOOTLOADER_H_ */
