/*	$OpenBSD: dcareg.h,v 1.8 2010/05/22 13:04:25 deraadt Exp $	*/
/*	$NetBSD: dcareg.h,v 1.6 1996/02/24 00:55:02 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)dcareg.h	8.1 (Berkeley) 6/10/93
 */

#include <hp300/dev/iotypes.h>			/* XXX */

struct dcadevice {
	/* card registers */
	u_char	dca_pad0;
	vu_char	dca_id;				/* 0x01 (read) */
#define		dca_reset	dca_id		/* 0x01 (write) */
	u_char	dca_pad1;
	vu_char	dca_ic;				/* 0x03 */
	u_char	dca_pad2;
	vu_char	dca_ocbrc;			/* 0x05 */
	u_char	dca_pad3;
	vu_char	dca_lcsm;			/* 0x07 */
	u_char	dca_pad4[8];
	/* chip registers */
	u_char	dca_pad5;
	vu_char	dca_data;			/* 0x11 */
	u_char	dca_pad6;
	vu_char	dca_ier;			/* 0x13 */
	u_char	dca_pad7;
	vu_char	dca_iir;			/* 0x15 (read) */
#define		dca_fifo	dca_iir		/* 0x15 (write) */
	u_char	dca_pad8;
	vu_char	dca_cfcr;			/* 0x17 */
	u_char	dca_pad9;
	vu_char	dca_mcr;			/* 0x19 */
	u_char	dca_padA;
	vu_char	dca_lsr;			/* 0x1B */
	u_char	dca_padB;
	vu_char	dca_msr;			/* 0x1D */
};

/* interface reset/id (300 only) */
#define	DCAID0		0x02
#define	DCAID1		0x42
#define DCACON          0x80	/* REMOTE/LOCAL switch */

/* interrupt control (300 only) */
#define	DCAIPL(x)	((((x) >> 4) & 3) + 3)
#define	IC_IR		0x40
#define	IC_IE		0x80

/*
 * 16 bit baud rate divisor (lower byte in dca_data, upper in dca_ier)
 * NB: This constant is for a 7.3728 clock frequency. The 300 clock
 *     frequency is 2.4576, giving a constant of 153600.
 */
#define	DCABRD(x)	(153600 / (x))

/* interrupt enable register */
#define	IER_ERXRDY	0x1	/* Enable receiver interrupt */
#define	IER_ETXRDY	0x2	/* Enable transmitter empty interrupt */
#define	IER_ERLS	0x4	/* Enable line status interrupt */
#define	IER_EMSC	0x8	/* Enable modem status interrupt */

/* interrupt identification register */
#define	IIR_IMASK	0xf
#define	IIR_RXTOUT	0xc
#define	IIR_RLS		0x6	/* Line status change */
#define	IIR_RXRDY	0x4	/* Receiver ready */
#define	IIR_TXRDY	0x2	/* Transmitter ready */
#define	IIR_NOPEND	0x1	/* No pending interrupts */
#define	IIR_MLSC	0x0	/* Modem status */
#define	IIR_FIFO_MASK	0xc0	/* set if FIFOs are enabled */

/* fifo control register */
#define	FIFO_ENABLE	0x01	/* Turn the FIFO on */
#define	FIFO_RCV_RST	0x02	/* Reset RX FIFO */
#define	FIFO_XMT_RST	0x04	/* Reset TX FIFO */
#define	FIFO_DMA_MODE	0x08
#define	FIFO_TRIGGER_1	0x00	/* Trigger RXRDY intr on 1 character */
#define	FIFO_TRIGGER_4	0x40	/* ibid 4 */
#define	FIFO_TRIGGER_8	0x80	/* ibid 8 */
#define	FIFO_TRIGGER_14	0xc0	/* ibid 14 */

/* character format control register */
#define	CFCR_DLAB	0x80
#define	CFCR_SBREAK	0x40
#define	CFCR_PZERO	0x30
#define	CFCR_PONE	0x20
#define	CFCR_PEVEN	0x10
#define	CFCR_PODD	0x00
#define	CFCR_PENAB	0x08
#define	CFCR_STOPB	0x04
#define	CFCR_8BITS	0x03
#define	CFCR_7BITS	0x02
#define	CFCR_6BITS	0x01
#define	CFCR_5BITS	0x00

/* modem control register */
#define	MCR_LOOPBACK	0x10	/* Loop test: echos from TX to RX */
#define	MCR_IEN		0x08	/* Out2: enables UART interrupts */
#define	MCR_DRS		0x04	/* Out1: resets some internal modems */
#define	MCR_RTS		0x02	/* Request To Send */
#define	MCR_DTR		0x01	/* Data Terminal Ready */

/* line status register */
#define	LSR_RCV_FIFO	0x80
#define	LSR_TSRE	0x40	/* Transmitter empty: byte sent */
#define	LSR_TXRDY	0x20	/* Transmitter buffer empty */
#define	LSR_BI		0x10	/* Break detected */
#define	LSR_FE		0x08	/* Framing error: bad stop bit */
#define	LSR_PE		0x04	/* Parity error */
#define	LSR_OE		0x02	/* Overrun, lost incoming byte */
#define	LSR_RXRDY	0x01	/* Byte ready in Receive Buffer */
#define	LSR_RCV_MASK	0x1f	/* Mask for incoming data or error */

/* modem status register */
#define	MSR_DCD		0x80	/* Current Data Carrier Detect */
#define	MSR_RI		0x40	/* Current Ring Indicator */
#define	MSR_DSR		0x20	/* Current Data Set Ready */
#define	MSR_CTS		0x10	/* Current Clear to Send */
#define	MSR_DDCD	0x08	/* DCD has changed state */
#define	MSR_TERI	0x04	/* RI has toggled low to high */
#define	MSR_DDSR	0x02	/* DSR has changed state */
#define	MSR_DCTS	0x01	/* CTS has changed state */
