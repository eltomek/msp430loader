/* flash.c
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
#include <stdint.h>
#include <stdbool.h>
#include "flash.h"
#include "bootloader.h"

inline void FlashErase(uint32_t address, uint32_t mode)
{
	while (FCTL3 & BUSY) ;
	FCTL3 = FWPW;												// Clear Lock bit
	FCTL1 = FWPW + mode;										// Set erase mode bit
	flashWriteByte(address, 0);									// Dummy write to erase flash segment
	while (FCTL3 & BUSY) ;										// test busy
	FCTL1 = FWKEY;												// Clear erase mode bit
	FCTL3 = FWKEY + LOCK;										// Set LOCK bit
}

// The _static inline_ functions below MUST be inlined in the target code, otherwise writing to flash bank A will corrupt it
inline uint8_t flashReadByte(uint32_t address)
{
	uint8_t result;
	uint16_t register sr, flash;

	__asm__ __volatile__ ("mov r2,%0":"=r"(sr):);				// save SR before disabling IRQ

	__asm__ __volatile__ ("movx.a %1, %0":"=r"(flash):"m"(address));
	__asm__ __volatile__ ("movx.b @%1, %0":"=m"(result):"r"(flash));
	__asm__ __volatile__ ("mov %0,r2"::"r"(sr));				// restore previous SR and IRQ state

	return result;
}

inline uint16_t flashReadWord(uint32_t address)
{
	uint16_t result;
	uint16_t register sr, flash;

	__asm__ __volatile__ ("mov r2,%0":"=r"(sr):);				// save SR before disabling IRQ

	__asm__ __volatile__ ("movx.a %1, %0":"=r"(flash):"m"(address));
	__asm__ __volatile__ ("movx.w @%1, %0":"=m"(result):"r"(flash));
	__asm__ __volatile__ ("mov %0,r2"::"r"(sr));				// restore previous SR and IRQ state

	return result;
}

inline void flashWriteByte(uint32_t address, uint8_t byte)
{
	uint16_t register sr, flash;
	__asm__ __volatile__ ("mov r2,%0":"=r"(sr):);				// save SR before disabling IRQ

	__asm__ __volatile__ ("movx.a %1,%0":"=r"(flash):"m"(address));
	__asm__ __volatile__ ("movx.b %1, @%0":"=r"(flash):"m"(byte));
	__asm__ __volatile__ ("mov %0,r2"::"r"(sr));				// restore previous SR and IRQ state
}

inline void flashWriteWord(uint32_t address, uint16_t byte)
{
	uint16_t register sr, flash;
	__asm__ __volatile__ ("mov r2,%0":"=r"(sr):);				// save SR before disabling IRQ

	__asm__ __volatile__ ("movx.a %1,%0":"=r"(flash):"m"(address));
	__asm__ __volatile__ ("movx.w %1, @%0":"=r"(flash):"m"(byte));
	__asm__ __volatile__ ("mov %0,r2"::"r"(sr));				// restore previous SR and IRQ state
}

inline bool flashEraseCheck(uint32_t flashAddr, uint16_t numberOfBytes)
{
	uint16_t i;

	if (numberOfBytes & 1)								// odd bytes number, use byte access
	{
		uint8_t val;
		for (i = 0; i < numberOfBytes; i++)
		{
			val = flashReadByte(flashAddr + i);
			if (val != 0xFF)
				return STATUS_FAIL;
		}
	}
	else												// even bytes number, use word access
	{
		uint16_t val;
		for (i = 0; i < numberOfBytes; i += 2)
		{
			val = flashReadWord(flashAddr + i);
			if (val != 0xFFFF)
				return STATUS_FAIL;
		}
	}
	return STATUS_SUCCESS;
}
