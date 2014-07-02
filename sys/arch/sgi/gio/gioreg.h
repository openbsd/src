/*	$OpenBSD: gioreg.h,v 1.4 2014/07/02 17:44:35 miod Exp $	*/
/*	$NetBSD: gioreg.h,v 1.4 2006/08/31 00:01:10 rumble Exp $	*/

/*
 * Copyright (c) 2003 Ilpo Ruotsalainen
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * <<Id: LICENSE_GC,v 1.1 2001/10/01 23:24:05 cgd Exp>>
 */

/*
 * The GIO Product Identification Word is the first word (1 or 4 bytes)
 * in each GIO device's address space. It is the same format for GIO32,
 * GIO32-bis, and GIO64 devices. The macros below extract the following
 * fields:
 *
 * Bits:
 *    0-6 Product ID Code
 *	7 Product Identification Word size (0: 8 bits, 1: 32 bits)
 *   8-15 Product Revision
 *     16 GIO Interface Size (0: 32, 1: 64; NB: GIO64 devices may be 32)
 *     17 Rom Present (1: present)
 *  18-31 Manufacturer-specific Code
 *
 * The upper three bytes containing the Product Revision, GIO Interface
 * Size, Rom Presence indicator, and Manufacturer-specific Code are only 
 * valid if bit 7 is set in the Product ID Word. If it is not set, all
 * values default to 0.
 *
 * If the Rom Present bit is set, the three words after the Product ID are
 * reserved for three ROM registers:
 *	Board Serial Number Register	(base_address + 0x4)
 *	ROM Index Register		(base_address + 0x8)
 *	ROM Read Register		(base_address + 0xc)
 *
 * The ROM Index Register is initialised by the CPU to 0 and incremented by
 * 4 on each read from the ROM Read Register. The Board Serial Number
 * Register contains a manufacturer-specific serial number.
 */

#define GIO_PRODUCT_32BIT_ID(x)		((x) & 0x80)
#define GIO_PRODUCT_PRODUCTID(x)	((x) & 0x7f)
#define GIO_PRODUCT_REVISION(x)		(((x) >> 8) & 0xff)
#define GIO_PRODUCT_IS_64BIT(x)		(!!((x) & 0x10000))
#define GIO_PRODUCT_HAS_ROM(x)		(!!((x) & 0x20000))
#define GIO_PRODUCT_MANUCODE(x)		((x) >> 18)

#define	GIO_ADDR_GFX			0x1f000000	/* 4MB */
#define	GIO_ADDR_EXP0			0x1f400000	/* 2MB */
#define	GIO_ADDR_EXP1			0x1f600000	/* 4MB */
#define	GIO_ADDR_END			0x1fa00000

#define	IS_GIO_ADDRESS(pa)	((pa) >= GIO_ADDR_GFX && (pa) < GIO_ADDR_END)
