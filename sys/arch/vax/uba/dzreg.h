/*	$NetBSD: dzreg.h,v 1.1 1996/04/08 17:22:21 ragge Exp $	*/
/*
 * Copyright (c) 1996  Ken C. Wellsch.  All rights reserved.
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
 */

union w_b
{
	u_short word;
	struct {
		u_char byte_lo;
		u_char byte_hi;
	} bytes;
};

struct DZregs
{
	volatile u_short dz_csr;	/* Control/Status Register (R/W) */
	volatile u_short dz_rbuf;	/* Receive Buffer (R only) */
#define dz_lpr		 dz_rbuf	/* Line Parameter Register (W only) */
	volatile union w_b u_tcr;	/* Transmit Control Register (R/W) */
	volatile union w_b u_msr;	/* Modem Status Register (R only) */
#define u_tdr		 u_msr		/* Transmit Data Register (W only) */
};

#define dz_tcr		u_tcr.bytes.byte_lo	/* tx enable bits */
#define dz_dtr		u_tcr.bytes.byte_hi	/* DTR status bits */
#define dz_ring		u_msr.bytes.byte_lo	/* RI status bits */
#define dz_dcd		u_msr.bytes.byte_hi	/* DCD status bits */
#define dz_tbuf		u_tdr.bytes.byte_lo	/* transmit character */
#define dz_break	u_tdr.bytes.byte_hi	/* BREAK set/clr bits */

typedef struct DZregs dzregs;

/* CSR bits */

#define DZ_CSR_TX_READY		0100000	/* Transmitter Ready */
#define DZ_CSR_TXIE		0040000	/* Transmitter Interrupt Enable */
#define DZ_CSR_SA		0020000	/* Silo Alarm */
#define DZ_CSR_SAE		0010000	/* Silo Alarm Enable */
#define DZ_CSR_TX_LINE_MASK	0007400	/* Which TX line */

#define DZ_CSR_RX_DONE		0000200	/* Receiver Done */
#define DZ_CSR_RXIE		0000100	/* Receiver Interrupt Enable */
#define DZ_CSR_MSE		0000040	/* Master Scan Enable */
#define DZ_CSR_RESET		0000020	/* Clear (reset) Controller */
#define DZ_CSR_MAINTENANCE	0000010
#define DZ_CSR_UNUSED		0000007

/* RBUF bits */

#define DZ_RBUF_DATA_VALID	0100000
#define DZ_RBUF_OVERRUN_ERR	0040000
#define DZ_RBUF_FRAMING_ERR	0020000
#define DZ_RBUF_PARITY_ERR	0010000
#define DZ_RBUF_RX_LINE_MASK	0007400

/* LPR bits */

#define DZ_LPR_UNUSED		0160000
#define DZ_LPR_RX_ENABLE	0010000

#define DZ_LPR_B50		0x0
#define DZ_LPR_B75		0x1
#define DZ_LPR_B110		0x2
#define DZ_LPR_B134		0x3
#define DZ_LPR_B150		0x4
#define DZ_LPR_B300		0x5
#define DZ_LPR_B600		0x6
#define DZ_LPR_B1200		0x7
#define DZ_LPR_B1800		0x8
#define DZ_LPR_B2000		0x9
#define DZ_LPR_B2400		0xA
#define DZ_LPR_B3600		0xB
#define DZ_LPR_B4800		0xC
#define DZ_LPR_B7200		0xD
#define DZ_LPR_B9600		0xE
#define DZ_LPR_ILLEGAL		0xF

#define DZ_LPR_OPAR		0000200
#define DZ_LPR_PARENB		0000100
#define DZ_LPR_2_STOP		0000040

#define DZ_LPR_5_BIT_CHAR	0000000
#define DZ_LPR_6_BIT_CHAR	0000010
#define DZ_LPR_7_BIT_CHAR	0000020
#define DZ_LPR_8_BIT_CHAR	0000030

#define DZ_LPR_CHANNEL_MASK	0000007
