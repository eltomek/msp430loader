/* flash.h
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

#ifndef FLASH_H_
#define FLASH_H_

#include <stdint.h>
#include <stdbool.h>

inline void FlashErase(uint32_t address, uint32_t mode);

inline uint8_t flashReadByte(uint32_t address);
inline uint16_t flashReadWord(uint32_t address);
inline void flashWriteByte(uint32_t address, uint8_t byte);
inline void flashWriteWord(uint32_t address, uint16_t byte);
inline bool flashEraseCheck(uint32_t flashAddr, uint16_t numberOfBytes);

#endif /* FLASH_H_ */
