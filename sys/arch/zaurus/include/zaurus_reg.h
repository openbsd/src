/*	$OpenBSD: zaurus_reg.h,v 1.3 2005/01/14 18:44:08 drahn Exp $	*/
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


#ifndef _EVBARM_ZAURUS_REG_H
#define _EVBARM_ZAURUS_REG_H

#include <arm/xscale/pxa2x0reg.h>

#define ZAURUS_SRAM_PBASE (PXA2X0_CS2_START+0x02000000)
#define ZAURUS_SRAM_SIZE  0x00100000

#define ZAURUS_91C96_PBASE PXA2X0_CS3_START
#define ZAURUS_91C96_IO	ZAURUS_91C96_PBASE
#define ZAURUS_91C96_ATTRMEM	(ZAURUS_91C96_PBASE+0x02000000)


/* SA-1111 companion chip registers */
#define ZAURUS_SACC_PBASE PXA2X0_CS4_START

/*
 * Logical mapping for onboard/integrated peripherals
 */
#define	ZAURUS_IO_AREA_VBASE	0xfd000000
#define ZAURUS_GPIO_VBASE	0xfd000000
#define ZAURUS_CLKMAN_VBASE 	0xfd100000
#define ZAURUS_INTCTL_VBASE 	0xfd200000
#define ZAURUS_AGPIO_VBASE	0xfd300000
#define ZAURUS_VBASE_FREE	0xfd400000
/* FFUART and/or BTUART are mapped to this area when
   used for console or kgdb port */

#define ioreg_read(a)  (*(volatile unsigned *)(a))
#define ioreg_write(a,v)  (*(volatile unsigned *)(a)=(v))

#define ioreg16_read(a)  (*(volatile uint16_t *)(a))
#define ioreg16_write(a,v)  (*(volatile uint16_t *)(a)=(v))

#define ioreg8_read(a)  (*(volatile uint8_t *)(a))
#define ioreg8_write(a,v)  (*(volatile uint8_t *)(a)=(v))

#endif /* _EVBARM_ZAURUS_REG_H */
