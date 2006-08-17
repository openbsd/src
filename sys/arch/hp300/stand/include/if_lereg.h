/*	$OpenBSD: if_lereg.h,v 1.3 2006/08/17 06:31:10 miod Exp $	*/
/*	$NetBSD: if_lereg.h,v 1.1 1996/01/01 18:10:56 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1990 The Regents of the University of California.
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
 *	@(#)if_lereg.h	7.1 (Berkeley) 5/8/90
 */

#define	LEID		21

#define	NTBUF	2
#define	TLEN	1
#define	NRBUF	8
#define	RLEN	3
#define	BUFSIZE	1518

#define vu_char		volatile u_char
#define vu_short	volatile u_short

/*
 * LANCE registers.
 */
struct lereg0 {
	u_char	ler0_pad0;
	vu_char	ler0_id;	/* ID */
	u_char	ler0_pad1;
	vu_char	ler0_status;	/* interrupt enable/status */
};

/*
 * Control and status bits -- lereg0
 */
#define	LE_IE		0x80		/* interrupt enable */
#define	LE_IR		0x40		/* interrupt requested */
#define	LE_LOCK		0x08		/* lock status register */
#define	LE_ACK		0x04		/* ack of lock */
#define	LE_JAB		0x02		/* loss of tx clock (???) */
#define LE_IPL(x)	((((x) >> 4) & 0x3) + 3)

struct lereg1 {
	vu_short	ler1_rdp;	/* data port */
	vu_short	ler1_rap;	/* register select port */
};

/*
 * Control and status bits -- lereg1
 */
#define	LE_SERR		0x8000
#define	LE_BABL		0x4000
#define	LE_CERR		0x2000
#define	LE_MISS		0x1000
#define	LE_MERR		0x0800
#define	LE_RINT		0x0400
#define	LE_TINT		0x0200
#define	LE_IDON		0x0100
#define	LE_INTR		0x0080
#define	LE_INEA		0x0040
#define	LE_RXON		0x0020
#define	LE_TXON		0x0010
#define	LE_TDMD		0x0008
#define	LE_STOP		0x0004
#define	LE_STRT		0x0002
#define	LE_INIT		0x0001

#define	LE_BSWP		0x0004
#define	LE_ACON		0x0002
#define	LE_BCON		0x0001

/*
 * Overlayed on 16K dual-port RAM.
 * Current size is 15,284 bytes with 8 x 1518 receive buffers and
 * 2 x 1518 transmit buffers.
 */

/*
 * LANCE initialization block
 */
struct init_block {
	u_short mode;		/* mode register */
	u_char padr[6];		/* ethernet address */
	u_long ladrf[2];	/* logical address filter (multicast) */
        u_short rdra;           /* low order pointer to receive ring */
        u_short rlen;           /* high order pointer and no. rings */
        u_short tdra;           /* low order pointer to transmit ring */
        u_short tlen;           /* high order pointer and no rings */
};

/*
 * Mode bits -- init_block
 */
#define	LE_PROM		0x8000		/* promiscuous */
#define	LE_INTL		0x0040		/* internal loopback */
#define	LE_DRTY		0x0020		/* disable retry */
#define	LE_COLL		0x0010		/* force collision */
#define	LE_DTCR		0x0008		/* disable transmit crc */
#define	LE_LOOP		0x0004		/* loopback */
#define	LE_DTX		0x0002		/* disable transmitter */
#define	LE_DRX		0x0001		/* disable receiver */
#define	LE_NORMAL	0x0000

/*
 * Message descriptor
 */
struct mds {
	u_short addr;
	u_short flags;
	u_short bcnt;
	u_short mcnt;
};

/* Message descriptor flags */
#define LE_OWN		0x8000		/* owner bit, 0=host, 1=LANCE */
#define LE_ERR		0x4000		/* error */
#define	LE_STP		0x0200		/* start of packet */
#define	LE_ENP		0x0100		/* end of packet */

/* Receive ring status flags */
#define LE_FRAM		0x2000		/* framing error error */
#define LE_OFLO		0x1000		/* silo overflow */
#define LE_CRC		0x0800		/* CRC error */
#define LE_RBUFF	0x0400		/* buffer error */

/* Transmit ring status flags */
#define LE_MORE		0x1000		/* more than 1 retry */
#define LE_ONE		0x0800		/* one retry */
#define LE_DEF		0x0400		/* deferred transmit */

/* Transmit errors */
#define LE_TBUFF	0x8000		/* buffer error */
#define LE_UFLO		0x4000		/* silo underflow */
#define LE_LCOL		0x1000		/* late collision */
#define LE_LCAR		0x0800		/* loss of carrier */
#define LE_RTRY		0x0400		/* tried 16 times */
