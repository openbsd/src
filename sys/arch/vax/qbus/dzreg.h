/*	$OpenBSD: dzreg.h,v 1.3 2006/07/31 18:51:06 miod Exp $	*/
/*	$NetBSD: dzreg.h,v 1.4 1999/05/27 16:03:13 ragge Exp $ */
/*
 * Copyright (c) 1996  Ken C. Wellsch.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

struct	dz_regs	{
	bus_addr_t dr_csr;
	bus_addr_t dr_rbuf;
#define dr_lpr	   dr_rbuf
	bus_addr_t dr_dtr;
	bus_addr_t dr_break;
	bus_addr_t dr_tbuf;
	bus_addr_t dr_tcr;
	bus_addr_t dr_tcrw;
	bus_addr_t dr_ring;
	bus_addr_t dr_dcd;
};
#define	DZ_UBA_CSR	0
#define	DZ_UBA_RBUF	2
#define	DZ_UBA_DTR	5
#define	DZ_UBA_BREAK	7
#define	DZ_UBA_TBUF	6
#define	DZ_UBA_TCR	4
#define	DZ_UBA_DCD	7
#define	DZ_UBA_RING	6

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
#define DZ_LPR_B19200		0xF

#define DZ_LPR_OPAR		0000200
#define DZ_LPR_PARENB		0000100
#define DZ_LPR_2_STOP		0000040

#define DZ_LPR_5_BIT_CHAR	0000000
#define DZ_LPR_6_BIT_CHAR	0000010
#define DZ_LPR_7_BIT_CHAR	0000020
#define DZ_LPR_8_BIT_CHAR	0000030

#define DZ_LPR_CHANNEL_MASK	0000007
