/*	$OpenBSD: dc21285mem.h,v 1.1 2004/02/01 05:09:49 drahn Exp $	*/
/*	$NetBSD: dc21285mem.h,v 1.2 2001/06/09 10:44:11 chris Exp $	*/

/*
 * Copyright (c) 1997,1998 Mark Brinicombe.
 * Copyright (c) 1997,1998 Causality Limited
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Physical memory map provided by the DC21285 'Footbridge'
 */

#define	DC21285_SDRAM_BASE		0x00000000
#define DC21285_SDRAM_SIZE		0x10000000	/* 256 MB */

#define	DC21285_SDRAM_A0MR		0x40000000
#define	DC21285_SDRAM_A1MR		0x40004000
#define	DC21285_SDRAM_A2MR		0x40008000
#define	DC21285_SDRAM_A3MR		0x4000C000

#define	DC21285_XBUS_XCS0		0x40010000
#define	DC21285_XBUS_XCS1		0x40011000
#define	DC21285_XBUS_XCS2		0x40012000
#define	DC21285_XBUS_NOCS		0x40013000

#define DC21285_ROM_BASE		0x41000000
#define DC21285_ROM_SIZE		0x01000000	/* 16MB */

#define DC21285_ARMCSR_BASE		0x42000000
#define DC21285_ARMCSR_SIZE		0x00100000	/* 1MB */

#define DC21285_SA_CACHE_FLUSH_BASE	0x50000000
#define DC21285_SA_CACHE_FLUSH_SIZE	0x01000000	/* 16MB */

#define DC21285_OUTBOUND_WRITE_FLUSH	0x78000000

#define DC21285_PCI_IACK_SPECIAL	0x79000000
#define DC21285_PCI_TYPE_1_CONFIG	0x7A000000
#define DC21285_PCI_TYPE_0_CONFIG	0x7B000000
#define DC21285_PCI_IO_BASE		0x7C000000
#define DC21285_PCI_IO_SIZE		0x00010000	/* 64K */
#define DC21285_PCI_MEM_BASE		0x80000000
#define DC21285_PCI_MEM_SIZE		0x80000000	/* 2GB */

/*
 * Standard Virtual memory map used for the DC21285 'Footbridge'
 */
#define DC21285_ARMCSR_VBASE		0xFD000000
#define	DC21285_ARMCSR_VSIZE		0x00100000	/* 1MB */
#define	DC21285_CACHE_FLUSH_VBASE	0xFD100000
#define	DC21285_CACHE_FLUSH_VSIZE	0x00100000	/* 1MB */
#define	DC21285_PCI_IO_VBASE		0xFD200000
#define	DC21285_PCI_IO_VSIZE		0x00100000	/* 1MB */
#define	DC21285_PCI_IACK_VBASE		0xFD300000
#define	DC21285_PCI_IACK_VSIZE		0x00100000	/* 1MB */
#define	DC21285_PCI_ISA_MEM_VBASE	0xFD400000
#define	DC21285_PCI_ISA_MEM_VSIZE	0x00100000	/* 1MB */
#define	DC21285_PCI_TYPE_1_CONFIG_VBASE	0xFE000000
#define	DC21285_PCI_TYPE_1_CONFIG_VSIZE	0x01000000	/* 16MB */
#define	DC21285_PCI_TYPE_0_CONFIG_VBASE	0xFF000000
#define	DC21285_PCI_TYPE_0_CONFIG_VSIZE	0x01000000	/* 16MB */
