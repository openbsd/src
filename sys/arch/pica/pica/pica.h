/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University,
 * Ralph Campbell and Rick Macklem.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)pica.h	8.1 (Berkeley) 6/10/93
 *      $Id: pica.h,v 1.2 1995/12/16 13:05:20 deraadt Exp $
 */

/*
 * HISTORY
 * Log:	pica.h,v
 * 	Created, from the ALI specs:
 */
/*
 *	File: pica.h
 * 	Author: Per Fogelstrom
 *	Date:	1/95
 *
 */

#ifndef	MIPS_PICA_H
#define	MIPS_PICA_H 1

/*
 * PICA's Physical address space
 */

#define PICA_PHYS_MIN		0x00000000	/* 256 Meg */
#define PICA_PHYS_MAX		0x0fffffff

/*
 * Memory map
 */

#define PICA_PHYS_MEMORY_START	0x00000000
#define PICA_PHYS_MEMORY_END	0x0fffffff	/* 256 Meg in 8 slots */

#define PICA_MEMORY_SIZE_REG	0xe00fffe0	/* Memory size register */
#define	PICA_CONFIG_REG		0xe00ffff0	/* Hardware config reg  */

/*
 * I/O map
 */

#define	PICA_P_LOCAL_IO_BASE	0x80000000	/* I/O Base address */
#define	PICA_V_LOCAL_IO_BASE	0xe0000000
#define	PICA_S_LOCAL_IO_BASE	0x00040000	/* Size */
#define PVLB PICA_V_LOCAL_IO_BASE
#define	PICA_SYS_TL_BASE	(PVLB+0x0018)	/* DMA transl. table base */
#define	PICA_SYS_TL_LIMIT	(PVLB+0x0020)	/* DMA transl. table limit */
#define	PICA_SYS_TL_IVALID	(PVLB+0x0028)	/* DMA transl. cache inval */
#define	PICA_SYS_DMA0_REGS	(PVLB+0x0100)	/* DMA ch0 base address */
#define	PICA_SYS_DMA1_REGS	(PVLB+0x0120)	/* DMA ch0 base address */
#define	PICA_SYS_DMA2_REGS	(PVLB+0x0140)	/* DMA ch0 base address */
#define	PICA_SYS_DMA3_REGS	(PVLB+0x0160)	/* DMA ch0 base address */
#define	PICA_SYS_IT_VALUE	(PVLB+0x0228)	/* Interval timer reload */
#define	PICA_SYS_IT_STAT	(PVLB+0x0230)	/* Interval timer count */
#define	PICA_SYS_EXT_IMASK	(PVLB+0x00e8)	/* External int enable mask */
#define	PICA_SYS_SONIC		(PVLB+0x1000)	/* SONIC base address */
#define	PICA_SYS_SCSI		(PVLB+0x2000)	/* SCSI base address */
#define	PICA_SYS_FLOPPY		(PVLB+0x3000)	/* Floppy base address */
#define	PICA_SYS_CLOCK		(PVLB+0x4000)	/* Clock base address */
#define	PICA_SYS_KBD		(PVLB+0x5000)	/* Keybrd/mouse base address */
#define	PICA_SYS_COM1		(PVLB+0x6000)	/* Com port 1 */
#define	PICA_SYS_COM2		(PVLB+0x7000)	/* Com port 2 */
#define	PICA_SYS_PAR1		(PVLB+0x8000)	/* Parallel port 1 */
#define	PICA_SYS_NVRAM		(PVLB+0x9000)	/* Unprotected NV-ram */
#define	PICA_SYS_PNVRAM		(PVLB+0xa000)	/* Protected NV-ram */
#define	PICA_SYS_NVPROM		(PVLB+0xb000)	/* Read only NV-ram */
#define	PICA_SYS_SOUND		(PVLB+0xc000)	/* Sound port */

#define	PICA_SYS_ISA_AS		(PICA_V_ISA_IO+0x70)

#define	PICA_P_DRAM_CONF	0x800e0000	/* Dram config registers */
#define	PICA_V_DRAM_CONF	0xe00e0000
#define	PICA_S_DRAM_CONF	0x00020000

#define	PICA_P_INT_SOURCE	0xf0000000	/* Interrupt src registers */
#define	PICA_V_INT_SOURCE	PICA_V_LOCAL_IO_BASE+PICA_S_LOCAL_IO_BASE
#define	PICA_S_INT_SOURCE	0x00001000
#define PVIS PICA_V_INT_SOURCE
#define	PICA_SYS_LB_IS		(PVIS+0x0000)	/* Local bus int source */
#define	PICA_SYS_LB_IE		(PVIS+0x0002)	/* Local bus int enables */
#define PICA_SYS_LB_IE_PAR1	0x0001		/* Parallel port enable */
#define	PICA_SYS_LB_IE_FLOPPY	0x0002		/* Floppy ctrl enable */
#define	PICA_SYS_LB_IE_SOUND	0x0004		/* Sound port enable */
#define	PICA_SYS_LB_IE_VIDEO	0x0008		/* Video int enable */
#define	PICA_SYS_LB_IE_SONIC	0x0010		/* Ethernet ctrl enable */
#define	PICA_SYS_LB_IE_SCSI	0x0020		/* Scsi crtl enable */
#define PICA_SYS_LB_IE_KBD	0x0040		/* Keyboard ctrl enable */
#define PICA_SYS_LB_IE_MOUSE	0x0080		/* Mouse ctrl enable */
#define	PICA_SYS_LB_IE_COM1	0x0100		/* Serial port 1 enable */
#define	PICA_SYS_LB_IE_COM2	0x0200		/* Serial port 2 enable */

#define	PICA_P_LOCAL_VIDEO_CTRL	0x60000000	/* Local video control */
#define	PICA_V_LOCAL_VIDEO_CTRL	0xe0200000
#define	PICA_S_LOCAL_VIDEO_CTRL	0x00200000

#define	PICA_P_EXTND_VIDEO_CTRL	0x60200000	/* Extended video control */
#define	PICA_V_EXTND_VIDEO_CTRL	0xe0400000
#define	PICA_S_EXTND_VIDEO_CTRL	0x00200000

#define	PICA_P_LOCAL_VIDEO	0x40000000	/* Local video memory */
#define	PICA_V_LOCAL_VIDEO	0xe0800000
#define	PICA_S_LOCAL_VIDEO	0x00800000

#define	PICA_P_ISA_IO		0x90000000	/* ISA I/O control */
#define	PICA_V_ISA_IO		0xe2000000
#define	PICA_S_ISA_IO		0x01000000

#define	PICA_P_ISA_MEM		0x91000000	/* ISA Memory control */
#define	PICA_V_ISA_MEM		0xe3000000
#define	PICA_S_ISA_MEM		0x01000000

/*
 *  Addresses used by various display drivers.
 */
#define MONO_BASE	(PICA_V_LOCAL_VIDEO_CTRL + 0x3B4)
#define MONO_BUF	(PICA_V_LOCAL_VIDEO + 0xB0000)
#define CGA_BASE	(PICA_V_LOCAL_VIDEO_CTRL + 0x3D4)
#define CGA_BUF		(PICA_V_LOCAL_VIDEO + 0xB8000)

/*
 *  Interrupt vector descriptor for device on pica bus.
 */
struct pica_int_desc {
	int		int_mask;	/* Mask used in PICA_SYS_LB_IE */
	intr_handler_t	int_hand;	/* Interrupt handler */
	void		*param;		/* Parameter to send to handler */
	int		spl_mask;	/* Spl mask for interrupt */
};

int	pica_intrnull __P((void *));
#endif	/* MIPS_PICA_H */
