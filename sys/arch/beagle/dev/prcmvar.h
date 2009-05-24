/* $OpenBSD: prcmvar.h,v 1.2 2009/05/24 00:36:41 drahn Exp $ */
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
#define PRCM_CLK_SPEED_32	0
#define PRCM_CLK_SPEED_SYS	1

void prcm_enableclock(int bit);

#define		PRCM_CLK_EN_MMC3		(30)
#define		PRCM_CLK_EN_ICR		(29)
#define		PRCM_CLK_EN_AES2 	(28)
#define		PRCM_CLK_EN_SHA12 	(27)
#define		PRCM_CLK_EN_DES2 	(26)
#define		PRCM_CLK_EN_MMC2		(25)
#define		PRCM_CLK_EN_MMC1		(24)
#define		PRCM_CLK_EN_MSPRO	(23)
#define		PRCM_CLK_EN_HDQ		(22)
#define		PRCM_CLK_EN_MCSPI4	(21)
#define		PRCM_CLK_EN_MCSPI3	(20)
#define		PRCM_CLK_EN_MCSPI2	(19)
#define		PRCM_CLK_EN_MCSPI1	(18)
#define		PRCM_CLK_EN_I2C3		(17)
#define		PRCM_CLK_EN_I2C2		(16)
#define		PRCM_CLK_EN_I2C1		(15)
#define		PRCM_CLK_EN_UART2	(14)
#define		PRCM_CLK_EN_UART1	(13)
#define		PRCM_CLK_EN_GPT11	(12)
#define		PRCM_CLK_EN_GPT10	(11)
#define		PRCM_CLK_EN_MCBSP5	(10)
#define		PRCM_CLK_EN_MCBSP1	(9)
#define		PRCM_CLK_EN_MAILBOXES 	(7)
#define		PRCM_CLK_EN_OMAPCTRL 	(6)
#define		PRCM_CLK_EN_HSOTGUSB 	(4)
#define		PRCM_CLK_EN_SDRC 	(1)

#define		CM_CORE_EN_USBTLL	(2+64)
#define		CM_CORE_EN_TS		(1+64)
#define		CM_CORE_EN_CPEFUSE	(0+64)

/* need interface for CM_AUTOIDLE */
