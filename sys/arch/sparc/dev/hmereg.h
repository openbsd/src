/*	$OpenBSD: hmereg.h,v 1.12 2005/02/22 20:44:26 brad Exp $	*/

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

#define HME_DEFAULT_JSIZE	4
#define HME_DEFAULT_IPKT_GAP0	16
#define HME_DEFAULT_IPKT_GAP1	8
#define HME_DEFAULT_IPKT_GAP2	4

/* global registers */
struct hme_gr {
	volatile u_int32_t	reset;		/* reset tx/rx */
	volatile u_int32_t	cfg;		/* config */
	volatile u_int32_t	_padding[62];	/* unused */
	volatile u_int32_t	stat;		/* intr status */
	volatile u_int32_t	imask;		/* intr mask */
};

/* hme_gr.reset (software reset register) */
#define GR_RESET_ETX		0x01	/* reset external tx */
#define GR_RESET_ERX		0x02	/* reset external rx */
#define GR_RESET_ALL		(GR_RESET_ETX | GR_RESET_ERX)

/* hme_gr.cfg (configuration register) */
#define GR_CFG_BURSTMSK		0x03	/* burst mask */
#define GR_CFG_BURST16		0x00	/* 16 byte bursts */
#define GR_CFG_BURST32		0x01	/* 32 byte bursts */
#define GR_CFG_BURST64		0x02	/* 32 byte bursts */
#define GR_CFG_64BIT		0x04
#define GR_CFG_PARITY		0x08
#define GR_CFG_RESV		0x10

/* hme_gr.stat (interrupt status register) */
#define GR_STAT_GOTFRAME	0x00000001 /* frame received */
#define GR_STAT_RCNTEXP		0x00000002 /* rx frame count expired */
#define GR_STAT_ACNTEXP		0x00000004 /* align error count expired */
#define GR_STAT_CCNTEXP		0x00000008 /* crc error count expired */
#define GR_STAT_LCNTEXP		0x00000010 /* length error count expired */
#define GR_STAT_RFIFOVF		0x00000020 /* rx fifo overflow */
#define GR_STAT_CVCNTEXP	0x00000040 /* code violation counter expired */
#define GR_STAT_STSTERR		0x00000080 /* xif sqe test failed */
#define GR_STAT_SENTFRAME	0x00000100 /* frame sent */
#define GR_STAT_TFIFO_UND	0x00000200 /* tx fifo underrun */
#define GR_STAT_MAXPKTERR	0x00000400 /* max-packet size error */
#define GR_STAT_NCNTEXP		0x00000800 /* normal collision count expired */
#define GR_STAT_ECNTEXP		0x00001000 /* excess collision count expired */
#define GR_STAT_LCCNTEXP	0x00002000 /* late collision count expired */
#define GR_STAT_FCNTEXP		0x00004000 /* first collision count expired */
#define GR_STAT_DTIMEXP		0x00008000 /* defer timer expired */
#define GR_STAT_RXTOHOST	0x00010000 /* pkt moved from rx fifo->memory */
#define GR_STAT_NORXD		0x00020000 /* out of receive descriptors */
#define GR_STAT_RXERR		0x00040000 /* rx dma error */
#define GR_STAT_RXLATERR	0x00080000 /* late error during rx dma */
#define GR_STAT_RXPERR		0x00100000 /* parity error during rx dma */
#define GR_STAT_RXTERR		0x00200000 /* tag error during rx dma */
#define GR_STAT_EOPERR		0x00400000 /* tx descriptor did not set EOP */
#define GR_STAT_MIFIRQ		0x00800000 /* mif needs attention */
#define GR_STAT_HOSTTOTX	0x01000000 /* pkt moved from memory->tx fifo */
#define GR_STAT_TXALL		0x02000000 /* all pkts in fifo transmitted */
#define GR_STAT_TXEACK		0x04000000 /* error during tx dma */
#define GR_STAT_TXLERR		0x08000000 /* late error during tx dma */
#define GR_STAT_TXPERR		0x10000000 /* parity error during tx dma */
#define GR_STAT_TXTERR		0x20000000 /* tag error durig tx dma */
#define GR_STAT_SLVERR		0x40000000 /* pio access error */
#define GR_STAT_SLVPERR		0x80000000 /* pio access parity error */

/* all the errors to worry about */
#define GR_STAT_ALL_ERRORS	\
	(GR_STAT_SLVPERR   | GR_STAT_SLVERR  | GR_STAT_TXTERR    | \
	 GR_STAT_TXPERR    | GR_STAT_TXLERR  | GR_STAT_TXEACK    | \
	 GR_STAT_EOPERR    | GR_STAT_RXTERR  | GR_STAT_RXPERR    | \
	 GR_STAT_RXLATERR  | GR_STAT_RXERR   | GR_STAT_NORXD     | \
	 GR_STAT_DTIMEXP   | GR_STAT_FCNTEXP | GR_STAT_LCCNTEXP  | \
	 GR_STAT_ECNTEXP   | GR_STAT_NCNTEXP | GR_STAT_MAXPKTERR | \
	 GR_STAT_TFIFO_UND | GR_STAT_STSTERR | GR_STAT_CVCNTEXP  | \
	 GR_STAT_RFIFOVF   | GR_STAT_LCNTEXP | GR_STAT_CCNTEXP   | \
	 GR_STAT_ACNTEXP)

#define	GR_STAT_BITS	\
	"\20\1RX\2RCNT\3ACNT\4CCNT\5LCNT\6RFIFO\7CVCNT\10STST" \
	"\11TX\12TFIFO\13MAXPKT\14NCNT\15ECNT\16LCCNT\17FCNT" \
	"\20DTIME\21RXHOST\22NORXD\23RXE\24EXLATE\25RXP\26RXT\27EOP" \
	"\30MIF\31TXHOST\32TXALL\33TXE\34TXL\35TXP\36TXT\37SLV" \
	"\40SLVP"

/* hme_gr.stat (interrupt status register) */
#define GR_IMASK_GOTFRAME	0x00000001 /* frame received */
#define GR_IMASK_RCNTEXP	0x00000002 /* rx frame count expired */
#define GR_IMASK_ACNTEXP	0x00000004 /* align error count expired */
#define GR_IMASK_CCNTEXP	0x00000008 /* crc error count expired */
#define GR_IMASK_LCNTEXP	0x00000010 /* length error count expired */
#define GR_IMASK_RFIFOVF	0x00000020 /* rx fifo overflow */
#define GR_IMASK_CVCNTEXP	0x00000040 /* code violation count expired */
#define GR_IMASK_STSTERR	0x00000080 /* xif sqe test failed */
#define GR_IMASK_SENTFRAME	0x00000100 /* frame sent */
#define GR_IMASK_TFIFO_UND	0x00000200 /* tx fifo underrun */
#define GR_IMASK_MAXPKTERR	0x00000400 /* max-packet size error */
#define GR_IMASK_NCNTEXP	0x00000800 /* normal collision count expired */
#define GR_IMASK_ECNTEXP	0x00001000 /* excess collision count expired */
#define GR_IMASK_LCCNTEXP	0x00002000 /* late collision count expired */
#define GR_IMASK_FCNTEXP	0x00004000 /* first collision count expired */
#define GR_IMASK_DTIMEXP	0x00008000 /* defer timer expired */
#define GR_IMASK_RXTOHOST	0x00010000 /* pkt moved from rx fifo->memory */
#define GR_IMASK_NORXD		0x00020000 /* out of receive descriptors */
#define GR_IMASK_RXERR		0x00040000 /* rx dma error */
#define GR_IMASK_RXLATERR	0x00080000 /* late error during rx dma */
#define GR_IMASK_RXPERR		0x00100000 /* parity error during rx dma */
#define GR_IMASK_RXTERR		0x00200000 /* tag error during rx dma */
#define GR_IMASK_EOPERR		0x00400000 /* tx descriptor did not set EOP */
#define GR_IMASK_MIFIRQ		0x00800000 /* mif needs attention */
#define GR_IMASK_HOSTTOTX	0x01000000 /* pkt moved from memory->tx fifo */
#define GR_IMASK_TXALL		0x02000000 /* all pkts in fifo transmitted */
#define GR_IMASK_TXEACK		0x04000000 /* error during tx dma */
#define GR_IMASK_TXLERR		0x08000000 /* late error during tx dma */
#define GR_IMASK_TXPERR		0x10000000 /* parity error during tx dma */
#define GR_IMASK_TXTERR		0x20000000 /* tag error during tx dma */
#define GR_IMASK_SLVERR		0x40000000 /* pio access error */
#define GR_IMASK_SLVPERR	0x80000000 /* PIO access parity error */

/*
 * external transmitter registers
 */
struct hme_txr {
	volatile u_int32_t	tx_pnding;	/* tx pending/wakeup */
        volatile u_int32_t	cfg;		/* tx cfg */
	volatile u_int32_t	tx_ring;	/* tx ring ptr */
	volatile u_int32_t	tx_bbase;	/* tx buffer base */
	volatile u_int32_t	tx_bdisp;	/* tx buffer displacement */
	volatile u_int32_t	tx_fifo_wptr;	/* tx fifo write pointer */
	volatile u_int32_t	tx_fifo_swptr;	/* tx fifo write ptr (shadow) */
	volatile u_int32_t	tx_fifo_rptr; 	/* tx fifo read pointer */
	volatile u_int32_t	tx_fifo_srptr;	/* tx fifo read ptr (shadow) */
	volatile u_int32_t	tx_fifo_pcnt;	/* tx fifo packet counter */
	volatile u_int32_t	smachine;	/* tx state machine */
	volatile u_int32_t	tx_rsize;	/* tx ring size */
	volatile u_int32_t	tx_bptr;	/* tx buffer pointer */
};

/* hme_txr.tx_pnding (tx pending/wakeup) */
#define TXR_TP_DMAWAKEUP	0x00000001	/* Restart transmit dma */

/* hme_txr.tx_cfg (tx configuration) */
#define TXR_CFG_DMAENABLE	0x00000001 /* enable tx dma */
#define TXR_CFG_FIFOTHRESH	0x000003fe /* tx fifo threshold */
#define TXR_CFG_IRQDAFTER	0x00000400 /* intr after tx-fifo empty */
#define TXR_CFG_IRQDBEFORE	0x00000000 /* intr before tx-fifo empty */
#define TXR_RSIZE_SHIFT		4

/*
 * external receiver registers
 */
struct hme_rxr {
	volatile u_int32_t	cfg;		/* rx cfg */
	volatile u_int32_t	rx_ring;	/* rx ring pointer */
	volatile u_int32_t	rx_bptr;	/* rx buffer ptr */
	volatile u_int32_t	rx_fifo_wptr;	/* rx fifo write ptr */
	volatile u_int32_t	rx_fifo_swptr;	/* rx fifo write ptr (shadow) */
	volatile u_int32_t	rx_fifo_rptr;	/* rx fifo read ptr */
	volatile u_int32_t	rx_fifo_srptr;	/* rx fifo read ptr (shadow) */
	volatile u_int32_t	smachine;	/* rx state machine */
};

/* hme_rxr.rx_cfg (rx configuration) */
#define RXR_CFG_DMAENABLE	0x00000001	/* rx dma enable */
#define RXR_CFG_reserved1	0x00000006	/* reserved bits */
#define RXR_CFG_BYTEOFFSET	0x00000038	/* rx first byte offset */
#define RXR_CFG_reserved2	0x000001c0	/* reserved bits */
#define RXR_CFG_RINGSIZE32	0x00000000	/* rx descptr ring size: 32 */
#define RXR_CFG_RINGSIZE64	0x00000200	/* rx descptr ring size: 64 */
#define RXR_CFG_RINGSIZE128	0x00000400	/* rx descptr ring size: 128 */
#define RXR_CFG_RINGSIZE256	0x00000600	/* rx descptr ring size: 128 */
#define RXR_CFG_reserved3	0x0000f800	/* reserved bits */
#define RXR_CFG_CSUMSTART	0x007f0000	/* rx offset of checksum */
#define RXR_CFG_CSUM_SHIFT	16

/*
 * configuration registers
 */
struct hme_cr {
        volatile u_int32_t	xif_cfg;	/* xif configuration reg */
        volatile u_int32_t	_padding[129];	/* reserved */
        volatile u_int32_t	tx_swreset;	/* tx software reset */
        volatile u_int32_t	tx_cfg;		/* tx configuration reg */
        volatile u_int32_t	ipkt_gap1;	/* interpacket gap 1 */
        volatile u_int32_t	ipkt_gap2;	/* interpacket gap 2 */
        volatile u_int32_t	attempt_limit;	/* tx attempt limit */
        volatile u_int32_t	stime;		/* tx slot time */
        volatile u_int32_t	preamble_len;	/* len of tx preamble */
        volatile u_int32_t	preamble_patt;	/* tx preamble pattern */
        volatile u_int32_t	tx_sframedelim;	/* tx frame delimiter */
        volatile u_int32_t	jsize;		/* tx jam size */
        volatile u_int32_t	tx_pkt_max;	/* tx maximum pkt size */
        volatile u_int32_t	tx_pkt_min;	/* tx minimum pkt size */
        volatile u_int32_t	peak_attempt;	/* tx peak counter */
        volatile u_int32_t	dt_ctr;		/* tx defer counter */
        volatile u_int32_t	nc_ctr;		/* tx normal collision cntr */
        volatile u_int32_t	fc_ctr;		/* tx first collision cntr */
        volatile u_int32_t	ex_ctr;		/* tx execess collision cntr */
        volatile u_int32_t	lt_ctr;		/* tx late collision cntr */
        volatile u_int32_t	rand_seed;	/* tx random seed */
        volatile u_int32_t	tx_smachine;	/* tx state machine */
        volatile u_int32_t	_padding2[44];	/* reserved */
        volatile u_int32_t	rx_swreset;	/* rx software reset */
        volatile u_int32_t	rx_cfg;		/* rx configuration */
        volatile u_int32_t	rx_pkt_max;	/* rx maximum pkt size */
        volatile u_int32_t	rx_pkt_min;	/* rx minimum pkt size */
        volatile u_int32_t	mac_addr2;	/* macaddress register2 (MSB) */
        volatile u_int32_t	mac_addr1;	/* macaddress register1 */
        volatile u_int32_t	mac_addr0;	/* macaddress register0 (LSB) */
        volatile u_int32_t	fr_ctr;		/* rx frame counter */
        volatile u_int32_t	gle_ctr;	/* rx giant counter */
        volatile u_int32_t	unale_ctr;	/* rx unaligned error cntr */
        volatile u_int32_t	rcrce_ctr;	/* rx crc error cntr */
        volatile u_int32_t	rx_smachine;	/* rx state machine */
        volatile u_int32_t	rx_cvalid;	/* rx code violation */
        volatile u_int32_t	_padding3;	/* reserved */
        volatile u_int32_t	htable3;	/* hash table 3 */
        volatile u_int32_t	htable2;	/* hash table 2 */
        volatile u_int32_t	htable1;	/* hash table 1 */
        volatile u_int32_t	htable0;	/* hash table 0 */
        volatile u_int32_t	afilter2;	/* address filter 2 */
        volatile u_int32_t	afilter1;	/* address filter 1 */
        volatile u_int32_t	afilter0;	/* address filter 0 */
        volatile u_int32_t	afilter_mask;	/* address filter mask */
};

/* BigMac XIF config register. */
#define CR_XCFG_ODENABLE  0x00000001 /* Output driver enable         */
#define CR_XCFG_XLBACK    0x00000002 /* Loopback-mode XIF enable     */
#define CR_XCFG_MLBACK    0x00000004 /* Loopback-mode MII enable     */
#define CR_XCFG_MIIDISAB  0x00000008 /* MII receive buffer disable   */
#define CR_XCFG_SQENABLE  0x00000010 /* SQE test enable              */
#define CR_XCFG_SQETWIN   0x000003e0 /* SQE time window              */
#define CR_XCFG_LANCE     0x00000010 /* Lance mode enable            */
#define CR_XCFG_LIPG0     0x000003e0 /* Lance mode IPG0              */

/* BigMac transmit config register. */
#define CR_TXCFG_ENABLE   0x00000001 /* Enable the transmitter       */
#define CR_TXCFG_SMODE    0x00000020 /* Enable slow transmit mode    */
#define CR_TXCFG_CIGN     0x00000040 /* Ignore transmit collisions   */
#define CR_TXCFG_FCSOFF   0x00000080 /* Do not emit FCS              */
#define CR_TXCFG_DBACKOFF 0x00000100 /* Disable backoff              */
#define CR_TXCFG_FULLDPLX 0x00000200 /* Enable full-duplex           */
#define CR_TXCFG_DGIVEUP  0x00000400 /* Don't give up on transmits   */

/* BigMac receive config register. */
#define CR_RXCFG_ENABLE   0x00000001 /* Enable the receiver             */
#define CR_RXCFG_PSTRIP   0x00000020 /* Pad byte strip enable           */
#define CR_RXCFG_PMISC    0x00000040 /* Enable promiscous mode          */
#define CR_RXCFG_DERR     0x00000080 /* Disable error checking          */
#define CR_RXCFG_DCRCS    0x00000100 /* Disable CRC stripping           */
#define CR_RXCFG_ME       0x00000200 /* Receive packets addressed to me */
#define CR_RXCFG_PGRP     0x00000400 /* Enable promisc group mode       */
#define CR_RXCFG_HENABLE  0x00000800 /* Enable the hash filter          */
#define CR_RXCFG_AENABLE  0x00001000 /* Enable the address filter       */

struct hme_tcvr {
	volatile u_int32_t bb_clock;	/* bit bang clock */
	volatile u_int32_t bb_data;	/* bit bang data */
	volatile u_int32_t bb_oenab;	/* bit bang output enable */
	volatile u_int32_t frame;	/* frame control & data */
	volatile u_int32_t cfg;		/* MIF configuration */
	volatile u_int32_t int_mask;	/* MIF interrupt mask */
	volatile u_int32_t status;	/* MIF status */
	volatile u_int32_t smachine;	/* MIF state machine */
};

#define	FRAME_WRITE	0x50020000	/* start a frame write */
#define	FRAME_READ	0x60020000	/* start a frame read */
#define	TCVR_FAILURE	0x80000000	/* impossible value */

/* Transceiver config register */
#define TCVR_CFG_PSELECT	    0x00000001	/* select PHY */
#define TCVR_CFG_PENABLE	0x00000002	/* enable MIF polling */
#define TCVR_CFG_BENABLE	0x00000004	/* enable bit bang */
#define TCVR_CFG_PREGADDR	0x000000f8	/* poll register addr */
#define TCVR_CFG_MDIO0		0x00000100	/* MDIO zero, data/attached */
#define TCVR_CFG_MDIO1		0x00000200	/* MDIO one,  data/attached */
#define TCVR_CFG_PDADDR		0x00007c00	/* device phy addr polling */

/* Here are some PHY addresses. */
#define	TCVR_PHYADDR_ETX		0	/* external transceiver */
#define	TCVR_PHYADDR_ITX		1	/* internal transceiver */

/* Transceiver status register */
#define TCVR_STAT_BASIC		0xffff0000	/* The "basic" part */
#define TCVR_STAT_NORMAL	0x0000ffff	/* The "non-basic" part */

/* hme flags */
#define	HME_FLAG_POLL		0x00000001	/* polling mif? */
#define	HME_FLAG_FENABLE	0x00000002	/* MII frame enabled? */
#define	HME_FLAG_LANCE		0x00000004	/* Lance mode IPG0? */
#define	HME_FLAG_RXENABLE	0x00000008	/* Receiver enabled? */
#define	HME_FLAG_AUTO		0x00000010	/* Auto-Neg? 0 = force */
#define	HME_FLAG_FULL		0x00000020	/* Full duplex enabled? */
#define	HME_FLAG_MACFULL	0x00000040	/* Full duplex in the MAC? */
#define	HME_FLAG_POLLENABLE	0x00000080	/* Try MIF polling? */
#define	HME_FLAG_RXCV		0x00000100	/* RXCV enable - XXX */
#define	HME_FLAG_INIT		0x00000200	/* Initialized once? */
#define	HME_FLAG_LINKUP		0x00000400	/* Is link up? */

#define	HME_FLAG_20_21		\
    (HME_FLAG_POLLENABLE | HME_FLAG_FENABLE)
#define HME_FLAG_NOT_A0 \
    (HME_FLAG_POLLENABLE | HME_FLAG_FENABLE | HME_FLAG_LANCE | HME_FLAG_RXCV)

/*
 * Transceiver type
 */
#define HME_TCVR_EXTERNAL	0
#define HME_TCVR_INTERNAL	1
#define HME_TCVR_NONE		2

struct hme_rxd {
	volatile u_int32_t	rx_flags;
	volatile u_int32_t	rx_addr;
};
#define	HME_RXD_OWN		0x80000000	/* desc owner: 1=hw,0=sw */
#define HME_RXD_OVERFLOW	0x40000000	/* 1 = buffer over flow */
#define HME_RXD_SIZE		0x3fff0000	/* descriptor size */
#define HME_RXD_CSUM		0x0000ffff	/* checksum mask */

struct hme_txd {
	volatile u_int32_t	tx_flags;
	volatile u_int32_t	tx_addr;
};
#define	HME_TXD_OWN		0x80000000	/* desc owner: 1=hw,0=sw */
#define	HME_TXD_SOP		0x40000000 	/* 1 = start of pkt */
#define	HME_TXD_EOP		0x20000000	/* 1 = end of pkt */
#define	HME_TXD_CSENABLE	0x10000000	/* 1 = use hw checksums */
#define	HME_TXD_CSLOCATION	0x0ff00000	/* checksum location mask */
#define	HME_TXD_CSBUFBEGIN	0x000fc000	/* checksum begin mask */
#define	HME_TXD_SIZE		0x00003fff 	/* pkt size mask */

#define HME_RX_RING_SIZE	32		/* Must be 32,64,128, or 256 */
#define HME_TX_RING_SIZE	32		/* 16<=x<=256 and div by 16 */
#define HME_RX_RING_MAX		256		/* maximum ring size: rx */
#define HME_TX_RING_MAX		256		/* maximum ring size: tx */
#define HME_RX_PKT_BUF_SZ	2048		/* size of a rx buffer */
#define HME_RX_OFFSET		2		/* packet offset */
#define HME_RX_CSUMLOC		0x00		/* checksum location */
#define HME_TX_PKT_BUF_SZ	1546		/* size of a tx buffer */
#define HME_RX_ALIGN_SIZE	64		/* XXX rx bufs must align 64 */
#define HME_RX_ALIGN_MASK	(~(RX_ALIGN_SIZE - 1))

struct hme_desc {
	struct hme_rxd hme_rxd[HME_RX_RING_MAX];
	struct hme_txd hme_txd[HME_TX_RING_MAX];
};

struct hme_bufs {
	char rx_buf[HME_RX_RING_SIZE][HME_RX_PKT_BUF_SZ];
	char tx_buf[HME_TX_RING_SIZE][HME_TX_PKT_BUF_SZ];
};
