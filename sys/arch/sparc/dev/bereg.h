/*	$OpenBSD: bereg.h,v 1.6 1998/09/04 05:59:19 jason Exp $	*/

/*
 * Copyright (c) 1998 Theo de Raadt and Jason L. Wright.
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
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * BE Global registers
 */
struct be_bregs {
	volatile u_int32_t xif_cfg;		/* XIF config */
	volatile u_int32_t _unused[63];		/* reserved */
	volatile u_int32_t stat;		/* status, clear on read */
	volatile u_int32_t imask;		/* interrupt mask */
	volatile u_int32_t _unused2[64];	/* reserved */
	volatile u_int32_t tx_swreset;		/* tx software reset */
	volatile u_int32_t tx_cfg;		/* tx config */
	volatile u_int32_t ipkt_gap1;		/* inter-packet gap 1 */
	volatile u_int32_t ipkt_gap2;		/* inter-packet gap 2 */
	volatile u_int32_t attempt_limit;	/* tx attempt limit */
	volatile u_int32_t stime;		/* tx slot time */
	volatile u_int32_t preamble_len;	/* size of tx preamble */
	volatile u_int32_t preamble_pattern;	/* pattern for tx preamble */
	volatile u_int32_t tx_sframe_delim;	/* tx delimiter */
	volatile u_int32_t jsize;		/* jam length */
	volatile u_int32_t tx_pkt_max;		/* tx max pkt size */
	volatile u_int32_t tx_pkt_min;		/* tx min pkt size */
	volatile u_int32_t peak_attempt;	/* count of tx peak attempts */
	volatile u_int32_t dt_ctr;		/* tx defer timer */
	volatile u_int32_t nc_ctr;		/* tx normal collision cntr */
	volatile u_int32_t fc_ctr;		/* tx first-collision cntr */
	volatile u_int32_t ex_ctr;		/* tx excess-collision cntr */
	volatile u_int32_t lt_ctr;		/* tx late-collision cntr */
	volatile u_int32_t rand_seed;		/* tx random number seed */
	volatile u_int32_t tx_smachine;		/* tx state machine */
	volatile u_int32_t _unused3[44];	/* reserved */
	volatile u_int32_t rx_swreset;		/* rx software reset */
	volatile u_int32_t rx_cfg;		/* rx config register */
	volatile u_int32_t rx_pkt_max;		/* rx max pkt size */
	volatile u_int32_t rx_pkt_min;		/* rx min pkt size */
	volatile u_int32_t mac_addr2;		/* ethernet address 2 (MSB) */
	volatile u_int32_t mac_addr1;		/* ethernet address 1 */
	volatile u_int32_t mac_addr0;		/* ethernet address 0 (LSB) */
	volatile u_int32_t fr_ctr;		/* rx frame receive cntr */
	volatile u_int32_t gle_ctr;		/* rx giant-len error cntr */
	volatile u_int32_t unale_ctr;		/* rx unaligned error cntr */
	volatile u_int32_t rcrce_ctr;		/* rx CRC error cntr */
	volatile u_int32_t rx_smachine;		/* rx state machine */
	volatile u_int32_t rx_cvalid;		/* rx code violation */
	volatile u_int32_t _unused4;		/* reserved */
	volatile u_int32_t htable3;		/* hash table 3 */
	volatile u_int32_t htable2;		/* hash table 2 */
	volatile u_int32_t htable1;		/* hash table 1 */
	volatile u_int32_t htable0;		/* hash table 0 */
	volatile u_int32_t afilter2;		/* address filter 2 */
	volatile u_int32_t afilter1;		/* address filter 1 */
	volatile u_int32_t afilter0;		/* address filter 0 */
	volatile u_int32_t afilter_mask;	/* address filter mask */
};

/* be_bregs.xif_cfg: XIF config. */
#define BE_BR_XCFG_ODENABLE	0x00000001	/* output driver enable */
#define BE_BR_XCFG_RESV		0x00000002	/* reserved, write as 1 */
#define BE_BR_XCFG_MLBACK	0x00000004	/* loopback-mode mii enable */
#define BE_BR_XCFG_SMODE	0x00000008	/* enable serial mode */

/* be_bregs.stat: status, clear on read. */
#define BE_BR_STAT_GOTFRAME	0x00000001	/* received a frame */
#define BE_BR_STAT_RCNTEXP	0x00000002	/* rx frame cntr expired */
#define BE_BR_STAT_ACNTEXP	0x00000004	/* align-error cntr expired */
#define BE_BR_STAT_CCNTEXP	0x00000008	/* crc-error cntr expired */
#define BE_BR_STAT_LCNTEXP	0x00000010	/* length-error cntr expired */
#define BE_BR_STAT_RFIFOVF	0x00000020	/* rx fifo overflow */
#define BE_BR_STAT_CVCNTEXP	0x00000040	/* code-violation cntr exprd */
#define BE_BR_STAT_SENTFRAME	0x00000100	/* transmitted a frame */
#define BE_BR_STAT_TFIFO_UND	0x00000200	/* tx fifo underrun */
#define BE_BR_STAT_MAXPKTERR	0x00000400	/* max-packet size error */
#define BE_BR_STAT_NCNTEXP	0x00000800	/* normal-collision cntr exp */
#define BE_BR_STAT_ECNTEXP	0x00001000	/* excess-collision cntr exp */
#define BE_BR_STAT_LCCNTEXP	0x00002000	/* late-collision cntr exp */
#define BE_BR_STAT_FCNTEXP	0x00004000	/* first-collision cntr exp */
#define BE_BR_STAT_DTIMEXP	0x00008000	/* defer-timer expired */

/* be_bregs.imask: interrupt mask. */
#define BE_BR_IMASK_GOTFRAME	0x00000001	/* received a frame */
#define BE_BR_IMASK_RCNTEXP	0x00000002	/* rx frame cntr expired */
#define BE_BR_IMASK_ACNTEXP	0x00000004	/* align-error cntr expired */
#define BE_BR_IMASK_CCNTEXP	0x00000008	/* crc-error cntr expired */
#define BE_BR_IMASK_LCNTEXP	0x00000010	/* length-error cntr expired */
#define BE_BR_IMASK_RFIFOVF	0x00000020	/* rx fifo overflow */
#define BE_BR_IMASK_CVCNTEXP	0x00000040	/* code-violation cntr exprd */
#define BE_BR_IMASK_SENTFRAME	0x00000100	/* transmitted a frame */
#define BE_BR_IMASK_TFIFO_UND	0x00000200	/* tx fifo underrun */
#define BE_BR_IMASK_MAXPKTERR	0x00000400	/* max-packet size error */
#define BE_BR_IMASK_NCNTEXP	0x00000800	/* normal-collision cntr exp */
#define BE_BR_IMASK_ECNTEXP	0x00001000	/* excess-collision cntr exp */
#define BE_BR_IMASK_LCCNTEXP	0x00002000	/* late-collision cntr exp */
#define BE_BR_IMASK_FCNTEXP	0x00004000	/* first-collision cntr exp */
#define BE_BR_IMASK_DTIMEXP	0x00008000	/* defer-timer expired */

/* be_bregs.tx_cfg: tx config. */
#define BE_BR_TXCFG_ENABLE	0x00000001	/* enable the transmitter */
#define BE_BR_TXCFG_FIFO	0x00000010	/* default tx fthresh */
#define BE_BR_TXCFG_SMODE	0x00000020	/* enable slow transmit mode */
#define BE_BR_TXCFG_CIGN	0x00000040	/* ignore tx collisions */
#define BE_BR_TXCFG_FCSOFF	0x00000080	/* do not emit fcs */
#define BE_BR_TXCFG_DBACKOFF	0x00000100	/* disable backoff */
#define BE_BR_TXCFG_FULLDPLX	0x00000200	/* enable full-duplex */

/* be_bregs.rx_cfg: rx config. */
#define BE_BR_RXCFG_ENABLE	0x00000001	/* enable the receiver */
#define BE_BR_RXCFG_FIFO	0x0000000e	/* default rx fthresh */
#define BE_BR_RXCFG_PSTRIP	0x00000020	/* pad byte strip enable */
#define BE_BR_RXCFG_PMISC	0x00000040	/* enable promiscous mode */
#define BE_BR_RXCFG_DERR	0x00000080	/* disable error checking */
#define BE_BR_RXCFG_DCRCS	0x00000100	/* disable crc stripping */
#define BE_BR_RXCFG_ME		0x00000200	/* receive packets for me */
#define BE_BR_RXCFG_PGRP	0x00000400	/* enable promisc group mode */
#define BE_BR_RXCFG_HENABLE	0x00000800	/* enable hash filter */
#define BE_BR_RXCFG_AENABLE	0x00001000	/* enable address filter */

/*
 * BE Channel registers
 */
struct be_cregs {
	volatile u_int32_t ctrl;		/* control */
	volatile u_int32_t stat;		/* status */
	volatile u_int32_t rxds;		/* rx descriptor ring ptr */
	volatile u_int32_t txds;		/* tx descriptor ring ptr */
	volatile u_int32_t rimask;		/* rx interrupt mask */
	volatile u_int32_t timask;		/* tx interrupt mask */
	volatile u_int32_t qmask;		/* qec error interrupt mask */
	volatile u_int32_t bmask;		/* be error interrupt mask */
	volatile u_int32_t rxwbufptr;		/* local memory rx write ptr */
	volatile u_int32_t rxrbufptr;		/* local memory rx read ptr */
	volatile u_int32_t txwbufptr;		/* local memory tx write ptr */
	volatile u_int32_t txrbufptr;		/* local memory tx read ptr */
	volatile u_int32_t ccnt;		/* collision counter */
};

/* be_cregs.ctrl: control. */
#define	BE_CR_CTRL_TWAKEUP	0x00000001	/* tx dma wakeup */

/* be_cregs.stat: status. */
#define BE_CR_STAT_BERROR	0x80000000	/* be error */
#define BE_CR_STAT_TXIRQ	0x00200000	/* tx interrupt */
#define BE_CR_STAT_TXDERR	0x00080000	/* tx descriptor is bad */
#define BE_CR_STAT_TXLERR	0x00040000	/* tx late error */
#define BE_CR_STAT_TXPERR	0x00020000	/* tx parity error */
#define BE_CR_STAT_TXSERR	0x00010000	/* tx sbus error ack */
#define BE_CR_STAT_RXIRQ	0x00000020	/* rx interrupt */
#define BE_CR_STAT_RXDROP	0x00000010	/* rx packet dropped */
#define BE_CR_STAT_RXSMALL	0x00000008	/* rx buffer too small */
#define BE_CR_STAT_RXLERR	0x00000004	/* rx late error */
#define BE_CR_STAT_RXPERR	0x00000002	/* rx parity error */
#define BE_CR_STAT_RXSERR	0x00000001	/* rx sbus error ack */

/* be_cregs.qmask: qec error interrupt mask. */
#define BE_CR_QMASK_TXDERR	0x00080000	/* tx descriptor is bad */
#define BE_CR_QMASK_TXLERR	0x00040000	/* tx late error */
#define BE_CR_QMASK_TXPERR	0x00020000	/* tx parity error */
#define BE_CR_QMASK_TXSERR	0x00010000	/* tx sbus error ack */
#define BE_CR_QMASK_RXDROP	0x00000010	/* rx packet dropped */
#define BE_CR_QMASK_RXSMALL	0x00000008	/* rx buffer too small */
#define BE_CR_QMASK_RXLERR	0x00000004	/* rx late error */
#define BE_CR_QMASK_RXPERR	0x00000002	/* rx parity error */
#define BE_CR_QMASK_RXSERR	0x00000001	/* rx sbus error ack */

/*
 * BE Tranceiver registers
 */
struct be_tregs {
	volatile u_int32_t	tcvr_pal;	/* tranceiver pal */
	volatile u_int32_t	mgmt_pal;	/* management pal */
};

/* be_tregs.tcvr_pal: tranceiver pal */
#define	TCVR_PAL_SERIAL		0x00000001	/* serial mode enable */
#define TCVR_PAL_EXTLBACK	0x00000002	/* external loopback */
#define TCVR_PAL_MSENSE		0x00000004	/* media sense */
#define TCVR_PAL_LTENABLE	0x00000008	/* link test enable */
#define TCVR_PAL_LTSTATUS	0x00000010	/* link test status: p1 only */

/* be_tregs.mgmt_pal: management pal */
#define MGMT_PAL_DCLOCK		0x00000001	/* data clock strobe */
#define MGMT_PAL_OENAB		0x00000002	/* output enable */
#define MGMT_PAL_MDIO		0x00000004	/* MDIO data/attached */
#define MGMT_PAL_EXT_MDIO	MGMT_PAL_MDIO	/* external mdio */
#define MGMT_PAL_TIMEO		0x00000008	/* tx enable timeout error */
#define MGMT_PAL_INT_MDIO	MGMT_PAL_TIMEO	/* internal mdio */

/*
 * BE receive descriptor
 */
struct be_rxd {
	volatile u_int32_t rx_flags;		/* rx descriptor flags */
	volatile u_int32_t rx_addr;		/* rx buffer address */
};

#define BE_RXD_OWN		0x80000000	/* ownership: 1=hw, 0=sw */
#define BE_RXD_UPDATE		0x10000000	/* being updated? */
#define BE_RXD_LENGTH		0x00001fff	/* packet length */

/*
 * BE transmit descriptor
 */
struct be_txd {
	volatile u_int32_t tx_flags;		/* tx descriptor flags */
	volatile u_int32_t tx_addr;		/* tx buffer address */
};

#define BE_TXD_OWN		0x80000000	/* ownership: 1=hw, 0=sw */
#define BE_TXD_SOP		0x40000000	/* start of packet marker */
#define BE_TXD_EOP		0x20000000	/* end of packet marker */
#define BE_TXD_UPDATE		0x10000000	/* being updated? */
#define BE_TXD_LENGTH		0x00001fff	/* packet length */

/* Buffer and Ring sizes: fixed ring size */
#define BE_TX_RING_MAXSIZE	256		/* maximum tx ring size */
#define BE_RX_RING_MAXSIZE	256		/* maximum rx ring size */
#define BE_TX_RING_SIZE		32
#define BE_RX_RING_SIZE		32
#define BE_PKT_BUF_SZ		2048

/*
 * BE descriptor rings
 */
struct be_desc {
	struct be_rxd be_rxd[BE_RX_RING_MAXSIZE];
	struct be_txd be_txd[BE_TX_RING_MAXSIZE];
};

/*
 * BE packet buffers
 */
struct be_bufs {
	char	rx_buf[BE_RX_RING_SIZE][BE_PKT_BUF_SZ];
	char	tx_buf[BE_TX_RING_SIZE][BE_PKT_BUF_SZ];
};

/* PHY addresses */
#define BE_PHY_EXTERNAL		0
#define BE_PHY_INTERNAL		1

/* Tranceiver types */
#define BE_TCVR_NONE		0
#define BE_TCVR_INTERNAL	1
#define BE_TCVR_EXTERNAL	2

#define BE_TCVR_READ_INVALID	0xff000000

#define BE_NEGOTIATE_MAXTICKS	16

#define PHY_BMCR		0x00	/* Basic Mode Control Register */
#define PHY_BMSR		0x01	/* Basic Mode Status Register */

/*
 * Basic Mode Control Register (BMCR)
 */
#define PHY_BMCR_RESET		0x8000	/* Software reset		*/
#define PHY_BMCR_LOOPBACK	0x4000	/* Lookback enable		*/
#define PHY_BMCR_SPEED		0x2000	/* 1=100Mb, 0=10Mb		*/
#define PHY_BMCR_ANE		0x1000	/* Auto-Negiation enable	*/
#define PHY_BMCR_PDOWN		0x0800	/* power down the chip		*/
#define PHY_BMCR_ISOLATE	0x0400	/* Isolate the chip		*/
#define PHY_BMCR_RAN		0x0200	/* Restart autonegotiation	*/
#define PHY_BMCR_DUPLEX		0x0100	/* 1=full, 0=half		*/
#define PHY_BMCR_COLLISONTEST	0x0080	/* Create collisions on TX	*/

/*
 * Basic Mode Status Register (BMSR)
 */
#define PHY_BMSR_100BASET4	0x8000	/* 100BaseT4 capable?		*/
#define PHY_BMSR_100BASETX_FULL	0x4000	/* 100BaseTX full duplex cap?	*/
#define PHY_BMSR_100BASETX_HALF	0x2000	/* 100BaseTX half duplex cap?	*/
#define PHY_BMSR_10BASET_FULL	0x1000	/* 10BaseT full duplex cap?	*/
#define PHY_BMSR_10BASET_HALF	0x0800	/* 10BaseT half duplex cap?	*/
#define PHY_BMSR_ANCOMPLETE	0x0020	/* auto-negotiation complete?	*/
#define PHY_BMSR_REMOTEFAULT	0x0010	/* Fault condition seen?	*/
#define PHY_BMSR_ANC		0x0008	/* Can auto-negotiate?		*/
#define PHY_BMSR_LINKSTATUS	0x0004	/* Link established?		*/
#define PHY_BMSR_JABBER		0x0002	/* Jabber detected?		*/
#define PHY_BMSR_EXTENDED	0x0001	/* Extended registers?		*/
