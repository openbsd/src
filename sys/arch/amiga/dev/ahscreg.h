/*	$NetBSD: ahscreg.h,v 1.2 1994/10/26 02:02:46 cgd Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
 * Copyright (c) 1982, 1990 The Regents of the University of California.
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
 *	@(#)dmareg.h
 */
#ifndef _AMIGA_DEV_AHSCREG_H_
#define _AMIGA_DEV_AHSCREG_H_

/*
 * Hardware layout of the A3000 SDMAC. This also contains the
 * registers for the sbic chip, but in favor of separating DMA and
 * scsi, the scsi-driver doesn't make use of this dependency
 */

#define v_char		volatile char
#define	v_int		volatile int
#define vu_char		volatile u_char
#define vu_short	volatile u_short
#define vu_int		volatile u_int

struct sdmac {
	short		pad0;
	vu_short DAWR;		/* DACK Width Register WO */
	vu_int   WTC;		/* Word Transfer Count Register RW */
	short		pad1;
	vu_short CNTR;		/* Control Register RW */
	vu_int   ACR;		/* Address Count Register RW */
	short		pad2;
	vu_short ST_DMA;	/* Start DMA Transfers RW-Strobe */
	short		pad3;
	vu_short FLUSH;		/* Flush FIFO RW-Strobe */
	short		pad4;
	vu_short CINT;		/* Clear Interrupts RW-Strobe */
	short		pad5;
	vu_short ISTR;		/* Interrupt Status Register RO */
	int		pad6[7];
	short		pad7;
	vu_short SP_DMA;	/* Stop DMA Transfers RW-Strobe */
#ifndef APOLLO
	char		pad8;
	vu_char  SASR;		/* sbic asr */
	char		pad9;
	vu_char  SCMD;		/* sbic data */
#else
	short		pad8;
	char		pad9;
	vu_char  SASR;		/* sbic asr */
	short		pad10;
	char		pad11;
	vu_char  SCMD;		/* sbic data */
#endif
};

/*
 * value to go into DAWR
 */
#define DAWR_AHSC	3	/* according to A3000T service-manual */

/*
 * bits defined for CNTR
 */
#define CNTR_TCEN	(1<<5)	/* Terminal Count Enable */
#define CNTR_PREST	(1<<4)	/* Perp Reset (not implemented :-((( ) */
#define CNTR_PDMD	(1<<3)  /* Perp Device Mode Select (1=SCSI,0=XT/AT) */
#define CNTR_INTEN	(1<<2)	/* Interrupt Enable */
#define CNTR_DDIR	(1<<1)	/* Device Direction. 1==rd host, wr perp */
#define CNTR_IO_DX	(1<<0)	/* IORDY & CSX1 Polarity Select */

/*
 * bits defined for ISTR
 */
#define ISTR_INTX	(1<<8)	/* XT/AT Interrupt pending */
#define ISTR_INT_F	(1<<7)	/* Interrupt Follow */
#define ISTR_INTS	(1<<6)	/* SCSI Peripheral Interrupt */
#define ISTR_E_INT	(1<<5)	/* End-Of-Process Interrupt */
#define ISTR_INT_P	(1<<4)	/* Interrupt Pending */
#define ISTR_UE_INT	(1<<3)	/* Under-Run FIFO Error Interrupt */
#define ISTR_OE_INT	(1<<2)	/* Over-Run FIFO Error Interrupt */
#define ISTR_FF_FLG	(1<<1)	/* FIFO-Full Flag */
#define ISTR_FE_FLG	(1<<0)	/* FIFO-Empty Flag */

#endif /* _AMIGA_DEV_AHSCREG_H_ */
