/*	$NetBSD: if_lereg.h,v 1.10 1995/01/24 05:50:52 gwr Exp $	*/

/*
 * LANCE Ethernet driver header file
 *
 * Copyright (c) 1995 Gordon W. Ross
 * Copyright (c) 1994 Charles Hannum.
 *
 * Copyright (C) 1993, Paul Richards. This software may be used, modified,
 *   copied, distributed, and sold, in both source and binary form provided
 *   that the above copyright and these terms are retained. Under no
 *   circumstances is the author responsible for the proper functioning
 *   of this software, nor does the author assume any responsibility
 *   for damages incurred with its use.
 */

/* Declarations specific to this driver */
#define NTBUF	2
#define TLEN	1
#define NRBUF	8
#define RLEN	3
#define BUFSIZE	1536

#define MEMSIZE 0x4000

/* Local Area Network Controller for Ethernet (LANCE) registers */
struct le_regs {
	u_short	lereg_data;	/* data port */
	u_short	lereg_addr;	/* address port */
};

/*
 * Control and status bits
 */
#define	LE_SERR		0x8000
#define	LE_BABL		0x4000
#define LE_CERR		0x2000
#define LE_MISS		0x1000
#define LE_MERR		0x0800
#define LE_RINT		0x0400
#define LE_TINT		0x0200
#define LE_IDON		0x0100
#define LE_INTR		0x0080
#define LE_INEA		0x0040
#define LE_RXON		0x0020
#define LE_TXON		0x0010
#define LE_TDMD		0x0008
#define LE_STOP		0x0004
#define LE_STRT		0x0002
#define LE_INIT		0x0001

#define LE_BSWP		0x0004
#define LE_ACON		0x0002
#define LE_BCON		0x0001

/*
 * LANCE initialization block
 */
struct init_block {
	u_short mode;		/* mode register */
	u_char padr[6];		/* ethernet address */
	u_long ladrf[2];	/* logical address filter (multicast) */
	u_short rdra;		/* low order pointer to receive ring */
	u_short rlen;		/* high order pointer and no. rings */
	u_short tdra;		/* low order pointer to transmit ring */
	u_short tlen;		/* high order pointer and no rings */
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


/* DEPCA-specific definitions */
#define	DEPCA_CSR	0x0
#define	DEPCA_CSR_SHE		0x80	/* Shared memory enabled */
#define	DEPCA_CSR_SWAP32	0x40	/* Byte swapped */
#define	DEPCA_CSR_DUM		0x08	/* rev E compatibility */
#define	DEPCA_CSR_IM		0x04	/* Interrupt masked */
#define	DEPCA_CSR_IEN		0x02	/* Interrupt enabled */
#define	DEPCA_CSR_NORMAL \
	(DEPCA_CSR_SHE | DEPCA_CSR_DUM | DEPCA_CSR_IEN)

#define	DEPCA_ADP	0xc
