/*	$OpenBSD: getsecs.c,v 1.1 2013/10/28 22:13:12 miod Exp $	*/
/*	$NetBSD: getsecs.c,v 1.1 2013/01/13 14:10:55 tsutsui Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <luna88k/stand/boot/samachdep.h>
#include <machine/board.h>
#include <luna88k/dev/timekeeper.h>

#define _DS_GET(off, data) \
	do { *chiptime = (off); (data) = (*chipdata); } while (0)
#define _DS_SET(off, data) \
	do { *chiptime = (off); *chipdata = (u_int8_t)(data); } while (0)

time_t
getsecs(void)
{
	u_int t;

	if (machtype == LUNA_88K) {
		volatile uint32_t *mclock =
		    (volatile uint32_t *)(0x45000000 + MK_NVRAM_SPACE);
		mclock[MK_CSR] |= MK_CSR_READ << 24;
		t =  bcdtobin(mclock[MK_SEC] >> 24);
		t += bcdtobin(mclock[MK_MIN] >> 24) * 60;
		t += bcdtobin(mclock[MK_HOUR] >> 24) * 60 * 60;
		mclock[MK_CSR] &= ~(MK_CSR_READ << 24);
	} else {
		volatile uint8_t *chiptime = (volatile uint8_t *)0x45000000;
		volatile u_int8_t *chipdata = chiptime + 1;

		uint8_t c;

		/* specify 24hr and BCD mode */
		_DS_GET(DS_REGB, c);
		c |= DS_REGB_24HR;
		c &= ~DS_REGB_BINARY;
		_DS_SET(DS_REGB, c);

		/* update in progress; spin loop */
		*chiptime = DS_REGA;
		while (*chipdata & DS_REGA_UIP)
			;

		*chiptime = DS_SEC;
		t =  bcdtobin(*chipdata);
		*chiptime = DS_MIN;
		t += bcdtobin(*chipdata) * 60;
		*chiptime = DS_HOUR;
		t += bcdtobin(*chipdata) * 60 * 60;
	}

	return (time_t)t;
}
