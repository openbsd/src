/*	$OpenBSD: if_aereg.h,v 1.4 1998/01/07 00:33:46 niklas Exp $	*/
/*	$NetBSD: if_aereg.h,v 1.2 1995/08/18 15:53:32 chopps Exp $	*/

/*
 * Copyright (c) 1995 Bernd Ernesti and Klaus Burkert. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California. All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bernd Ernesti, by Klaus
 *	Burkert, by Michael van Elst, and by the University of California,
 *	Berkeley and its contributors.
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
 *	@(#)if_lereg.h	8.1 (Berkeley) 6/10/93
 *
 *	This is based on the original LANCE files, as the PCnet-ISA used on
 *	the Ariadne is a LANCE-descendant optimized for the PC-ISA bus.
 *	This causes some modifications, all data that is to go into registers
 *	or to structures (buffer-descriptors, init-block) has to be
 *	byte-swapped. In addition ALL write accesses to the board have to be
 *	WORD or LONG, BYTE-access is prohibited!!
 */

#define	ETHER_MAX_LEN	1518
#define	ETHER_MIN_LEN	64

#define AERBUF		16	/* 16 receive buffers */
#define AERBUFLOG2	4
#define AETBUF		4	/* 4 transmit buffers */
#define AETBUFLOG2	2

#define	AE_RLEN		(AERBUFLOG2 << 13)
#define	AE_TLEN		(AETBUFLOG2 << 13)

/*
 * PCnet-ISA registers.
 */

struct aereg1 {
	u_int16_t aer1_rdp;	/* data port */
	u_int16_t aer1_rap;	/* register select port */
	u_int16_t aer1_reset;	/* reading this resets the PCnet-ISA */
	u_int16_t aer1_idp;	/* isa configuration port */
};

struct aereg2 {
	/* init block */
	u_int16_t aer2_mode;		/* +0x0000 */
	u_int16_t aer2_padr[3];		/* +0x0002 */
	u_int16_t aer2_ladrf[4];	/* +0x0008 */
	u_int16_t aer2_rdra;		/* +0x0010 */
	u_int16_t aer2_rlen;		/* +0x0012 */
	u_int16_t aer2_tdra;		/* +0x0014 */
	u_int16_t aer2_tlen;		/* +0x0016 */
	/* receive message descriptors */
	struct	aermd {			/* +0x0018 */
		u_int16_t rmd0;
		u_int16_t rmd1;
		int16_t   rmd2;
		u_int16_t rmd3;
	} aer2_rmd[AERBUF];
	/* transmit message descriptors */
	struct	aetmd {			/* +0x0058 */
		u_int16_t tmd0;
		u_int16_t tmd1;
		int16_t   tmd2;
		u_int16_t tmd3;
	} aer2_tmd[AETBUF];
	char	aer2_rbuf[AERBUF][ETHER_MAX_LEN]; /* +0x0060 */
	char	aer2_tbuf[AETBUF][ETHER_MAX_LEN]; /* +0x2FD0 */
};


/*
 * Control and status bits -- aereg1
 */
#define	AE_CSR0		0x0000	/* Control and status register */
#define	AE_CSR1		0x0100	/* low address of init block */
#define	AE_CSR2		0x0200	/* high address of init block */
#define	AE_CSR3		0x0300	/* Bus master and control */

/* Control and status register 0 (csr0) */
#define	AE_SERR		0x0080	/* error summary */
#define	AE_BABL		0x0040	/* transmitter timeout error */
#define	AE_CERR		0x0020	/* collision */
#define	AE_MISS		0x0010	/* missed a packet */
#define	AE_MERR		0x0008	/* memory error */
#define	AE_RINT		0x0004	/* receiver interrupt */
#define	AE_TINT		0x0002	/* transmitter interrupt */
#define	AE_IDON		0x0001	/* initalization done */
#define	AE_INTR		0x8000	/* interrupt condition */
#define	AE_INEA		0x4000	/* interrupt enable */
#define	AE_RXON		0x2000	/* receiver on */
#define	AE_TXON		0x1000	/* transmitter on */
#define	AE_TDMD		0x0800	/* transmit demand */
#define	AE_STOP		0x0400	/* disable all external activity */
#define	AE_STRT		0x0200	/* enable external activity */
#define	AE_INIT		0x0100	/* begin initalization */

#define AE_PROM		0x0080	/* promiscuous mode */
#define AE_MODE		0x0000

/*
 * Control and status bits -- aereg2
 */
#define	AE_OWN		0x0080	/* LANCE owns the packet */
#define	AE_ERR		0x0040	/* error summary */
#define	AE_STP		0x0002	/* start of packet */
#define	AE_ENP		0x0001	/* end of packet */

#define	AE_MORE		0x0010	/* multiple collisions */
#define	AE_ONE		0x0008	/* single collision */
#define	AE_DEF		0x0004	/* defferred transmit */

#define	AE_FRAM		0x0020	/* framing error */
#define	AE_OFLO		0x0010	/* overflow error */
#define	AE_CRC		0x0008	/* CRC error */
#define	AE_RBUFF	0x0004	/* buffer error */

/* Transmit message descriptor 3 (tmd3) */
#define	AE_TBUFF	0x0080	/* buffer error */
#define	AE_UFLO		0x0040	/* underflow error */
#define	AE_LCOL		0x0010	/* late collision */
#define	AE_LCAR		0x0008	/* loss of carrier */
#define	AE_RTRY		0x0004	/* retry error */
#define	AE_TDR_MASK	0xff03	/* time domain reflectometry counter */

#define SWAP(x) swap16(x)

