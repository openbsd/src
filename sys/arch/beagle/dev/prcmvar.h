/* $OpenBSD: prcmvar.h,v 1.1 2009/05/08 03:13:26 drahn Exp $ */
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
#define PRCM_CLK_SPEED_EXT	2

void prcm_enableclock(int bit);

#define  	PRCM_CLK_EN_DSS1	0
#define  	PRCM_CLK_EN_DSS2	1
#define  	PRCM_CLK_EN_TV		2
#define  	PRCM_CLK_EN_VLYNQ	3
#define  	PRCM_CLK_EN_GP2		4
#define  	PRCM_CLK_EN_GP		5
#define  	PRCM_CLK_EN_GP4		6
#define  	PRCM_CLK_EN_GP5		7
#define  	PRCM_CLK_EN_GP6		8
#define  	PRCM_CLK_EN_GP7		9
#define  	PRCM_CLK_EN_GP8		10
#define  	PRCM_CLK_EN_GP9		11
#define  	PRCM_CLK_EN_GP10	12
#define  	PRCM_CLK_EN_GP11	13
#define  	PRCM_CLK_EN_GP12	14
#define  	PRCM_CLK_EN_MCBSP1	15
#define  	PRCM_CLK_EN_MCBSP2	16
#define  	PRCM_CLK_EN_MCSPI1	17
#define  	PRCM_CLK_EN_MCSPI2	18
#define  	PRCM_CLK_EN_I2C1	19
#define  	PRCM_CLK_EN_I2C2	20
#define  	PRCM_CLK_EN_UART1	21
#define  	PRCM_CLK_EN_UART2	22
#define  	PRCM_CLK_EN_HDQ		23
#define  	PRCM_CLK_EN_EAC		24
#define  	PRCM_CLK_EN_FAC		25
#define  	PRCM_CLK_EN_MMC		26
#define  	PRCM_CLK_EN_MSPR0	27
#define  	PRCM_CLK_EN_WDT3	28
#define  	PRCM_CLK_EN_WDT4	29
#define  	PRCM_CLK_EN_MAILBOX	30
#define  	PRCM_CLK_EN_CAM		31
#define  	PRCM_CLK_EN_USB		32
#define  	PRCM_CLK_EN_SSI		33
#define  	PRCM_CLK_EN_UART3	34

/* need interface for CM_AUTOIDLE */
