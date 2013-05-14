/* $OpenBSD: prcmvar.h,v 1.5 2013/05/14 12:01:17 rapha Exp $ */
/*
 * Copyright (c) 2007,2009 Dale Rahn <drahn@openbsd.org>
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

void prcm_setclock(int clock, int speed);
void prcm_enablemodule(int mod);
void prcm_disablemodule(int mod);

#define PRCM_CLK_SPEED_32	0
#define PRCM_CLK_SPEED_SYS	1

enum PRCM_MODULES {
	PRCM_TIMER0,
	PRCM_TIMER1,
	PRCM_TIMER2,
	PRCM_TIMER3,
	PRCM_MMC,
	PRCM_USB,
	PRCM_USBTLL
};

#define PRCM_REG_MAX	6
/* need interface for CM_AUTOIDLE */
