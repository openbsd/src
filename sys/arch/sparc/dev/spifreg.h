/*	$OpenBSD: spifreg.h,v 1.1 1999/02/01 00:30:42 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define	SERIAL_INTR	0
#define	PARALLEL_INTR	1

struct ppcregs {
	volatile u_int8_t	in_pdata;	/* input data reg */
	volatile u_int8_t	in_pstat;	/* input status reg */
	volatile u_int8_t	in_pctrl;	/* input control reg */
	volatile u_int8_t	in_pweird;	/* input weird reg */
	volatile u_int8_t	out_pdata;	/* output data reg */
	volatile u_int8_t	out_pstat;	/* output status reg */
	volatile u_int8_t	out_pctrl;	/* output control reg */
	volatile u_int8_t	out_pweird;	/* output weird reg */
	volatile u_int8_t	_unused[500];	/* unused space */
	volatile u_int8_t	iack_pdata;	/* intr-ack data reg */
	volatile u_int8_t	iack_pstat;	/* intr-ack status reg */
	volatile u_int8_t	iack_pctrl;	/* intr-ack control reg */
	volatile u_int8_t	iack_pweird;	/* intr-ack weird reg */
};

struct stcregs {
	volatile u_int8_t	_unused0[1];	/* 0x00 unused */
	volatile u_int8_t	ccr;		/* channel command reg */
	volatile u_int8_t	srer;		/* service req enable reg */
	volatile u_int8_t	cor1;		/* channel option reg 1 */
	volatile u_int8_t	cor2;		/* channel option reg 2 */
	volatile u_int8_t	cor3;		/* channel option reg 3 */
	volatile u_int8_t	ccsr;		/* channel cntrl status reg */
	volatile u_int8_t	rdcr;		/* rx data count reg */
	volatile u_int8_t	_unused1[1];	/* 0x08 unused */
	volatile u_int8_t	schr1;		/* special char reg 1 */
	volatile u_int8_t	schr2;		/* special char reg 2 */
	volatile u_int8_t	schr3;		/* special char reg 3 */
	volatile u_int8_t	schr4;		/* special char reg 4 */
	volatile u_int8_t	_unused2[3];	/* 0x0d - 0x0f unused */

	volatile u_int8_t	mcor1;		/* modem change option reg 1 */
	volatile u_int8_t	mcor2;		/* modem change option reg 2 */
	volatile u_int8_t	mcr;		/* modem change reg */
	volatile u_int8_t	_unused3[5];	/* 0x13 - 0x17 unused */
	volatile u_int8_t	rtpr;		/* rx timeout period reg */
	volatile u_int8_t	_unused4[7];

	volatile u_int8_t	_unused5[8];	/* 0x19 - 0x27 unused */
	volatile u_int8_t	msvr;		/* modem signal value reg */
	volatile u_int8_t	msvrts;		/* modem sig value rts reg */
	volatile u_int8_t	msvdtr;		/* modem sig value dtr reg */
	volatile u_int8_t	_unused6[5];	/* 0x2b - 0x2f unused */

	volatile u_int8_t	_unused7[1];	/* 0x30 unused */
	volatile u_int8_t	rbprh;		/* rx bit rate period reg hi */
	volatile u_int8_t	rbprl;		/* rx bit rate period reg lo */
	volatile u_int8_t	rbr;		/* rx bit reg */
	volatile u_int8_t	_unused8[5];	/* 0x34 - 0x38 unused */
	volatile u_int8_t	tbprh;		/* tx bit rate period reg hi */
	volatile u_int8_t	tbprl;		/* tx bit rate period reg lo */
	volatile u_int8_t	_unused9[5];	/* 0x34 - 0x38 unused */

	volatile u_int8_t	gsvr;		/* global service vector reg */
	volatile u_int8_t	gscr1;		/* global service chan reg 1 */
	volatile u_int8_t	gscr2;		/* global service chan reg 2 */
	volatile u_int8_t	gscr3;		/* global service chan reg 3 */
	volatile u_int8_t	_unused10[12];	/* 0x44 - 0x4f unused */

	volatile u_int8_t	_unused11[16];	/* 0x50 - 0x5f unused */

	volatile u_int8_t	_unused12[1];	/* 0x60 unused */
	volatile u_int8_t	msmr;		/* modem service match reg */
	volatile u_int8_t	tsmr;		/* tx service match reg */
	volatile u_int8_t	rsmr;		/* rx service match reg */
	volatile u_int8_t	car;		/* channel access reg */
	volatile u_int8_t	srsr;		/* service request stat reg */
	volatile u_int8_t	srcr;		/* service request conf reg */
	volatile u_int8_t	_unused13[4];	/* 0x67 - 0x6a unused */
	volatile u_int8_t	gfrcr;		/* global firmwr rev code reg */
	volatile u_int8_t	_unused14[4];	/* 0x6c - 0x6f unused */

	volatile u_int8_t	pprh;		/* prescalar period reg hi */
	volatile u_int8_t	pprl;		/* prescalar period reg lo */
	volatile u_int8_t	_unused15[3];	/* 0x72 - 0x74 unused */
	volatile u_int8_t	mrar;		/* modem request ack reg */
	volatile u_int8_t	trar;		/* tx request ack reg */
	volatile u_int8_t	rrar;		/* rx request ack reg */
	volatile u_int8_t	rdr;		/* rx data reg */
	volatile u_int8_t	_unused16[1];	/* 0x79 unused */
	volatile u_int8_t	rcsr;		/* rx char status reg */
	volatile u_int8_t	tdr;		/* tx data reg */
	volatile u_int8_t	_unused17[3];	/* 0x7c - 0x7e unused */
	volatile u_int8_t	eosrr;		/* end of service req reg */
};

/*
 * The register for the SUNW,spif looks something like:
 *    Offset:		Function:
 *	0000 - 03ff	unused
 *	0400 - 0408	dtr latches (one per port)
 *	0409 - 07ff	unused
 *	0800 - 087f	CD180 registers (normal mapping)
 *	0880 - 0bff	unused
 *	0c00 - 0c7f	CD180 registers (*iack mapping)
 *	0c80 - 0dff	unused
 *	0e00 - 1fff	PPC registers
 */
struct spifregs {
	volatile u_int8_t	_unused1[1024];	/* 0x000-0x3ff unused */
	volatile u_int8_t	dtrlatch[8];	/* per port dtr latch */
	volatile u_int8_t	_unused2[1016];	/* 0x409-0x7ff unused */
	struct stcregs		stc;		/* regular cd-180 regs */
	volatile u_int8_t	_unused3[896];	/* 0x880-0xbff unused */
	struct stcregs		istc;		/* *iack cd-180 regs */
	volatile u_int8_t	_unused4[384];	/* 0xc80-0xdff unused */
	struct ppcregs		ppc;		/* parallel port regs */
};

/*  The mapping of minor device number -> card and port is done as
 * follows by default:
 *
 *  +---+---+---+---+---+---+---+---+
 *  | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
 *  +---+---+---+---+---+---+---+---+
 *    |   |   |   |   |   |   |   |
 *    |   |   |   |   |   +---+---+---> port number
 *    |   |   |   |   |
 *    |   |   |   |   +---------------> unused
 *    |   |   |   |
 *    |   |   |   +-------------------> dialout (on tty ports)
 *    |   |   |
 *    |   |   +-----------------------> unused
 *    |   |
 *    +---+---------------------------> card number
 *
 */

#define	CD180_SRCR_PKGTYP	0x80	/* chip package type */
#define	CD180_SRCR_REGACKEN	0x40
#define	CD180_SRCR_DAISYEN	0x20
#define	CD180_SRCR_GLOBPRI	0x10
#define	CD180_SRCR_UNFAIR	0x08
#define	CD180_SRCR_AUTOPRI	0x04
#define	CD180_SRCR_reserved	0x02
#define	CD180_SRCR_PRISEL	0x01

#define	CD180_CCR_RESET		0x80	/* chip/channel reset */
#define CD180_CCR_RESETALL	0x01	/* global reset */
#define	CD180_CCR_RESETCHAN	0x00	/* current channel reset */

#define	CD180_CCR_CORCHG	0x40	/* channel option reg has changed */
#define	CD180_CCR_CORCHG1	0x02	/* cor1 has changed */
#define	CD180_CCR_CORCHG2	0x04	/* cor2 has changed */
#define	CD180_CCR_CORCHG3	0x08	/* cor3 has changed */

#define	CD180_CCR_SENDSPCHG	0x20
#define	CD180_CCR_SSPC0		0x01
#define	CD180_CCR_SSPC1		0x02
#define	CD180_CCR_SSPC2		0x04

#define	CD180_CCR_CHANCTL	0x10	/* channel control command */
#define	CD180_CCR_CHAN_TXEN	0x08	/* enable channel tx */
#define	CD180_CCR_CHAN_TXDIS	0x04	/* disable channel tx */
#define	CD180_CCR_CHAN_RXEN	0x02	/* enable channel rx */
#define	CD180_CCR_CHAN_RXDIS	0x01	/* disable channel rx */

#define	CD180_COR1_EVENPAR		0x00	/* even parity */
#define	CD180_COR1_ODDPAR		0x80	/* odd parity */
#define	CD180_COR1_PARMODE_NO		0x00	/* no parity */
#define	CD180_COR1_PARMODE_FORCE	0x20	/* force (odd=1, even=0) */
#define CD180_COR1_PARMODE_NORMAL	0x40	/* normal parity mode */
#define	CD180_COR1_PARMODE_NA		0x60	/* notused */
#define	CD180_COR1_IGNPAR		0x10	/* ignore parity */
#define	CD180_COR1_STOP1		0x00	/* 1 stop bit */
#define	CD180_COR1_STOP15		0x04	/* 1.5 stop bits */
#define	CD180_COR1_STOP2		0x08	/* 2 stop bits */
#define	CD180_COR1_STOP25		0x0c	/* 2.5 stop bits */
#define	CD180_COR1_CS5			0x00	/* 5 bit characters */
#define	CD180_COR1_CS6			0x01	/* 6 bit characters */
#define	CD180_COR1_CS7			0x02	/* 7 bit characters */
#define	CD180_COR1_CS8			0x03	/* 8 bit characters */

#define	CD180_COR2_IXM			0x80	/* implied xon mode */
#define	CD180_COR2_TXIBE		0x40	/* tx in-band flow control */
#define	CD180_COR2_ETC			0x20	/* embedded tx command enbl */
#define	CD180_COR2_LLM			0x10	/* local loopback mode */
#define	CD180_COR2_RLM			0x08	/* remote loopback mode */
#define	CD180_COR2_RTSAO		0x04	/* RTS automatic output enbl */
#define	CD180_COR2_CTSAE		0x02	/* CTS automatic enable */
#define	CD180_COR2_DSRAE		0x01	/* DSR automatic enable */

#define	CD180_MCOR1_DSRZD		0x80	/* catch 0->1 DSR changes */
#define	CD180_MCOR1_CDZD		0x40	/* catch 0->1 CD changes */
#define	CD180_MCOR1_CTSZD		0x40	/* catch 0->1 CTS changes */
#define	CD180_MCOR1_DTRTHRESH		0x0f	/* DTR threshold mask */

#define	CD180_MCOR2_DSROD		0x80	/* catch 1->0 DSR changes */
#define	CD180_MCOR2_CDOD		0x40	/* catch 1->0 CD changes */
#define	CD180_MCOR2_CTSOD		0x20	/* catch 1->0 CTS changes */

#define	CD180_SRER_DSR			0x80	/* DSR service request */
#define	CD180_SRER_CD			0x40	/* CD service request */
#define	CD180_SRER_CTS			0x20	/* CTS service request */
#define	CD180_SRER_RXD			0x10	/* RXD service request */
#define	CD180_SRER_RXSCD		0x08	/* RX special char request */
#define	CD180_SRER_TXD			0x04	/* TX ready service request */
#define	CD180_SRER_TXE			0x02	/* TX empty service request */
#define	CD180_SRER_NNDT			0x01	/* No new data timeout req */

#define	CD180_MSVR_DSR			0x80	/* DSR input state */
#define	CD180_MSVR_CD			0x40	/* CD input state */
#define	CD180_MSVR_CTS			0x20	/* CTS input state */
#define	CD180_MSVR_DTR			0x02	/* DTR output state */
#define	CD180_MSVR_RTS			0x01	/* RTS output state */

#define	CD180_GSVR_IMASK		0x07	/* interrupt type mask */
#define	CD180_GSVR_NOREQUEST		0x00	/* no request pending */
#define	CD180_GSVR_STATCHG		0x01	/* modem signal change */
#define	CD180_GSVR_TXDATA		0x02	/* tx service request */
#define	CD180_GSVR_RXGOOD		0x03	/* rx service request */
#define	CD180_GSVR_reserved1		0x04
#define	CD180_GSVR_reserved2		0x05
#define	CD180_GSVR_reserved3		0x06
#define	CD180_GSVR_RXEXCEPTION		0x07	/* rx exception request */

#define	STTY_RX_FIFO_THRESHOLD	6
#define	STTY_RX_DTR_THRESHOLD	9

#define	CD180_RCSR_TO			0x80	/* time out */
#define	CD180_RCSR_SCD2			0x40	/* special char detect 2 */
#define	CD180_RCSR_SCD1			0x20	/* special char detect 1 */
#define	CD180_RCSR_SCD0			0x10	/* special char detect 0 */
#define	CD180_RCSR_BE			0x08	/* break exception */
#define	CD180_RCSR_PE			0x04	/* parity exception */
#define	CD180_RCSR_FE			0x02	/* framing exception */
#define	CD180_RCSR_OE			0x01	/* overrun exception */

#define	CD180_TX_FIFO_SIZE	8		/* 8 chars of fifo */
/*
 * These are the offsets of the MRAR,TRAR, and RRAR in *IACK space.
 * The high bit must be set as per specs for the MSMR, TSMR, and RSMR.
 */
#define	SPIF_MSMR			0xf5	/* offset of MRAR | 0x80 */
#define	SPIF_TSMR			0xf6	/* offset of TRAR | 0x80 */
#define	SPIF_RSMR			0xf7	/* offset of RRAR | 0x80 */

#define SPIF_MAX_CARDS		4
#define SPIF_MAX_TTY		8
#define SPIF_MAX_BPP		1

#define SPIF_CARD(x)	((minor(x) >> 6) & 0x03)
#define SPIF_PORT(x)	(minor(x) & 0x0f)

#define STTY_DIALOUT(x) (minor(x) & 0x10)

/* "verosc" node tells which oscillator we have.  */
#define	SPIF_OSC9	1		/* 9.8304 Mhz */
#define	SPIF_OSC10	2		/* 10Mhz */

#define	SPIF_PPRH	0xf0
#define	SPIF_PPRL	0x00

#define	STTYF_CDCHG		0x01		/* carrier changed */
#define	STTYF_RING_OVERFLOW	0x02		/* ring buffer overflowed */
#define	STTYF_DONE		0x04		/* done... flush buffers */
#define	STTYF_SET_BREAK		0x08		/* set break signal */
#define	STTYF_CLR_BREAK		0x10		/* clear break signal */
#define	STTYF_STOP		0x20		/* stopped */

#define	STTY_RBUF_SIZE		(2 * 512)
