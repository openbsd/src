/*	$OpenBSD: dlreg.h,v 1.2 1998/05/11 14:59:06 niklas Exp $	*/
/*	$NetBSD: dlreg.h,v 1.1 1997/02/04 19:13:19 ragge Exp $	*/
/*
 * Copyright (c) 1997  Ben Harris.  All rights reserved.
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
 *	This product includes software developed by Ben Harris.
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

/*
 * dlreg.h -- Definitions for the DL11 and DLV11 serial cards.
 *
 * Style in imitation of dzreg.h.
 */

union w_b
{
	u_short word;
	struct {
		u_char byte_lo;
		u_char byte_hi;
	} bytes;
};

struct DLregs
{
	volatile u_short dl_rcsr; /* Receive Control/Status Register (R/W) */
	volatile u_short dl_rbuf; /* Receive Buffer (R) */
	volatile u_short dl_xcsr; /* Transmit Control/Status Register (R/W) */
	volatile union w_b u_xbuf; /* Transmit Buffer (W) */
#define dl_xbuf u_xbuf.bytes.byte_lo	
};

typedef struct DLregs dlregs;

/* RCSR bits */

#define DL_RCSR_RX_DONE		0x0080 /* Receiver Done (R) */
#define DL_RCSR_RXIE		0x0040 /* Receiver Interrupt Enable (R/W) */
#define DL_RCSR_READER_ENABLE	0x0001 /* [paper-tape] Reader Enable (W) */
#define DL_RCSR_BITS		"\20\1READER_ENABLE\7RXIE\10RX_DONE\n"

/* RBUF bits */

#define DL_RBUF_ERR		0x8000 /* Error (R) */
#define DL_RBUF_OVERRUN_ERR	0x4000 /* Overrun Error (R) */
#define DL_RBUF_FRAMING_ERR	0x2000 /* Framing Error (R) */
#define DL_RBUF_PARITY_ERR	0x1000 /* Parity Error (R) */
#define DL_RBUF_DATA_MASK	0x00FF /* Receive Data (R) */
#define DL_RBUF_BITS	"\20\15PARITY_ERR\16FRAMING_ERR\17OVERRUN_ERR\20ERR\n"

/* XCSR bits */

#define DL_XCSR_TX_READY	0x0080 /* Transmitter Ready (R) */
#define DL_XCSR_TXIE		0x0040 /* Transmit Interrupt Enable (R/W) */
#define DL_XCSR_TX_BREAK	0x0001 /* Transmit Break (R/W) */
#define DL_XCSR_BITS		"\20\1TX_BREAK\7TXIE\10TX_READY\n"

/* XBUF is just data byte right justified. */
