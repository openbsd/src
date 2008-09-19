/*	$OpenBSD: board.c,v 1.1 2008/09/19 20:18:03 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <machine/prom.h>

#include "stand.h"
#include "libsa.h"

#include <dev/busswreg.h>

void
board_setup()
{
	int board;
	u_int8_t cpuspeed, version, btimer, pbt;

	board = mvmeprom_brdid()->model;

	switch (board) {
	case BRD_197:
		/*
		 * The following is similar to what m197_bootstrap()
		 * in mvme88k/m197_machdep.c does.
		 * Please refer to the comments in there for details.
		 */
		cpuspeed = 256 - *(volatile u_int8_t *)(BS_BASE + BS_PADJUST);
		version = *(volatile u_int8_t *)(BS_BASE + BS_CHIPREV);
		btimer = *(volatile u_int8_t *)(BS_BASE + BS_BTIMER);
		pbt = btimer & BS_BTIMER_PBT_MASK;
		btimer = (btimer & ~BS_BTIMER_PBT_MASK);
	
		if (cpuspeed < 50 || version <= 0x01) {
			if (pbt < BS_BTIMER_PBT256)
				pbt = BS_BTIMER_PBT256;
		} else {
			if (pbt < BS_BTIMER_PBT64)
				pbt = BS_BTIMER_PBT64;
		}

		*(volatile u_int8_t *)(BS_BASE + BS_BTIMER) = btimer | pbt;

		break;
	}
}
