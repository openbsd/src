/*	$OpenBSD: dhureg.h,v 1.2 2002/12/27 19:20:49 hugh Exp $	*/
/*	$NetBSD: dhureg.h,v 1.4 1999/05/28 20:17:29 ragge Exp $	*/
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

#ifdef notdef
union w_b
{
	u_short word;
	struct {
		u_char byte_lo;
		u_char byte_hi;
	} bytes;
};

struct DHUregs
{
	volatile union w_b u_csr;	/* Control/Status Register (R/W) */
	volatile u_short dhu_rbuf;	/* Receive Buffer (R only) */
#define dhu_txchar	 dhu_rbuf	/* Transmit Character (W only) */
	volatile u_short dhu_lpr;	/* Line Parameter Register (R/W) */
	volatile u_short dhu_stat;	/* Line Status (R only) */
	volatile u_short dhu_lnctrl;	/* Line Control (R/W) */
	volatile u_short dhu_tbufad1;	/* Transmit Buffer Address 1 (R/W) */
	volatile u_short dhu_tbufad2;	/* Transmit Buffer Address 2 (R/W) */
	volatile u_short dhu_tbufcnt;	/* Transmit Buffer Count (R/W) */
};

#define dhu_csr		u_csr.word
#define dhu_csr_lo	u_csr.bytes.byte_lo
#define dhu_csr_hi	u_csr.bytes.byte_hi

typedef struct DHUregs dhuregs;
#endif

#define	DHU_UBA_CSR	0
#define	DHU_UBA_CSR_HI	1
#define	DHU_UBA_RBUF	2
#define	DHU_UBA_TXCHAR	2
#define	DHU_UBA_LPR	4
#define	DHU_UBA_STAT	6
#define	DHU_UBA_LNCTRL	8
#define	DHU_UBA_TBUFAD1	10
#define	DHU_UBA_TBUFAD2	12
#define	DHU_UBA_TBUFCNT	14

/* CSR bits */

#define DHU_CSR_TX_ACTION	0100000
#define DHU_CSR_TXIE		0040000
#define DHU_CSR_DIAG_FAIL	0020000
#define DHU_CSR_TX_DMA_ERROR	0010000
#define DHU_CSR_TX_LINE_MASK	0007400
#define DHU_CSR_RX_DATA_AVAIL	0000200
#define DHU_CSR_RXIE		0000100
#define DHU_CSR_MASTER_RESET	0000040
#define DHU_CSR_UNUSED		0000020
#define DHU_CSR_CHANNEL_MASK	0000017

/* RBUF bits */

#define DHU_RBUF_DATA_VALID	0100000
#define DHU_RBUF_OVERRUN_ERR	0040000
#define DHU_RBUF_FRAMING_ERR	0020000
#define DHU_RBUF_PARITY_ERR	0010000
#define DHU_RBUF_RX_LINE_MASK	0007400

#define DHU_DIAG_CODE		0070001
#define DHU_MODEM_CODE		0070000

/* TXCHAR bits */

#define DHU_TXCHAR_DATA_VALID	0100000

/* LPR bits */

#define DHU_LPR_B50		0x0
#define DHU_LPR_B75		0x1
#define DHU_LPR_B110		0x2
#define DHU_LPR_B134		0x3
#define DHU_LPR_B150		0x4
#define DHU_LPR_B300		0x5
#define DHU_LPR_B600		0x6
#define DHU_LPR_B1200		0x7
#define DHU_LPR_B1800		0x8
#define DHU_LPR_B2000		0x9
#define DHU_LPR_B2400		0xA
#define DHU_LPR_B4800		0xB
#define DHU_LPR_B7200		0xC
#define DHU_LPR_B9600		0xD
#define DHU_LPR_B19200		0xE
#define DHU_LPR_B38400		0xF

#define DHU_LPR_5_BIT_CHAR	0000000
#define DHU_LPR_6_BIT_CHAR	0000010
#define DHU_LPR_7_BIT_CHAR	0000020
#define DHU_LPR_8_BIT_CHAR	0000030
#define DHU_LPR_PARENB		0000040
#define DHU_LPR_EPAR		0000100
#define DHU_LPR_2_STOP		0000200

/* STAT bits */

#define DHU_STAT_DSR		0100000
#define DHU_STAT_RI		0020000
#define DHU_STAT_DCD		0010000
#define DHU_STAT_CTS		0004000
#define DHU_STAT_MDL		0001000
#define DHU_STAT_DHU		0000400

/* LNCTRL bits */

#define DHU_LNCTRL_DMA_ABORT	0000001
#define DHU_LNCTRL_IAUTO	0000002
#define DHU_LNCTRL_RX_ENABLE	0000004
#define DHU_LNCTRL_BREAK	0000010
#define DHU_LNCTRL_OAUTO	0000020
#define DHU_LNCTRL_FORCE_XOFF	0000040
#define DHU_LNCTRL_LINK_TYPE	0000400
#define DHU_LNCTRL_DTR		0001000
#define DHU_LNCTRL_RTS		0010000

/* TBUFAD2 bits */

#define DHU_TBUFAD2_DMA_START	0000200
#define DHU_TBUFAD2_TX_ENABLE	0100000
