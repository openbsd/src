/*	$OpenBSD: spifreg.h,v 1.4 1999/02/23 23:47:48 jason Exp $	*/

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

/* Parallel Status: read only */
#define	PPC_PSTAT_ERROR		0x08		/* error */
#define	PPC_PSTAT_SELECT	0x10		/* select */
#define	PPC_PSTAT_PAPER		0x20		/* paper out */
#define	PPC_PSTAT_ACK		0x40		/* ack */
#define	PPC_PSTAT_BUSY		0x80		/* busy */

/* Parallel Control: read/write */
#define	PPC_CTRL_STROBE		0x01		/* strobe, 1=drop strobe */
#define	PPC_CTRL_AFX		0x02		/* auto form-feed */
#define	PPC_CTRL_INIT		0x04		/* init, 1=enable printer */
#define	PPC_CTRL_SLCT		0x08		/* SLC, 1=select printer */
#define	PPC_CTRL_IRQE		0x10		/* IRQ, 1=enable intrs */
#define	PPC_CTRL_OUTPUT		0x20		/* direction: 1=ppc out */

/*
 * The 'stc' is a Cirrus Logic CL-CD180 (either revision B or revision C)
 */
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

/* Global Firmware Revision Code Register (rw) */
#define	CD180_GFRCR_REV_B	0x81		/* CL-CD180B */
#define	CD180_GFRCR_REV_C	0x82		/* CL-CD180C */

/* Service Request Configuration Register (rw) (CD180C or higher) */
#define	CD180_SRCR_PKGTYP		0x80	/* pkg type,0=PLCC,1=PQFP */
#define	CD180_SRCR_REGACKEN		0x40	/* register ack enable */
#define	CD180_SRCR_DAISYEN		0x20	/* daisy chain enable */
#define	CD180_SRCR_GLOBPRI		0x10	/* global priority */
#define	CD180_SRCR_UNFAIR		0x08	/* use unfair interrupts */
#define	CD180_SRCR_AUTOPRI		0x02	/* automatic priority */
#define	CD180_SRCR_PRISEL		0x01	/* select rx/tx as high pri */

/* Prescalar Period Register High (rw) */
#define	CD180_PPRH	0xf0		/* high byte */
#define	CD180_PPRL	0x00		/* low byte */

/* Global Service Vector Register (rw) */
/* Modem Request Acknowledgement Register (ro) (and IACK equivalent) */
/* Receive Request Acknowledgement Register (ro) (and IACK equivalent) */
/* Transmit Request Acknowledgement Register (ro) (and IACK equivalent) */
#define	CD180_GSVR_USERMASK		0xf8	/* user defined bits */
#define	CD180_GSVR_IMASK		0x07	/* interrupt type mask */
#define	CD180_GSVR_NOREQUEST		0x00	/* no request pending */
#define	CD180_GSVR_STATCHG		0x01	/* modem signal change */
#define	CD180_GSVR_TXDATA		0x02	/* tx service request */
#define	CD180_GSVR_RXGOOD		0x03	/* rx service request */
#define	CD180_GSVR_reserved1		0x04	/* reserved */
#define	CD180_GSVR_reserved2		0x05	/* reserved */
#define	CD180_GSVR_reserved3		0x06	/* reserved */
#define	CD180_GSVR_RXEXCEPTION		0x07	/* rx exception request */

/* Service Request Status Register (ro) (CD180C and higher) */
#define	CD180_SRSR_MREQINT		0x01	/* modem request internal */
#define	CD180_SRSR_MREQEXT		0x02	/* modem request external */
#define	CD180_SRSR_TREQINT		0x04	/* tx request internal */
#define	CD180_SRSR_TREQEXT		0x08	/* tx request external */
#define	CD180_SRSR_RREQINT		0x10	/* rx request internal */
#define	CD180_SRSR_RREQEXT		0x20	/* rx request external */
#define	CD180_SRSR_ILV_MASK		0xc0	/* internal service context */
#define	CD180_SRSR_ILV_NONE		0x00	/* not in service context */
#define	CD180_SRSR_ILV_RX		0xc0	/* in rx service context */
#define	CD180_SRSR_ILV_TX		0x80	/* in tx service context */
#define	CD180_SRSR_ILV_MODEM		0x40	/* in modem service context */

/* Global Service Channel Register 1,2,3 (rw) */
#define	CD180_GSCR_CHANNEL(gscr)	(((gscr) >> 2) & 7)

/* Receive Data Count Register (ro) */
#define	CD180_RDCR_MASK			0x0f	/* mask for fifo length */

/* Receive Character Status Register (ro) */
#define	CD180_RCSR_TO			0x80	/* time out */
#define	CD180_RCSR_SCD2			0x40	/* special char detect 2 */
#define	CD180_RCSR_SCD1			0x20	/* special char detect 1 */
#define	CD180_RCSR_SCD0			0x10	/* special char detect 0 */
#define	CD180_RCSR_BE			0x08	/* break exception */
#define	CD180_RCSR_PE			0x04	/* parity exception */
#define	CD180_RCSR_FE			0x02	/* framing exception */
#define	CD180_RCSR_OE			0x01	/* overrun exception */

/* Service Request Enable Register (rw) */
#define	CD180_SRER_DSR			0x80	/* DSR service request */
#define	CD180_SRER_CD			0x40	/* CD service request */
#define	CD180_SRER_CTS			0x20	/* CTS service request */
#define	CD180_SRER_RXD			0x10	/* RXD service request */
#define	CD180_SRER_RXSCD		0x08	/* RX special char request */
#define	CD180_SRER_TXD			0x04	/* TX ready service request */
#define	CD180_SRER_TXE			0x02	/* TX empty service request */
#define	CD180_SRER_NNDT			0x01	/* No new data timeout req */

/* Channel Command Register (rw) */
/* Reset Channel Command */
#define	CD180_CCR_CMD_RESET		0x80	/* chip/channel reset */
#define CD180_CCR_RESETALL		0x01	/* global reset */
#define	CD180_CCR_RESETCHAN		0x00	/* current channel reset */
/* Channel Option Register Command */
#define	CD180_CCR_CMD_COR		0x40	/* channel opt reg changed */
#define	CD180_CCR_CORCHG1		0x02	/* cor1 has changed */
#define	CD180_CCR_CORCHG2		0x04	/* cor2 has changed */
#define	CD180_CCR_CORCHG3		0x08	/* cor3 has changed */
/* Send Special Character Command */
#define	CD180_CCR_CMD_SPC		0x20	/* send special chars changed */
#define	CD180_CCR_SSPC0			0x01	/* send special char 0 change */
#define	CD180_CCR_SSPC1			0x02	/* send special char 1 change */
#define	CD180_CCR_SSPC2			0x04	/* send special char 2 change */
/* Channel Control Command */
#define	CD180_CCR_CMD_CHAN		0x10	/* channel control command */
#define	CD180_CCR_CHAN_TXEN		0x08	/* enable channel tx */
#define	CD180_CCR_CHAN_TXDIS		0x04	/* disable channel tx */
#define	CD180_CCR_CHAN_RXEN		0x02	/* enable channel rx */
#define	CD180_CCR_CHAN_RXDIS		0x01	/* disable channel rx */

/* Channel Option Register 1 (rw) */
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

/* Channel Option Register 2 (rw) */
#define	CD180_COR2_IXM			0x80	/* implied xon mode */
#define	CD180_COR2_TXIBE		0x40	/* tx in-band flow control */
#define	CD180_COR2_ETC			0x20	/* embedded tx command enbl */
#define	CD180_COR2_LLM			0x10	/* local loopback mode */
#define	CD180_COR2_RLM			0x08	/* remote loopback mode */
#define	CD180_COR2_RTSAO		0x04	/* RTS automatic output enbl */
#define	CD180_COR2_CTSAE		0x02	/* CTS automatic enable */
#define	CD180_COR2_DSRAE		0x01	/* DSR automatic enable */

/* Channel Option Register 3 (rw) */
#define	CD180_COR3_XON2			0x80	/* XON char in spc1&3 */
#define	CD180_COR3_XON1			0x00	/* XON char in spc1 */
#define	CD180_COR3_XOFF2		0x40	/* XOFF char in spc2&4 */
#define	CD180_COR3_XOFF1		0x00	/* XOFF char in spc2 */
#define	CD180_COR3_FCT			0x20	/* flow control transparency */
#define	CD180_COR3_SCDE			0x10	/* special char recognition */
#define	CD180_COR3_RXFIFO_MASK		0x0f	/* rx fifo threshold */

/* Channel Control Status Register (ro) */
#define	CD180_CCSR_RXEN			0x80	/* rx is enabled */
#define	CD180_CCSR_RXFLOFF		0x40	/* rx flow-off */
#define	CD180_CCSR_RXFLON		0x20	/* rx flow-on */
#define	CD180_CCSR_TXEN			0x08	/* tx is enabled */
#define	CD180_CCSR_TXFLOFF		0x04	/* tx flow-off */
#define	CD180_CCSR_TXFLON		0x02	/* tx flow-on */

/* Receiver Bit Register (ro) */
#define	CD180_RBR_RXD			0x40	/* state of RxD pin */
#define	CD180_RBR_STARTHUNT		0x20	/* looking for start bit */

/* Modem Change Register (rw) */
#define	CD180_MCR_DSR			0x80	/* DSR changed */
#define	CD180_MCR_CD			0x40	/* CD changed */
#define	CD180_MCR_CTS			0x20	/* CTS changed */

/* Modem Change Option Register 1 (rw) */
#define	CD180_MCOR1_DSRZD		0x80	/* catch 0->1 DSR changes */
#define	CD180_MCOR1_CDZD		0x40	/* catch 0->1 CD changes */
#define	CD180_MCOR1_CTSZD		0x40	/* catch 0->1 CTS changes */
#define	CD180_MCOR1_DTRTHRESH		0x0f	/* DTR threshold mask */

/* Modem Change Option Register 2 (rw) */
#define	CD180_MCOR2_DSROD		0x80	/* catch 1->0 DSR changes */
#define	CD180_MCOR2_CDOD		0x40	/* catch 1->0 CD changes */
#define	CD180_MCOR2_CTSOD		0x20	/* catch 1->0 CTS changes */

/* Modem Signal Value Register (rw) */
#define	CD180_MSVR_DSR			0x80	/* DSR input state */
#define	CD180_MSVR_CD			0x40	/* CD input state */
#define	CD180_MSVR_CTS			0x20	/* CTS input state */
#define	CD180_MSVR_DTR			0x02	/* DTR output state */
#define	CD180_MSVR_RTS			0x01	/* RTS output state */

/* Modem Signal Value Register - Request To Send (w) (CD180C and higher) */
#define	CD180_MSVRTS_RTS		0x01	/* RTS signal value */

/* Modem Signal Value Register - Data Terminal Ready (w) (CD180C and higher) */
#define	CD180_MSVDTR_DTR		0x02	/* DTR signal value */

/*
 * The register map for the SUNW,spif looks something like:
 *    Offset:		Function:
 *	0000 - 03ff	Boot ROM
 *	0400 - 0408	dtr latches (one per port)
 *	0409 - 07ff	unused
 *	0800 - 087f	CD180 registers (normal mapping)
 *	0880 - 0bff	unused
 *	0c00 - 0c7f	CD180 registers (*iack mapping)
 *	0c80 - 0dff	unused
 *	0e00 - 1fff	PPC registers
 *
 * One note about the DTR latches:  The values stored there are reversed.
 * By writing a 1 to the latch, DTR is lowered, and by writing a 0, DTR
 * is raised.  The latches cannot be read, and no other value can be written
 * there or the system will crash due to "excessive bus loading (see
 * SBus loading and capacitance spec)"
 *
 * The *iack registers are read/written with the IACK bit set.  When
 * the interrupt routine starts, it reads the MRAR, TRAR, and RRAR registers
 * from this mapping.  This signals an interrupt acknowlegement cycle.
 * (NOTE: these are not really the MRAR, TRAR, and RRAR... They are copies
 * of the GSVR, I just mapped them to the same location as the mrar, trar,
 * and rrar because it seemed appropriate).
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

/*
 * The mapping of minor device number -> card and port is done as
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
#define SPIF_MAX_CARDS		4
#define SPIF_MAX_TTY		8
#define SPIF_MAX_BPP		1

/*
 * device selectors
 */
#define SPIF_CARD(x)	((minor(x) >> 6) & 0x03)
#define SPIF_PORT(x)	(minor(x) & 0x0f)
#define STTY_DIALOUT(x) (minor(x) & 0x10)

#define	STTY_RX_FIFO_THRESHOLD	6
#define	STTY_RX_DTR_THRESHOLD	7
#define	CD180_TX_FIFO_SIZE	8		/* 8 chars of fifo */

/*
 * These are the offsets of the MRAR, TRAR, and RRAR in *IACK space.
 * The high bit must be set as per specs for the MSMR, TSMR, and RSMR.
 */
#define	SPIF_MSMR			0xf5	/* offset of MRAR | 0x80 */
#define	SPIF_TSMR			0xf6	/* offset of TRAR | 0x80 */
#define	SPIF_RSMR			0xf7	/* offset of RRAR | 0x80 */

/*
 * "verosc" node tells which oscillator we have.
 */
#define	SPIF_OSC9	1		/* 9.8304 Mhz */
#define	SPIF_OSC10	2		/* 10Mhz */

/*
 * There are two interrupts, serial gets interrupt[0], and parallel
 * gets interrupt[1]
 */
#define	SERIAL_INTR	0
#define	PARALLEL_INTR	1

/*
 * spif tty flags
 */
#define	STTYF_CDCHG		0x01		/* carrier changed */
#define	STTYF_RING_OVERFLOW	0x02		/* ring buffer overflowed */
#define	STTYF_DONE		0x04		/* done... flush buffers */
#define	STTYF_SET_BREAK		0x08		/* set break signal */
#define	STTYF_CLR_BREAK		0x10		/* clear break signal */
#define	STTYF_STOP		0x20		/* stopped */

#define	STTY_RBUF_SIZE		(2 * 512)
