/*	$OpenBSD: machine_reg.h,v 1.2 2009/09/09 11:34:02 marex Exp $	*/
/*	$NetBSD: lubbock_reg.h,v 1.1 2003/06/18 10:51:15 bsh Exp $ */

/*
 * Copyright (c) 2002, 2003  Genetec Corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec Corporation may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _PALM_REG_H
#define _PALM_REG_H

#include <arm/xscale/pxa2x0reg.h>

/*
 * Logical mapping for onboard/integrated peripherals
 */
#define	PALM_IO_AREA_VBASE	0xfd000000
#define PALM_GPIO_VBASE		0xfd000000
#define PALM_CLKMAN_VBASE 	0xfd100000
#define PALM_INTCTL_VBASE 	0xfd200000
#define PALM_VBASE_FREE		0xfd300000
/* FFUART, BTUART and/or STUART are mapped to this area when
   used for console or kgdb port */

#define ioreg_read(a)  (*(volatile unsigned *)(a))
#define ioreg_write(a,v)  (*(volatile unsigned *)(a)=(v))

#define ioreg16_read(a)  (*(volatile uint16_t *)(a))
#define ioreg16_write(a,v)  (*(volatile uint16_t *)(a)=(v))

#define ioreg8_read(a)  (*(volatile uint8_t *)(a))
#define ioreg8_write(a,v)  (*(volatile uint8_t *)(a)=(v))

/* GPIOs */
#define GPIO14_MMC_DETECT		14	/* MMC detect*/
#define GPIO12_TC_MMC_DETECT		12	/* TC MMC detect*/
#define GPIO114_MMC_POWER		114	/* MMC power */
#define GPIO98_PALMZ72_MMC_POWER	98	/* Z72 MMC power */
#define GPIO32_PALMTC_MMC_POWER		32	/* TC MMC power */

#define GPIO13_PALMTX_USB_DETECT	13
#define GPIO15_USB_DETECT		15
#define GPIO95_USB_PULLUP		95

#endif /* _PALM_REG_H */
