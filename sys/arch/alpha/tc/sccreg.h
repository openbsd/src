/* $OpenBSD: sccreg.h,v 1.5 2003/06/02 23:27:44 millert Exp $ */
/* $NetBSD: sccreg.h,v 1.3 1997/04/06 22:30:30 cgd Exp $ */

/* 
 * Copyright (c) 1991,1990,1989,1994,1995 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
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
 *	@(#)sccreg.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Definitions for Intel 82530 serial communications chip.  Each chip is a
 * dual uart with the A channels used for the keyboard and mouse with the B
 * channel(s) for comm ports with modem control. Since some registers are
 * used for the other channel, the following macros are used to access the
 * register ports.
 *
 * Actual access to the registers is provided by sccvar.h, as it's
 * machine-dependent.
 */

/* Scc channel numbers; B channel comes first. */
#define	SCC_CHANNEL_B	0
#define	SCC_CHANNEL_A	1

#define	SCC_INIT_REG(scc, chan) {					\
	char tmp;							\
	scc_get_datum((scc)->scc_channel[(chan)].scc_command, tmp);	\
	scc_get_datum((scc)->scc_channel[(chan)].scc_command, tmp);	\
}

#define	SCC_READ_REG(scc, chan, reg, val) {				\
	scc_set_datum((scc)->scc_channel[(chan)].scc_command, reg);	\
	scc_get_datum((scc)->scc_channel[(chan)].scc_command, val);	\
}

#define	SCC_READ_REG_ZERO(scc, chan, val) {				\
	scc_get_datum((scc)->scc_channel[(chan)].scc_command, val);	\
}

#define	SCC_WRITE_REG(scc, chan, reg, val) {				\
	scc_set_datum((scc)->scc_channel[(chan)].scc_command, reg);	\
	scc_set_datum((scc)->scc_channel[(chan)].scc_command, val);	\
}

#define	SCC_WRITE_REG_ZERO(scc, chan, val) {				\
	scc_set_datum((scc)->scc_channel[(chan)].scc_command, val);	\
}

#define	SCC_READ_DATA(scc, chan, val) {					\
	scc_get_datum((scc)->scc_channel[(chan)].scc_data, val);	\
}

#define	SCC_WRITE_DATA(scc, chan, val) {				\
	scc_set_datum((scc)->scc_channel[(chan)].scc_data, val);	\
}

/* Addressable registers. */
#define	SCC_RR0		0	/* status register */
#define	SCC_RR1		1	/* special receive conditions */
#define	SCC_RR8		8	/* recv buffer (alias for data) */
#define	SCC_RR10	10	/* sdlc status */
#define	SCC_RR15	15	/* interrupts currently enabled */

#define	SCC_WR0		0	/* reg select, and commands */
#define	SCC_WR1		1	/* interrupt and DMA enables */
#define	SCC_WR3		3	/* receiver params and enables */
#define	SCC_WR4		4	/* clock/char/parity params */
#define	SCC_WR5		5	/* xmit params and enables */
#define	SCC_WR8		8	/* xmit buffer (alias for data) */
#define	SCC_WR9		9	/* vectoring and resets */
#define	SCC_WR10	10	/* synchr params */
#define	SCC_WR11	11	/* clocking definitions */
#define	SCC_WR14	14	/* BRG enables and commands */
#define	SCC_WR15	15	/* interrupt enables */

/* Read register's defines. */

/*
 * RR2 contains the interrupt vector unmodified (channel A) or
 * modified as follows (channel B, if vector-include-status).
 */
#define	SCC_RR2_STATUS(val)	((val)&0xf)

#define	SCC_RR2_B_XMIT_DONE	0x0
#define	SCC_RR2_B_EXT_STATUS	0x2
#define	SCC_RR2_B_RECV_DONE	0x4
#define	SCC_RR2_B_RECV_SPECIAL	0x6
#define	SCC_RR2_A_XMIT_DONE	0x8
#define	SCC_RR2_A_EXT_STATUS	0xa
#define	SCC_RR2_A_RECV_DONE	0xc
#define	SCC_RR2_A_RECV_SPECIAL	0xe

/* RR12/RR13 hold the timing base, upper byte in RR13. */
#define	SCC_GET_TIMING_BASE(scc, chan, val) {				\
		register char tmp;					\
		SCC_READ_REG(scc, chan, ZSRR_BAUDLO, val);		\
		SCC_READ_REG(scc, chan, ZSRR_BAUDHI, tmp);		\
		(val) = ((val) << 8) | (tmp & 0xff);			\
	}

/*
 * Write register's defines.
 */

/* WR12/WR13 are for timing base preset */
#define	SCC_SET_TIMING_BASE(scc, chan, val) {				\
		SCC_WRITE_REG(scc, chan, ZSWR_BAUDLO, val);		\
		SCC_WRITE_REG(scc, chan, ZSWR_BAUDHI, (val) >> 8);	\
	}

/* Bits in dm lsr, copied from dmreg.h. */
#define	DML_DSR		0000400		/* data set ready, not a real DM bit */
#define	DML_RNG		0000200		/* ring */
#define	DML_CAR		0000100		/* carrier detect */
#define	DML_CTS		0000040		/* clear to send */
#define	DML_SR		0000020		/* secondary receive */
#define	DML_ST		0000010		/* secondary transmit */
#define	DML_RTS		0000004		/* request to send */
#define	DML_DTR		0000002		/* data terminal ready */
#define	DML_LE		0000001		/* line enable */
