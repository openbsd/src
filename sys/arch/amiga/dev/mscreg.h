/*	$NetBSD: mscreg.h,v 1.4 1995/10/07 18:22:14 chopps Exp $ */

/*
 * Copyright (c) 1993 Zik.
 * Copyright (c) 1995 Jukka Marin <jmarin@teeri.jmp.fi>.
 * Copyright (c) 1995 Rob Healey <rhealey@kas.helios.mn.org>.
 * Copyright (c) 1982, 1986, 1990 Regents of the University of California.
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
 *    - created by zik 931207
 *    - Fixed break value. 950108 RFH
 *    - Added 6502 field to mscmemory so board can be halted while
 *	it's memory is being reloaded. 950108 RFH
 *    - Ripped out structure guts and replaced with Jukka Marin's stuff for his
 *	freely redistributable version of the 2232 6502c code. Yea!!!!!!
 *	950916 RFH
 *    - Added Jukka's turbo board detection support and tmpbuf for copys. 950918
 *    - Change to NetBSD style for integration in to the main tree. 950919
 */

#define	NUMLINES	7		/* number of lines per card */
#define	IOBUFLEN	256		/* number of bytes per buffer */
#define	IOBUFLENMASK	0xff	/* mask for maximum number of bytes */
#define	IOBUFHIGHWATER	192	/* point at which to enable output */
#define	IOBUFLOWWATER	128	/* point at which to wake output */

#define	MSC_VBL_PRIORITY	1	/* priority of vbl interrupt handler */

#define	MSC_UNKNOWN	0	/* crystal not known */
#define	MSC_NORMAL	1	/* normal A2232 (1.8432 MHz oscillator) */
#define	MSC_TURBO	2	/* turbo A2232 (3.6864 MHz oscillator) */


struct msccommon {
	char   Crystal;	/* normal (1) or turbo (2) board? */
	u_char Pad_a;
	u_char TimerH;	/* timer value after speed check */
	u_char TimerL;
};

struct mscstatus {
	u_char InHead;		/* input queue head */
	u_char InTail;		/* input queue tail */
	u_char Pad_a;		/* paddington */
	u_char Pad_b;		/* paddington */
	u_char OutDisable;	/* disables output */
	u_char OutHead;		/* output queue head */
	u_char OutTail;		/* output queue tail */
	u_char OutCtrl;		/* soft flow control character to send */
	u_char OutFlush;	/* flushes output buffer */
	u_char Setup;		/* causes reconfiguration */
	u_char Param;		/* parameter byte - see MSCPARAM */
	u_char Command;		/* command byte - see MSCCMD */
	u_char SoftFlow;	/* enables xon/xoff flow control */
	/* private 65C02 fields: */
	u_char chCD;		/* used to detect CD changes */
	u_char XonOff;		/* stores XON/XOFF enable/disable */
	u_char Pad_c;		/* paddington */
};

#define	MSC_MEMPAD	\
    (0x0200 - NUMLINES * sizeof(struct mscstatus) - sizeof(struct msccommon))
	
struct mscmemory {
	struct mscstatus Status[NUMLINES];	/* 0x0000-0x006f status areas */
	struct msccommon Common;		/* 0x0070-0x0073 common flags */
	u_char Dummy1[MSC_MEMPAD];		/* 0x00XX-0x01ff */
	u_char OutBuf[NUMLINES][IOBUFLEN];	/* 0x0200-0x08ff output bufs */
	u_char InBuf[NUMLINES][IOBUFLEN];	/* 0x0900-0x0fff input bufs */
	u_char InCtl[NUMLINES][IOBUFLEN];	/* 0x1000-0x16ff control data */
	u_char Dummy2[ 0x2000 - 7 * 0x0100];	/* 0x1700-0x2fff */
	u_char Code[0x1000];			/* 0x3000-0x3fff code area */
	u_short InterruptAck;			/* 0x4000        intr ack */
	u_char Dummy3[0x3ffe];			/* 0x4002-0x7fff */
	u_short Enable6502Reset;		/* 0x8000 Stop board, */
						/*  6502 RESET line held low */
	u_char Dummy4[0x3ffe];			/* 0x8002-0xbfff */
	u_short ResetBoard;			/* 0xc000 reset board & run, */
						/*  6502 RESET line held high */
};

#undef MSC_MEMPAD

struct mscdevice {
	volatile struct mscmemory *board;	/* where the board is located */
	int flags;			/* modem control flags */
	int openflags;			/* flags for device open */
	u_char port;			/* which port on the board (0-6) */
	u_char active;			/* does this port have hardware? */
	u_char closing;			/* are we flushing before close? */
	u_char paddington;		/* just for padding */
	char tmpbuf[IOBUFLEN];		/* temp buffer for data transfers */
};

#define	MSCINCTL_CHAR	0	/* corresponding byte in InBuf is a character */
#define	MSCINCTL_EVENT	1	/* corresponding byte in InBuf is an event */

#define	MSCEVENT_Break		1	/* break set */
#define	MSCEVENT_CarrierOn	2	/* carrier raised */
#define	MSCEVENT_CarrierOff	3	/* carrier dropped */

#define	MSCCMD_Enable		0x1	/* enable/DTR bit */
#define	MSCCMD_Close		0x2	/* close the device */
#define	MSCCMD_Open		0xb	/* open the device */
#define	MSCCMD_CMask		0xf	/* command mask */
#define	MSCCMD_RTSOff		0x0  	/* turn off RTS */
#define	MSCCMD_RTSOn		0x8	/* turn on RTS */
#define	MSCCMD_Break		0xd	/* transmit a break */
#define	MSCCMD_RTSMask		0xc	/* mask for RTS stuff */
#define	MSCCMD_NoParity		0x00	/* don't use parity */
#define	MSCCMD_OddParity	0x20	/* odd parity */
#define	MSCCMD_EvenParity	0x60	/* even parity */
#define	MSCCMD_ParityMask	0xe0	/* parity mask */

#define	MSCPARAM_B115200	0x0	/* baud rates */
#define	MSCPARAM_B50		0x1
#define	MSCPARAM_B75		0x2
#define	MSCPARAM_B110		0x3
#define	MSCPARAM_B134		0x4
#define	MSCPARAM_B150		0x5
#define	MSCPARAM_B300		0x6
#define	MSCPARAM_B600		0x7
#define	MSCPARAM_B1200		0x8
#define	MSCPARAM_B1800		0x9
#define	MSCPARAM_B2400		0xa
#define	MSCPARAM_B3600		0xb
#define	MSCPARAM_B4800		0xc
#define	MSCPARAM_B7200		0xd
#define	MSCPARAM_B9600		0xe
#define	MSCPARAM_B19200		0xf
#define	MSCPARAM_BaudMask	0xf	/* baud rate mask */
#define	MSCPARAM_RcvBaud	0x10	/* enable receive baud rate */
#define	MSCPARAM_8Bit		0x00	/* numbers of bits */
#define	MSCPARAM_7Bit		0x20
#define	MSCPARAM_6Bit		0x40
#define	MSCPARAM_5Bit		0x60
#define	MSCPARAM_BitMask	0x60	/* numbers of bits mask */

/* tty number from device */
#define	MSCTTY(dev)	(minor(dev) & 0x7e)

/* slot number from device */
#define	MSCSLOT(dev)	((minor(dev) & 0x7e)>>1)

/* dialin mode from device */
#define	MSCDIALIN(dev)	(minor(dev) & 0x01)

/* board number from device */
#define	MSCBOARD(dev)	((minor(dev))>>4)

/* line number from device */
#define	MSCLINE(dev)	(((minor(dev)) & 0xe)>>1)

/* number of slots */
#define	MSCSLOTS	((NMSC-1)*8+7)

/* number of ttys */
#define	MSCTTYS		(MSCSLOTS*2)

/* board number given slot number */
#define	MSCUNIT(slot)	((slot)>>3)

/* slot number given unit and line */
#define	MSCSLOTUL(unit,line)	(((unit)<<3)+(line))

/* tty number given slot */
#define	MSCTTYSLOT(slot)	((slot)<<1)

#ifndef TRUE
#define	TRUE	1
#define	FALSE	0
#endif
