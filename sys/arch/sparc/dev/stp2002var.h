/*	$OpenBSD: stp2002var.h,v 1.1 1998/07/17 21:33:11 jason Exp $	*/

/*
 * Copyright (c) 1998 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

struct stp_rxd {
	volatile u_int32_t	rx_flags;
	volatile u_int32_t	rx_addr;
};
#define	STP_RXFLAG_OWN		0x80000000	/* desc owner: 1=hw,0=sw */
#define STP_RXFLAG_OVERFLOW	0x40000000	/* 1 = buffer over flow */
#define STP_RXFLAG_SIZE		0x3fff0000	/* desciptor size */
#define STP_RXFLAG_CSUM		0x0000ffff	/* checksum mask */

struct stp_txd {
	volatile u_int32_t	tx_flags;
	volatile u_int32_t	tx_addr;
};
#define	STP_TXFLAG_OWN		0x80000000	/* desc owner: 1=hw,0=sw */
#define	STP_TXFLAG_SOP		0x40000000 	/* 1 = start of pkt */
#define	STP_TXFLAG_EOP		0x20000000	/* 1 = end of pkt */
#define	STP_TXFLAG_CSENABLE	0x10000000	/* 1 = use hw checksums */
#define	STP_TXFLAG_CSLOCATION	0x0ff00000	/* checksum location mask */
#define	STP_TXFLAG_CSBUFBEGIN	0x000fc000	/* checksum begin mask */
#define	STP_TXFLAG_SIZE		0x00003fff 	/* pkt size mask */

#define STP_RX_RING_SIZE	32	/* Must be 32, 64, 128, or 256 */
#define STP_TX_RING_SIZE	32	/* 16<=x<=256 and divisible by 16 */
#define STP_RX_RING_MAX		256	/* maximum ring size: rx */
#define STP_TX_RING_MAX		256	/* maximum ring size: tx */
#define STP_RX_PKT_BUF_SZ	2048	/* size of a rx buffer */
#define STP_RX_OFFSET		2	/* packet offset */
#define STP_RX_CSUMLOC		0x00	/* checksum location */
#define STP_TX_PKT_BUF_SZ	1546	/* size of a tx buffer */
#define STP_RX_ALIGN_SIZE	64	/* rx buffers must align 64 XXX */
#define STP_RX_ALIGN_MASK	(~(RX_ALIGN_SIZE - 1))

struct stp_desc {
	struct stp_rxd stp_rxd[STP_RX_RING_MAX];
	struct stp_txd stp_txd[STP_TX_RING_MAX];
};

struct stp_bufs {
	char rx_buf[STP_RX_RING_SIZE][STP_RX_PKT_BUF_SZ];
	char tx_buf[STP_TX_RING_SIZE][STP_TX_PKT_BUF_SZ];
};

struct stp_base {
	/* public members */
	struct	arpcom stp_arpcom;			/* ethernet common */
	u_long	stp_rx_dvma, stp_tx_dvma;		/* ring dva pointers */
        void    (*stp_tx_dmawakeup) __P((void *));	/* func to start tx */

	/* internal use */
	struct	stp_desc *stp_desc, *stp_desc_dva;	/* ring descriptors */
	struct	stp_bufs *stp_bufs, *stp_bufs_dva;	/* packet buffers */
	int	stp_first_td, stp_last_td, stp_no_td;	/* tx counters */
	int	stp_last_rd;				/* rx counters */
};

void		stp2002_meminit	__P((struct stp_base *));
int		stp2002_rint	__P((struct stp_base *));
int		stp2002_tint	__P((struct stp_base *));
void		stp2002_start	__P((struct stp_base *));
