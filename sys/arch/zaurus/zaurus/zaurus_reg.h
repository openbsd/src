/*	$OpenBSD: zaurus_reg.h,v 1.2 2005/01/02 19:43:07 drahn Exp $	*/
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


#ifndef _EVBARM_LUBBOCK_REG_H
#define _EVBARM_LUBBOCK_REG_H

#include <arm/xscale/pxa2x0reg.h>

/* lubbock on-board IOs */
#define LUBBOCK_OBIO_PBASE PXA2X0_CS2_START /* Physical address */
#define LUBBOCK_OBIO_SIZE  0x00000100

#define LUBBOCK_SRAM_PBASE (PXA2X0_CS2_START+0x02000000)
#define LUBBOCK_SRAM_SIZE  0x00100000

#define LUBBOCK_91C96_PBASE PXA2X0_CS3_START
#define LUBBOCK_91C96_IO	LUBBOCK_91C96_PBASE
#define LUBBOCK_91C96_ATTRMEM	(LUBBOCK_91C96_PBASE+0x02000000)


/* SA-1111 companion chip registers */
#define LUBBOCK_SACC_PBASE PXA2X0_CS4_START

/*
 * Logical mapping for onboard/integrated peripherals
 */
#define	LUBBOCK_IO_AREA_VBASE	0xfd000000
#define LUBBOCK_OBIO_VBASE	0xfd000000
#define LUBBOCK_GPIO_VBASE	0xfd100000
#define LUBBOCK_CLKMAN_VBASE 	0xfd200000
#define LUBBOCK_INTCTL_VBASE 	0xfd300000
#define LUBBOCK_AGPIO_VBASE	0xfd400000
#define LUBBOCK_VBASE_FREE	0xfd500000
/* FFUART and/or BTUART are mapped to this area when
   used for console or kgdb port */

/*
 * Onboard register address
 * (offset from LUBBOCK_OBIO_PBASE)
 */
#define LUBBOCK_SYSTEMID	0x0000
#define LUBBOCK_HEXLED  	0x0010
#define LUBBOCK_LEDCTL		0x0040
#define LUBBOCK_CONFIGSW	0x0050
#define LUBBOCK_USERSW		0x0060
#define LUBBOCK_MISCWR		0x0080
#define  MISCWR_S1PWR	 	(3U<<14)
#define  MISCWR_LCDDISP		(1U<<8)
#define  MISCWR_IRDAMODE	(1U<<4)	/* 1=FIR, 0=SIR */
#define  MISCWR_GREENLED	(1U<<3)
#define  MISCWR_ENETEN16	(1U<<2)
#define  MISCWR_PCRESET		(1U<<1)	/* Processor card reset */
#define  MSICWR_SYSRESET	(1U<<0)
#define LUBBOCK_MISCRD		0x0090
#define LUBBOCK_INTRMASK	0x00c0
#define LUBBOCK_INTRCTL		0x00d0

#define ioreg_read(a)  (*(volatile unsigned *)(a))
#define ioreg_write(a,v)  (*(volatile unsigned *)(a)=(v))

#define ioreg16_read(a)  (*(volatile uint16_t *)(a))
#define ioreg16_write(a,v)  (*(volatile uint16_t *)(a)=(v))

#define ioreg8_read(a)  (*(volatile uint8_t *)(a))
#define ioreg8_write(a,v)  (*(volatile uint8_t *)(a)=(v))

#endif /* _EVBARM_LUBBOCK_REG_H */
