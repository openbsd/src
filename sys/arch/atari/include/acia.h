/*	$NetBSD: acia.h,v 1.3 1995/06/09 19:47:30 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
 * 4. The name of the author may not be used to endorse or promote products
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

#ifndef _MACHINE_ACIA_H
#define _MACHINE_ACIA_H
/*
 * Atari ST hardware:
 * Motorola 6850 Asynchronous Communications Interface Adapter
 */

#define	KBD	(((struct acia *)AD_ACIA))
#define	MDI	(((struct acia *)AD_ACIA) + 1)

struct acia {
	volatile u_char	acb[4];	/* use only the even bytes */
};

#define	ac_cs	acb[0]		/* control and status register	*/
#define	ac_da	acb[2]		/* data register		*/

/* bits in control register: */
/*		0x03		*//* clock divider */
#define A_Q01	0x00		/* don't divide				*/
#define A_Q16	0x01		/* divide by 16				*/
#define A_Q64	0x02		/* divide by 64				*/
#define A_RESET	0x03		/* master reset				*/
/*		0x1C		*//* word select bits */
#define	A_72E	0x00		/* 7 data, 2 stop, parity even		*/
#define	A_72O	0x04		/* 7 data, 2 stop, parity odd		*/
#define	A_71E	0x08		/* 7 data, 1 stop, parity even		*/
#define	A_71O	0x0C		/* 7 data, 1 stop, parity odd		*/
#define	A_82N	0x10		/* 8 data, 2 stop, no parity		*/
#define	A_81N	0x14		/* 8 data, 1 stop, no parity		*/
#define	A_81E	0x18		/* 8 data, 1 stop, parity even		*/
#define	A_81O	0x1C		/* 8 data, 1 stop, parity odd		*/
/*		0x60		*//* RTS Low/High, TXINT en/dis, BREAK	*/
#define	A_TXPOL	0x00		/* RTS Low, TXINT disabled		*/
#define	A_TXINT	0x20		/* RTS Low, TXINT enabled		*/
#define	A_TXOFF	0x40		/* RTS High, TXINT disabled		*/
#define	A_BREAK	0x60		/* RTS Low, TXINT disabled, BREAK	*/
#define	A_RXINT	0x80		/* enable receiver interrupt		*/

/* bits in status register: */
#define	A_RXRDY	0x01		/* receiver ready			*/
#define	A_TXRDY	0x02		/* transmitter ready			*/
#define	A_CLOST	0x04		/* Carrier Lost				*/
#define	A_CTS	0x08		/* Clear To Send			*/
#define	A_FE	0x10		/* Frame Error				*/
#define	A_OE	0x20		/* Overrun Error			*/
#define	A_PE	0x40		/* Parity Error				*/
#define	A_IRQ	0x80		/* State of IRQ signal			*/

/* values for the TT: */
#define	KBD_INIT	(A_81N|A_Q64)
#define	MDI_INIT	(A_81N|A_Q16)

#endif /* _MACHINE_ACIA_H */
