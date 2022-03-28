/* bootloader.c
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

#include <msp430.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "bootloader.h"
#include "flash.h"

void (*app_func)() = (void*)FLASH_PROGRAM_REGION_START;

void SetImageStatusFlag(bl_image_status_t status)
{
	const size_t info_seg_size = 128;
	uint8_t i;

	// store current info segment
	uint8_t* seg_copy_ptr;
	seg_copy_ptr = (uint8_t*)malloc(info_seg_size);					// allocate memory to store info segment copy

	if (seg_copy_ptr != NULL)
	{
		for (i = 0; i < info_seg_size; i++)
		{
			*(seg_copy_ptr + i) = flashReadByte(BL_IMAGE_INFO_SEG_ADDR + i);
		}

		// modify the segment with new status
		*(seg_copy_ptr + BL_IMAGE_STATUS_FLAG_OFFSET) = status;

		// erase info segment
		FlashErase(BL_IMAGE_INFO_SEG_ADDR, ERASE);

		// write info segment from saved one
		FCTL3 = FWKEY;												// Clear Lock bit
		FCTL1 = FWKEY + WRT;										// Enable byte/word write mode

		while (FCTL3 & BUSY) ;										// test busy

		for (i = 0; i < info_seg_size; i++)
		{
			flashWriteByte(BL_IMAGE_INFO_SEG_ADDR + i, *(seg_copy_ptr + i));
		}

		FCTL1 = FWKEY;												// Clear WRT bit
		FCTL3 = FWKEY + LOCK;										// Set LOCK bit

		while (FCTL3 & BUSY) ;										// test busy

		free(seg_copy_ptr);
	}
}

bl_image_status_t GetImageStatusFlag()
{
	bl_image_status_t status;
	uint8_t *bl_image_status_ptr = (uint8_t*)(BL_IMAGE_INFO_SEG_ADDR + BL_IMAGE_STATUS_FLAG_OFFSET);
	status = *bl_image_status_ptr;

	return status;
}

bool reflash()
{
	uint32_t flash_addr, data_addr;
	uint16_t i, bootloader_reset_vector;
	uint16_t* bootloader_reset_vector_ptr;

	bootloader_reset_vector_ptr = (uint16_t*)0xFFFE;
	bootloader_reset_vector = *bootloader_reset_vector_ptr;

	///////////////////////////////////////////////////////
	// 1. COPY PROGRAM TO BACKUP AREA
	///////////////////////////////////////////////////////

	// 1.1 erase backup region (bank erase)

	FlashErase(FLASH_BACKUP_REGION_START, MERAS);

	P1OUT |= BIT0;

	if (flashEraseCheck((uint32_t)FLASH_BACKUP_REGION_START, IMAGE_TOTAL_SIZE)) // verify if backup region is erased
		return STATUS_FAIL;

	// 1.2 copy current program region to backup region

	// 1.2.1 copy application part

	FCTL3 = FWKEY;												// Clear Lock bit
	FCTL1 = FWKEY + WRT;										// Enable byte/word write mode

	// copy
	data_addr = (uint32_t)FLASH_PROGRAM_REGION_START;
	flash_addr = (uint32_t)FLASH_BACKUP_REGION_START;
	i = IMAGE_APP_SIZE;
	while (i > 0)
	{
		while (FCTL3 & BUSY) ;									// test busy
		flashWriteWord(flash_addr, flashReadWord(data_addr));

		flash_addr += 2;
		data_addr += 2;
		i -= 2;
	}

	// verify
	data_addr = (uint32_t)FLASH_PROGRAM_REGION_START;
	flash_addr = (uint32_t)FLASH_BACKUP_REGION_START;
	i = IMAGE_APP_SIZE;
	while (i > 0)
	{
		while (FCTL3 & BUSY) ;									// test busy
		if (flashReadWord(data_addr) != flashReadWord(flash_addr))
			return STATUS_FAIL;

		flash_addr += 2;
		data_addr += 2;
		i -= 2;
	}

	// 1.2.2 copy vector table

	data_addr = (uint32_t)FLASH_PROGRAM_VECTTBL_START;
	flash_addr = (uint32_t)FLASH_BACKUP_VECTTBL_START;

	i = IMAGE_VECTTBL_SIZE;
	while (i > 0)
	{
		// copy
		while (FCTL3 & BUSY) ;									// test busy

		if (data_addr == 0xFFFE)								// write application's reset vector not the bootloader's one
			flashWriteWord(flash_addr, flashReadWord(APP_RESET_VECTOR_ADDR));
		else
		{
			flashWriteWord(flash_addr, flashReadWord(data_addr));

			// verify
			while (FCTL3 & BUSY) ;									// test busy
			if (flashReadWord(data_addr) != flashReadWord(flash_addr))
				return STATUS_FAIL;
		}

		flash_addr += 2;
		data_addr += 2;
		i -= 2;
	}

	FCTL1 = FWKEY;												// Clear WRT bit
	FCTL3 = FWKEY + LOCK;										// Set LOCK bit

	P4OUT |= BIT7;

	///////////////////////////////////////////////////////
	// 2. COPY IMAGE FROM DOWNLOAD REGION TO PROGRAM REGION (REPLACE IMAGE)
	///////////////////////////////////////////////////////

	// 2.1 erase program region, segment erase starting from FLASH_PROGRAM_REGION_START

	uint16_t seg_cnt = 												// count of 512-byte segments in occupied by application, vector table and gap between them
			(((FLASH_PROGRAM_VECTTBL_START + IMAGE_VECTTBL_SIZE - FLASH_PROGRAM_REGION_START) - 1) >> 9) + 1;

	for (i = 0; i < seg_cnt; i++)									// while iterating through seg_cnt skip the last one so not to write beyond the image area
	{
		FlashErase((uint32_t)(FLASH_PROGRAM_REGION_START + (i << 9)), ERASE);
	}

	if (flashEraseCheck((uint32_t)FLASH_PROGRAM_REGION_START, FLASH_PROGRAM_VECTTBL_START + IMAGE_VECTTBL_SIZE - FLASH_PROGRAM_REGION_START)) // verify if program region is erased
		return STATUS_FAIL;

	// 2.2 copy download region to program region

	FCTL3 = FWKEY;											// Clear Lock bit
	FCTL1 = FWKEY + WRT;									// Enable byte/word write mode

	// 2.2.1 write bootloader's reset vector value to reset vector (0xFFFE)

	while (FCTL3 & BUSY) ;							// test busy
	flashWriteWord((uint32_t)0xFFFE, bootloader_reset_vector);	// restore bootloader's reset vector

	P4OUT &= ~BIT7;

	// 2.2.2 copy application part

	// copy
	data_addr = (uint32_t)FLASH_DOWNLOAD_REGION_START;
	flash_addr = (uint32_t)FLASH_PROGRAM_REGION_START;
	i = IMAGE_APP_SIZE;
	while (i > 0)
	{
		// copy
		while (FCTL3 & BUSY) ;									// test busy
		flashWriteWord(flash_addr, flashReadWord(data_addr));

		flash_addr += 2;
		data_addr += 2;
		i -= 2;
	}

	// verify
	data_addr = (uint32_t)FLASH_DOWNLOAD_REGION_START;
	flash_addr = (uint32_t)FLASH_PROGRAM_REGION_START;
	i = IMAGE_APP_SIZE;
	while (i > 0)
	{
		while (FCTL3 & BUSY) ;									// test busy
		if (flashReadWord(data_addr) != flashReadWord(flash_addr))
			return STATUS_FAIL;

		flash_addr += 2;
		data_addr += 2;
		i -= 2;
	}

	// 2.2.3 copy vector table

	data_addr = (uint32_t)FLASH_DOWNLOAD_VECTTBL_START;
	flash_addr = (uint32_t)FLASH_PROGRAM_VECTTBL_START;

	FCTL3 = FWKEY;										// Clear Lock bit
	FCTL1 = FWKEY + WRT;								// Enable byte/word write mode

	i = IMAGE_VECTTBL_SIZE;
	while (i > 0)
	{
		while (FCTL3 & BUSY) ;							// test busy

		// redirect application's reset vector (2 bytes) to APP_RESET_VECTOR_ADDR
		if (flash_addr == 0xFFFE)
			flashWriteWord(APP_RESET_VECTOR_ADDR, flashReadWord(data_addr));
		else
		{
			flashWriteWord(flash_addr, flashReadWord(data_addr));

			// verify
			while (FCTL3 & BUSY) ;									// test busy
			if (flashReadWord(data_addr) != flashReadWord(flash_addr))
				return STATUS_FAIL;
		}

		flash_addr += 2;
		data_addr += 2;
		i -= 2;
	}

	FCTL1 = FWKEY;										// Clear WRT bit
	FCTL3 = FWKEY + LOCK;								// Set LOCK bit

	return STATUS_SUCCESS;
}

bool recover()
{
	uint32_t flash_addr, data_addr;
	uint16_t i, bootloader_reset_vector;
	uint16_t* bootloader_reset_vector_ptr;

	bootloader_reset_vector_ptr = (uint16_t*)0xFFFE;
	bootloader_reset_vector = *bootloader_reset_vector_ptr;

	///////////////////////////////////////////////////////
	// 1. COPY IMAGE FROM BACKUP REGION TO PROGRAM REGION (REPLACE IMAGE)
	///////////////////////////////////////////////////////

	// 1.1 erase program region, segment erase starting from FLASH_PROGRAM_REGION_START

	uint16_t seg_cnt = 												// count of 512-byte segments in occupied by application, vector table and gap between them
			(((FLASH_PROGRAM_VECTTBL_START + IMAGE_VECTTBL_SIZE - FLASH_PROGRAM_REGION_START) - 1) >> 9) + 1;

	for (i = 0; i < seg_cnt; i++)									// while iterating through seg_cnt skip the last one so not to write beyond the image area
	{
		FlashErase((uint32_t)(FLASH_PROGRAM_REGION_START + (i << 9)), ERASE);
	}

	// 1.2 copy backup region to program region

	FCTL3 = FWKEY;											// Clear Lock bit
	FCTL1 = FWKEY + WRT;									// Enable byte/word write mode

	// 1.2.1 write bootloader's reset vector value to reset vector (0xFFFE)

	while (FCTL3 & BUSY) ;							// test busy
	flashWriteWord((uint32_t)0xfffe, bootloader_reset_vector);	// restore bootloader's reset vector

	// 1.2.2 copy application part

	data_addr = (uint32_t)FLASH_BACKUP_REGION_START;
	flash_addr = (uint32_t)FLASH_PROGRAM_REGION_START;

	i = IMAGE_APP_SIZE;
	while (i > 0)
	{
		while (FCTL3 & BUSY) ;									// test busy
		flashWriteWord(flash_addr, flashReadWord(data_addr));

		flash_addr += 2;
		data_addr += 2;
		i -= 2;
	}

	// 1.2.3 copy vector table

	data_addr = (uint32_t)FLASH_BACKUP_VECTTBL_START;
	flash_addr = (uint32_t)FLASH_PROGRAM_VECTTBL_START;

	FCTL3 = FWKEY;										// Clear Lock bit
	FCTL1 = FWKEY + WRT;								// Enable byte/word write mode

	i = IMAGE_VECTTBL_SIZE;
	while (i > 0)
	{
		while (FCTL3 & BUSY) ;							// test busy

		// redirect application's reset vector (2 bytes) to APP_RESET_VECTOR_ADDR
		if (flash_addr == 0xFFFE)
			flashWriteWord((uint32_t)APP_RESET_VECTOR_ADDR, flashReadWord(data_addr));
		else
			flashWriteWord(flash_addr, flashReadWord(data_addr));

		flash_addr += 2;
		data_addr += 2;
		i -= 2;
	}

	FCTL1 = FWKEY;										// Clear WRT bit
	FCTL3 = FWKEY + LOCK;								// Set LOCK bit

	return STATUS_SUCCESS;
}

int main()
{
	while (true)
	{
		WDTCTL = WDTPW + WDTHOLD;									// Stop WDT

		//////////////////////////////////////////////////////////////////////////////////////
		// ACLK = XT1 = 32768 Hz
		// MCLK = SMCLK = XT2 = 4.0 MHz

		//////////////////////// use XT1 32768 Hz ////////////////////////////////////////////
		P5SEL |= BIT4 + BIT5;										// Select XT1
		UCSCTL6 &= ~XT1OFF;											// Enable XT1
		UCSCTL6 |= XCAP_3;											// Internal load cap

		//////////////////////// use XT2 4.0 MHz ////////////////////////////////////////////
		P5SEL |= BIT2 + BIT3;										// Select XT2

		UCSCTL6 &= ~XT2OFF;											// Enable XT2
		UCSCTL3 |= SELREF__XT1CLK;									// FLLref = XT1

		do															// Loop until XT1 and XT2 stabilize
		{
			UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + DCOFFG);				// Clear XT2, XT1 fault flags
			SFRIFG1 &= ~OFIFG;										// Clear oscillator fault flag
		} while (SFRIFG1 & OFIFG);									// Test oscillator fault flag

		UCSCTL6 &= ~(XT2DRIVE0 + XT2DRIVE1);						// Decrease XT2 Drive as it is stabilized
		UCSCTL6 &= ~(XT1DRIVE0 + XT1DRIVE1);						// Decrease XT1 Drive as it is stabilized

		UCSCTL4 |= SELA__XT1CLK + SELS__XT2CLK + SELM__XT2CLK;		// ACLK = XT1, MCLK = SMCLK = XT2
		//////////////////////////////////////////////////////////////////////////////////

		//debug GREEN led
		P4DIR |= BIT7;
		P4OUT &= ~BIT7;

		//debug RED led
		P1DIR |= BIT0;
		P1OUT &= ~BIT0;

		__dint();

		bl_image_status_t status = GetImageStatusFlag();
		uint16_t *ram_ptr;

		switch (status)
		{
		case BL_IMAGE_DOWNLOAD:															// new image in download region, reprogram
			// copy reflashing function to RAM
			ram_ptr = (void*)malloc((size_t) ROM_CODE_SIZE);

			if (ram_ptr != NULL)
			{
				memcpy(ram_ptr, (void*)&reflash, ROM_CODE_SIZE);

				bool (*func_in_ram)() = (void*)ram_ptr;

				bool reflash_status = func_in_ram();									// call the reprogramming function residing in RAM

				free(ram_ptr);

				if (reflash_status == STATUS_SUCCESS)
				{
					SetImageStatusFlag(BL_IMAGE_PENDING_VALIDATION);
				}
				else
				{
					SetImageStatusFlag(BL_IMAGE_FLASHING_ERROR);
				}
			}
			break;

		case BL_IMAGE_PENDING_VALIDATION:												// image not validated by the application, recover image from backup
			P1OUT |= BIT0;
			P4OUT |= BIT7;

			ram_ptr = (void*)malloc((size_t) ROM_CODE_SIZE);

			if (ram_ptr != NULL)
			{
				memcpy(ram_ptr, (void*)&recover, ROM_CODE_SIZE);

				bool (*func_in_ram)() = (void*)ram_ptr;

				bool recovery_status = func_in_ram();									// call the reprogramming function residing in RAM

				free(ram_ptr);

				if (recovery_status == STATUS_SUCCESS)
				{
					SetImageStatusFlag(BL_IMAGE_RECOVERED);
				}
				else
				{
					SetImageStatusFlag(BL_IMAGE_FLASHING_ERROR);
				}
				McuReset();
			}
			break;

		case BL_IMAGE_VALIDATED:														// image validated by the application, clear status flag
			SetImageStatusFlag(BL_IMAGE_NONE);
			break;

		case BL_IMAGE_RECOVERED:
		case BL_IMAGE_FLASHING_ERROR:
		case BL_IMAGE_NONE:
			break;
		}

		WDTCTL = WDTPW + WDTSSEL__ACLK + WDTIS__8192K;									// WDT set for 00h:04m:16s  at ACLK

		// call application
		volatile uint16_t *app_reset_vector_ptr = (uint16_t*)APP_RESET_VECTOR_ADDR;
		volatile uint16_t app_reset_vector = *app_reset_vector_ptr;
		void (*app_func)() = (void*)app_reset_vector;

		app_func();
	}

	return 0;
}

void McuReset()
{
	PMMCTL0 = PMMPW + PMMSWBOR + PMMCOREV_2;
}

