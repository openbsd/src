/*	$NetBSD: asic.h,v 1.8 1996/05/19 01:42:54 jonathan Exp $	*/

/* 
 * Copyright (c) 1991,1990,1989,1994,1995 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Author: Jonathan Stone
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

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
 *	@(#)asic.h	8.1 (Berkeley) 6/10/93
 */

#ifndef	MIPS_IOASIC_H
#define	MIPS_IOASIC_H 1

/*
 * Slot definitions
 *
 * The IOASIC is memory-mapped into a turbochannel slot as the "system"
 * pseudo-slot pseudo-slot, containing on-the-baseboard devices for
 * turbochannel Decstations and  Alphas.  Regardless of the size
 * of the host turbochannel slots, the IO ASIC provides up to  sixteen
 * fixed-size  256 Kbyte subslots. (or 512 Kbytes on alphas?)
 *
 * Slot 0 is always system ROM, with a Turbochannel ROM ident string.
 * Slot 1 is always the ASIC's own registers.
 *
 * Lance ethernet (and dma registers), SCC serial chip, ASC scsi chip
 * (and DMA registers for all those), and a real-time clock are
 * present on all IO asic machines.  Other slots number vary by machine
 * (pmax or alpha) and model of machine.
 *
 * The Decstation/xx (MAXINE) and all(?) Alphas have an audio/ISDN chip.
 *
 * The MAXINE has only one SCC serial chip, but  has a  DTOP "desktop bus"
 * device. All other  IOASIC machines have two SCCs.
 *
 * The MAXINE also a floppy-disk controller, and a free-running
 * clock with 1-microsecond resolution.
 */

#define	IOASIC_SLOT_0_START		0x000000
#define	IOASIC_SLOT_1_START		0x040000
#define	IOASIC_SLOT_2_START		0x080000
#define	IOASIC_SLOT_3_START		0x0c0000
#define	IOASIC_SLOT_4_START		0x100000
#define	IOASIC_SLOT_5_START		0x140000
#define	IOASIC_SLOT_6_START		0x180000
#define	IOASIC_SLOT_7_START		0x1c0000
#define	IOASIC_SLOT_8_START		0x200000
#define	IOASIC_SLOT_9_START		0x240000
#define	IOASIC_SLOT_10_START		0x280000
#define	IOASIC_SLOT_11_START		0x2c0000
#define	IOASIC_SLOT_12_START		0x300000
#define	IOASIC_SLOT_13_START		0x340000
#define	IOASIC_SLOT_14_START		0x380000
#define	IOASIC_SLOT_15_START		0x3c0000
#define	IOASIC_SLOTS_END		0x3fffff
	
/*
 *  IOASIC register offsets (slot 1)
 */

#define	IOASIC_SCSI_DMAPTR		IOASIC_SLOT_1_START+0x000
#define	IOASIC_SCSI_NEXTPTR		IOASIC_SLOT_1_START+0x010
#define	IOASIC_LANCE_DMAPTR		IOASIC_SLOT_1_START+0x020
#define	IOASIC_SCC_T1_DMAPTR		IOASIC_SLOT_1_START+0x030
#define	IOASIC_SCC_R1_DMAPTR		IOASIC_SLOT_1_START+0x040
#define	IOASIC_SCC_T2_DMAPTR		IOASIC_SLOT_1_START+0x050
#define	IOASIC_SCC_R2_DMAPTR		IOASIC_SLOT_1_START+0x060
#define	IOASIC_FLOPPY_DMAPTR		IOASIC_SLOT_1_START+0x070
#define	IOASIC_ISDN_X_DMAPTR		IOASIC_SLOT_1_START+0x080
#define	IOASIC_ISDN_X_NEXTPTR		IOASIC_SLOT_1_START+0x090
#define	IOASIC_ISDN_R_DMAPTR		IOASIC_SLOT_1_START+0x0a0
#define	IOASIC_ISDN_R_NEXTPTR		IOASIC_SLOT_1_START+0x0b0
#define	IOASIC_BUFF0			IOASIC_SLOT_1_START+0x0c0
#define	IOASIC_BUFF1			IOASIC_SLOT_1_START+0x0d0
#define	IOASIC_BUFF2			IOASIC_SLOT_1_START+0x0e0
#define	IOASIC_BUFF3			IOASIC_SLOT_1_START+0x0f0
#define	IOASIC_CSR			IOASIC_SLOT_1_START+0x100
#define	IOASIC_INTR			IOASIC_SLOT_1_START+0x110
#define	IOASIC_IMSK			IOASIC_SLOT_1_START+0x120
#define	IOASIC_CURADDR			IOASIC_SLOT_1_START+0x130
#define	IOASIC_ISDN_X_DATA		IOASIC_SLOT_1_START+0x140
#define	IOASIC_ISDN_R_DATA		IOASIC_SLOT_1_START+0x150
#define	IOASIC_LANCE_DECODE		IOASIC_SLOT_1_START+0x160
#define	IOASIC_SCSI_DECODE		IOASIC_SLOT_1_START+0x170
#define	IOASIC_SCC0_DECODE		IOASIC_SLOT_1_START+0x180
#define	IOASIC_SCC1_DECODE		IOASIC_SLOT_1_START+0x190
#define	IOASIC_FLOPPY_DECODE		IOASIC_SLOT_1_START+0x1a0
#define	IOASIC_SCSI_SCR			IOASIC_SLOT_1_START+0x1b0
#define	IOASIC_SCSI_SDR0		IOASIC_SLOT_1_START+0x1c0
#define	IOASIC_SCSI_SDR1		IOASIC_SLOT_1_START+0x1d0
#define	IOASIC_CTR			IOASIC_SLOT_1_START+0x1e0 /*5k/240,alpha only*/

/*
 * System Status and Control register (SSR) bit definitions.
 * (The SSR is the IO ASIC register named ASIC_CSR above).
 */

#define IOASIC_CSR_DMAEN_T1		0x80000000	/* rw */
#define IOASIC_CSR_DMAEN_R1		0x40000000	/* rw */
#define IOASIC_CSR_DMAEN_T2		0x20000000	/* rw */
#define IOASIC_CSR_DMAEN_R2		0x10000000	/* rw */
#define	IOASIC_CSR_FASTMODE		0x08000000	/* rw */ /*not on pmaxes*/
#define IOASIC_CSR_xxx			0x07800000	/* reserved */
#define IOASIC_CSR_DS_xxx		0x0f800000	/* reserved */
#define IOASIC_CSR_FLOPPY_DIR		0x00400000	/* rw */
#define IOASIC_CSR_DMAEN_FLOPPY		0x00200000	/* rw */
#define IOASIC_CSR_DMAEN_ISDN_T		0x00100000	/* rw */
#define IOASIC_CSR_DMAEN_ISDN_R		0x00080000	/* rw */
#define IOASIC_CSR_SCSI_DIR		0x00040000	/* rw */
#define IOASIC_CSR_DMAEN_SCSI		0x00020000	/* rw */
#define IOASIC_CSR_DMAEN_LANCE		0x00010000	/* rw */

/*
 * The low-order 16 bits of SSR are general-purpose bits
 * with model-dependent meaning.
 * The following  are common on all three IOASIC Decstations,
 * (except perhaps TXDIS_1 and TXDIS_2 on xine?).
 * The enable bits appear to be valid on Alphas, also.
 * XXX CDG -- reorganize to separate out bitfields with
 * common meaninds on Alpha, pmax?
 */
#define IOASIC_CSR_DIAGDN		0x00008000	/* rw */	/* (all) */
#define IOASIC_CSR_TXDIS_2	0x00004000	/* rw */ 	/* kmin,kn03 */
#define IOASIC_CSR_TXDIS_1	0x00002000	/* rw */	/* kmin,kn03 */
#define IOASIC_CSR_SCC_ENABLE	0x00000800	/* rw */	/* (all) */
#define IOASIC_CSR_RTC_ENABLE	0x00000400	/* rw */	/* (all) */
#define IOASIC_CSR_SCSI_ENABLE	0x00000200	/* rw */	/* (all) */
#define IOASIC_CSR_LANCE_ENABLE	0x00000100	/* rw */	/* (all) */


/* kn03-specific SRR bit definitions: common bitfields above, plus: */
#define KN03_CSR_LEDS		0x000000ff	/* rw */	/* kn03 */


/* kmin-specific SSR bit definitions: common bitfields above, plus: */
#define KMIN_CSR_LEDS		0x000000ff	/* rw */


/* xine-specific SSR bit definitions: common bitfields above, plus: */
#define XINE_CSR_ISDN_ENABLE	0x00001000	/* rw */
#define XINE_CSR_FLOPPY_ENABLE	0x00000080	/* rw */
#define XINE_CSR_VDAC_ENABLE	0x00000040	/* rw */
#define XINE_CSR_DTOP_ENABLE	0x00000020	/* rw */
#define XINE_CSR_LED		0x00000001	/* rw */



/*
 * System Interrupt Register (and interrupt mask register).
 * The defines above call the SIR IOASIC_INTR,  and the SIRM is called
 * IOASIC_IMSK.
 */
#define	IOASIC_INTR_T1_PAGE_END		0x80000000	/* rz */
#define	IOASIC_INTR_T1_READ_E		0x40000000	/* rz */
#define	IOASIC_INTR_R1_HALF_PAGE	0x20000000	/* rz */
#define	IOASIC_INTR_R1_DMA_OVRUN	0x10000000	/* rz */
#define	IOASIC_INTR_T2_PAGE_END		0x08000000	/* rz */
#define	IOASIC_INTR_T2_READ_E		0x04000000	/* rz */
#define	IOASIC_INTR_R2_HALF_PAGE	0x02000000	/* rz */
#define	IOASIC_INTR_R2_DMA_OVRUN	0x01000000	/* rz */
#define	IOASIC_INTR_FLOPPY_DMA_E	0x00800000	/* rz */
#define	IOASIC_INTR_ISDN_PTR_LOAD	0x00400000	/* rz */
#define	IOASIC_INTR_ISDN_OVRUN		0x00200000	/* rz */
#define	IOASIC_INTR_ISDN_READ_E		0x00100000	/* rz */
#define	IOASIC_INTR_SCSI_PTR_LOAD	0x00080000	/* rz */
#define	IOASIC_INTR_SCSI_OVRUN		0x00040000	/* rz */
#define	IOASIC_INTR_SCSI_READ_E		0x00020000	/* rz */
#define	IOASIC_INTR_LANCE_READ_E	0x00010000	/* rz */

/*
 * SIR and SIRM low-order bits.

 * The low-order 16 bits of SIR and SIRM are general-purpose bits
 * with model-dependent meaning.
 * The following four bits  of the SIRM have the same meaning on
 * all three IOASIC Decstations and apparently on Alphas too.
 * 
 * the MAXINE (decstation 5000/xx) is weird; see below.
 */

#define	IOASIC_INTR_NVR_JUMPER		0x00004000	/* ro */
#define	IOASIC_INTR_NRMOD_JUMPER	0x00000400	/* ro */
#define	IOASIC_INTR_SCSI		0x00000200	/* ro */
#define	IOASIC_INTR_LANCE		0x00000100	/* ro */

/* The following are valid for both kmin and kn03. */

#define	KMIN_INTR_SCC_1			0x00000080	/* ro */ /*kmin,kn03*/
#define	KMIN_INTR_SCC_0			0x00000040	/* ro */

#define	KMIN_INTR_CLOCK			0x00000020	/* ro */
#define	KMIN_INTR_PSWARN		0x00000010	/* ro */
#define	KMIN_INTR_SCSI_FIFO		0x00000004	/* ro */
#define	KMIN_INTR_PBNC			0x00000002	/* ro */
#define	KMIN_INTR_PBNO			0x00000001	/* ro */
#define	KMIN_INTR_ASIC			0xff0f0004

/* kmin-specific SIR/SIRM definitions */
#define	KMIN_INTR_TIMEOUT		0x00001000	/* ro */
#define	KMIN_IM0			0xff0f13f0	/* all good ones enabled */


/* kn03-specific SIR/SIRM definitions */
#define	KN03_INTR_TC_2			0x00002000	/* ro */
#define	KN03_INTR_TC_1			0x00001000	/* ro */
#define	KN03_INTR_TC_0			0x00000800	/* ro */
#define	KN03_IM0			0xff0f3bf0	/* all good ones enabled */

/*
 * SIR/SIRM  low-order bit definitions for the MAXINE.
 * The MAXINE has so many onboard devices that  it
 * has very little in common with the kmin and kn03 --
 * just the two jumpers and the SCSI and LANCE interrupt bits.
 * (is the clock-interrupt-enable bit _really_ different)?
 */

#define	XINE_INTR_xxxx			0x00002808	/* ro */
#define	XINE_INTR_FLOPPY		0x00008000	/* ro */
/*#define	XINE_INTR_NVR_JUMPER	0x00004000 */	/* ro */
#define	XINE_INTR_POWERUP		0x00002000	/* ro */
#define	XINE_INTR_TC_0			0x00001000	/* ro */
#define	XINE_INTR_ISDN			0x00000800	/* ro */
/*#define	XINE_INTR_NRMOD_JUMPER	0x00000400 */	/* ro */
/*#define	XINE_INTR_SCSI		0x00000200 */	/* ro */
/*#define	XINE_INTR_LANCE		0x00000100 */	/* ro */
#define	XINE_INTR_FLOPPY_HDS		0x00000080	/* ro */
#define	XINE_INTR_SCC_0			0x00000040	/* ro */
#define	XINE_INTR_TC_1			0x00000020	/* ro */
#define	XINE_INTR_FLOPPY_XDS		0x00000010	/* ro */
#define	XINE_INTR_VINT			0x00000008	/* ro */
#define	XINE_INTR_N_VINT		0x00000004	/* ro */
#define	XINE_INTR_DTOP_TX		0x00000002	/* ro */
#define	XINE_INTR_DTOP_RX		0x00000001	/* ro */
#define	XINE_INTR_DTOP			0x00000003
#define	XINE_INTR_ASIC			0xffff0000
#define	XINE_IM0			0xffff9b6b	/* all good ones enabled */




/* DMA pointer registers (SCSI, Comm, ...) */

#define	IOASIC_DMAPTR_MASK		0xffffffe0
#define	IOASIC_DMAPTR_SHIFT		5
#define	IOASIC_DMAPTR_SET(reg,val) \
	(reg) = (((val)<<IOASIC_DMAPTR_SHIFT)&IOASIC_DMAPTR_MASK)
#define	IOASIC_DMAPTR_GET(reg,val) \
	(val) = (((reg)&IOASIC_DMAPTR_MASK)>>IOASIC_DMAPTR_SHIFT)
#define	IOASIC_DMA_ADDR(p)		(((unsigned)p) << (5-2))

/* For the LANCE DMA pointer register initialization the above suffices */

/* More SCSI DMA registers */

#define	IOASIC_SCR_STATUS		0x00000004
#define	IOASIC_SCR_WORD			0x00000003

/* Various Decode registers */

#define	IOASIC_DECODE_HW_ADDRESS	0x000003f0
#define	IOASIC_DECODE_CHIP_SELECT	0x0000000f

/*
 * The IOASIC is mapped at different addresses on each model, so we
 * define register addresses as base  plus offset.
 */
#define	IOASIC_REG_SCSI_DMAPTR(base)	((base) + IOASIC_SCSI_DMAPTR)
#define	IOASIC_REG_SCSI_DMANPTR(base)	((base) + IOASIC_SCSI_NEXTPTR)
#define	IOASIC_REG_LANCE_DMAPTR(base)	((base) + IOASIC_LANCE_DMAPTR)
#define	IOASIC_REG_SCC_T1_DMAPTR(base)	((base) + IOASIC_SCC_T1_DMAPTR)
#define	IOASIC_REG_SCC_R1_DMAPTR(base)	((base) + IOASIC_SCC_R1_DMAPTR)
#define	IOASIC_REG_SCC_T2_DMAPTR(base)	((base) + IOASIC_SCC_T2_DMAPTR)
#define	IOASIC_REG_SCC_R2_DMAPTR(base)	((base) + IOASIC_SCC_R2_DMAPTR)
#define	IOASIC_REG_FLOPPY_DMAPTR(base)	((base) + IOASIC_FLOPPY_DMAPTR)
#define	IOASIC_REG_ISDN_X_DMAPTR(base)	((base) + IOASIC_ISDN_X_DMAPTR)
#define	IOASIC_REG_ISDN_X_NEXTPTR(base)	((base) + IOASIC_ISDN_X_NEXTPTR)
#define	IOASIC_REG_ISDN_R_DMAPTR(base)	((base) + IOASIC_ISDN_R_DMAPTR)
#define	IOASIC_REG_ISDN_R_NEXTPTR(base)	((base) + IOASIC_ISDN_R_NEXTPTR)
#define	IOASIC_REG_BUFF0(base)		((base) + IOASIC_BUFF0)
#define	IOASIC_REG_BUFF1(base)		((base) + IOASIC_BUFF1)
#define	IOASIC_REG_BUFF2(base)		((base) + IOASIC_BUFF2)
#define	IOASIC_REG_BUFF3(base)		((base) + IOASIC_BUFF3)
#define	IOASIC_REG_CSR(base)		((base) + IOASIC_CSR)
#define	IOASIC_REG_INTR(base)		((base) + IOASIC_INTR)
#define	IOASIC_REG_IMSK(base)		((base) + IOASIC_IMSK)
#define	IOASIC_REG_CURADDR(base)	((base) + IOASIC_CURADDR)
#define	IOASIC_REG_ISDN_X_DATA(base)	((base) + IOASIC_ISDN_X_DATA)
#define	IOASIC_REG_ISDN_R_DATA(base)	((base) + IOASIC_ISDN_R_DATA)
#define	IOASIC_REG_LANCE_DECODE(base)	((base) + IOASIC_LANCE_DECODE)
#define	IOASIC_REG_SCSI_DECODE(base)	((base) + IOASIC_SCSI_DECODE)
#define	IOASIC_REG_SCC0_DECODE(base)	((base) + IOASIC_SCC0_DECODE)
#define	IOASIC_REG_SCC1_DECODE(base)	((base) + IOASIC_SCC1_DECODE)
#define	IOASIC_REG_FLOPPY_DECODE(base)	((base) + IOASIC_FLOPPY_DECODE)
#define	IOASIC_REG_SCSI_SCR(base)	((base) + IOASIC_SCSI_SCR)
#define	IOASIC_REG_SCSI_SDR0(base)	((base) + IOASIC_SCSI_SDR0)
#define	IOASIC_REG_SCSI_SDR1(base)	((base) + IOASIC_SCSI_SDR1)
#define	IOASIC_REG_CTR(base)		((base) + IOASIC_CTR)

/*
 * And slot assignments.
 */
#define	IOASIC_SYS_ETHER_ADDRESS(base)	((base) + IOASIC_SLOT_2_START)
#define	IOASIC_SYS_LANCE(base)		((base) + IOASIC_SLOT_3_START)

#endif	/* MIPS_IOASIC_H */
