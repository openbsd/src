/*	$OpenBSD: ds.h,v 1.1 1997/09/13 14:16:19 niklas Exp $	*/
/*	$NetBSD: ds.h,v 1.1 1997/07/06 22:22:01 is Exp $	*/

/*-
 * Copyright (c) 1997 Ignatios Souvatzis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Definitions for access to Dallas Semiconductor chips which attach to 
 * the same 1-wire bus as the DS2404 RTC.
 */

#ifndef DALLAS_SEMI_CHIPS_H
#define DALLAS_SEMI_CHIPS_H

/* Family codes (low byte of the ROM) */

#define DS_FAMILY_2404	0x04	/* DS2404 Econoram Time Chip */

/*
 * ROM access codes. These are only available from the 1-wire bus, and one
 * of them MUST be used before a memory access code is called. If you want
 * to detect which devices are on the bus, you have to issue the ROM search
 * function (see data sheet).
 * If only one device is on the BUS, and you don't want any ROM function,
 * issue the SKIP function.
 * READ ROM works only if only one device is on the bus.
 */

#define DS_ROM_MATCH		0x55	/* 55 8-bytes-of-ROM-to-select */
#define DS_ROM_SEARCH		0xf0	/* see data sheet */
#define DS_ROM_SKIP		0xCC	/* don't do ROM function */
#define DS_ROM_READ		0x33	/* 33 -> 8 bytes of ROM */

/*
 * Memory access codes. These are available from the 1- or 3-wire bus, and
 * but you must use one of the ROM access codes first, if using the 1-wire 
 * bus.
 *
 * You can read from any starting address up to the end of the chip, or
 * abort the read with a reset pulse.
 * You first write 2-32 bytes beginning some address to the scratchpad.
 * Starting address and final byte stream length are remembered by the 
 * chip. After reading data and address/length back from the scratchpad,
 * and verifying the information, you can issue the copy scratchpad command
 * to copy the written parts of the scratchpad to the corresponding parts
 * of the implied (in the address) memory page.
 */

#define DS_MEM_WRITE_SCRATCH	0x0f	/* 0F low-ads high-ads data ... */
#define DS_MEM_READ_SCRATCH	0xaa	/* AA -> low-ads high-ads end-ofs 
					 * data ... */
#define DS_MEM_COPY_SCRATCH	0x55	/* 55 low-ads high-ads end-ofs */
#define DS_MEM_READ_MEMORY	0xf0	/* F0 low-ads high-ads -> data ...*/

/*
 * Hardware handle for access functions
 */

struct ds_handle {
	int (*ds_read_bit) __P((void *));
	void (*ds_write_bit) __P((void *, int));
	void (*ds_reset) __P((void *));
	void *ds_hw_handle;
};

/*
 * Functions for access to Dallas Semiconductor chips which attach to 
 * the same 1-wire bus as the DS2404 RTC.
 */

static __inline u_int8_t ds_read_byte __P((struct ds_handle *));
static __inline void ds_write_byte __P((struct ds_handle *, unsigned int));

static __inline u_int8_t
ds_read_byte(dsh)
	struct ds_handle *dsh;
{
	u_int8_t buf;
	int i;

	for (i = buf = 0; i < 8; ++i)
		buf |= (dsh->ds_read_bit)(dsh->ds_hw_handle) << i;

	return buf;
}

static __inline void
ds_write_byte(dsh, b)
	struct ds_handle *dsh;
	unsigned int b;
{
	int i;

	for (i = 0; i < 8; ++i) {
		(dsh->ds_write_bit)(dsh->ds_hw_handle, b & 1);
		b >>= 1;
	}
}

#endif /* DALLAS_SEMI_CHIPS_H */
