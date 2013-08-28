/*	$OpenBSD: if_vmxreg.h,v 1.3 2013/08/28 10:19:19 reyk Exp $	*/

/*
 * Copyright (c) 2013 Tsubai Masanari
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

struct UPT1_TxStats {
	u_int64_t TSO_packets;
	u_int64_t TSO_bytes;
	u_int64_t ucast_packets;
	u_int64_t ucast_bytes;
	u_int64_t mcast_packets;
	u_int64_t mcast_bytes;
	u_int64_t bcast_packets;
	u_int64_t bcast_bytes;
	u_int64_t error;
	u_int64_t discard;
} __packed;

struct UPT1_RxStats {
	u_int64_t LRO_packets;
	u_int64_t LRO_bytes;
	u_int64_t ucast_packets;
	u_int64_t ucast_bytes;
	u_int64_t mcast_packets;
	u_int64_t mcast_bytes;
	u_int64_t bcast_packets;
	u_int64_t bcast_bytes;
	u_int64_t nobuffer;
	u_int64_t error;
} __packed;

/* interrupt moderation levels */
#define UPT1_IMOD_NONE     0		/* no moderation */
#define UPT1_IMOD_HIGHEST  7		/* least interrupts */
#define UPT1_IMOD_ADAPTIVE 8		/* adaptive interrupt moderation */

/* hardware features */
#define UPT1_F_CSUM 0x0001		/* Rx checksum verification */
#define UPT1_F_RSS  0x0002		/* receive side scaling */
#define UPT1_F_VLAN 0x0004		/* VLAN tag stripping */
#define UPT1_F_LRO  0x0008		/* large receive offloading */

#define VMXNET3_BAR0_IMASK(irq)	(0x000 + (irq) * 8)	/* interrupt mask */
#define VMXNET3_BAR0_TXH(q)	(0x600 + (q) * 8)	/* Tx head */
#define VMXNET3_BAR0_RXH1(q)	(0x800 + (q) * 8)	/* ring1 Rx head */
#define VMXNET3_BAR0_RXH2(q)	(0xa00 + (q) * 8)	/* ring2 Rx head */
#define VMXNET3_BAR1_VRRS	0x000	/* VMXNET3 revision report selection */
#define VMXNET3_BAR1_UVRS	0x008	/* UPT version report selection */
#define VMXNET3_BAR1_DSL	0x010	/* driver shared address low */
#define VMXNET3_BAR1_DSH	0x018	/* driver shared address high */
#define VMXNET3_BAR1_CMD	0x020	/* command */
#define VMXNET3_BAR1_MACL	0x028	/* MAC address low */
#define VMXNET3_BAR1_MACH	0x030	/* MAC address high */
#define VMXNET3_BAR1_INTR	0x038	/* interrupt status */
#define VMXNET3_BAR1_EVENT	0x040	/* event status */

#define VMXNET3_CMD_ENABLE	0xcafe0000	/* enable VMXNET3 */
#define VMXNET3_CMD_DISABLE	0xcafe0001	/* disable VMXNET3 */
#define VMXNET3_CMD_RESET	0xcafe0002	/* reset device */
#define VMXNET3_CMD_SET_RXMODE	0xcafe0003	/* set interface flags */
#define VMXNET3_CMD_SET_FILTER	0xcafe0004	/* set address filter */
#define VMXNET3_CMD_GET_STATUS	0xf00d0000	/* get queue errors */
#define VMXNET3_CMD_GET_LINK	0xf00d0002	/* get link status */
#define VMXNET3_CMD_GET_MACL	0xf00d0003
#define VMXNET3_CMD_GET_MACH	0xf00d0004

#define VMXNET3_DMADESC_ALIGN	128

/* All descriptors are in little-endian format. */
struct vmxnet3_txdesc {
	u_int64_t		tx_addr;

	u_int32_t		tx_word2;
#define	VMXNET3_TX_LEN_M	0x00003fff
#define	VMXNET3_TX_LEN_S	0
#define VMXNET3_TX_GEN_M	0x00000001	/* generation */
#define VMXNET3_TX_GEN_S	14
#define VMXNET3_TX_RES0		0x00008000
#define	VMXNET3_TX_DTYPE_M	0x00000001	/* descriptor type */
#define	VMXNET3_TX_DTYPE_S	16		/* descriptor type */
#define	VMXNET3_TX_RES1		0x00000002
#define VMXNET3_TX_OP_M		0x00003fff	/* offloading position */
#define	VMXNET3_TX_OP_S		18

	u_int32_t		tx_word3;
#define VMXNET3_TX_HLEN_M	0x000003ff	/* header len */
#define VMXNET3_TX_HLEN_S	0
#define VMXNET3_TX_OM_M		0x00000003	/* offloading mode */
#define VMXNET3_TX_OM_S		10
#define VMXNET3_TX_EOP		0x00001000	/* end of packet */
#define VMXNET3_TX_COMPREQ	0x00002000	/* completion request */
#define VMXNET3_TX_RES2		0x00004000
#define VMXNET3_TX_VTAG_MODE	0x00008000	/* VLAN tag insertion mode */
#define VMXNET3_TX_VLANTAG_M	0x0000ffff
#define VMXNET3_TX_VLANTAG_S	16
} __packed;

/* offloading modes */
#define VMXNET3_OM_NONE 0
#define VMXNET3_OM_CSUM 2
#define VMXNET3_OM_TSO  3

struct vmxnet3_txcompdesc {
	u_int32_t		txc_word0;	
#define VMXNET3_TXC_EOPIDX_M	0x00000fff	/* eop index in Tx ring */
#define VMXNET3_TXC_EOPIDX_S	0
#define VMXNET3_TXC_RES0_M	0x000fffff
#define VMXNET3_TXC_RES0_S	12

	u_int32_t		txc_word1;
	u_int32_t		txc_word2;

	u_int32_t		txc_word3;
#define VMXNET3_TXC_RES2_M	0x00ffffff
#define VMXNET3_TXC_TYPE_M	0x0000007f
#define VMXNET3_TXC_TYPE_S	24
#define VMXNET3_TXC_GEN_M	0x00000001
#define VMXNET3_TXC_GEN_S	31
} __packed;

struct vmxnet3_rxdesc {
	u_int64_t		rx_addr;

	u_int32_t		rx_word2;
#define VMXNET3_RX_LEN_M	0x00003fff
#define VMXNET3_RX_LEN_S	0
#define VMXNET3_RX_BTYPE_M	0x00000001	/* buffer type */
#define VMXNET3_RX_BTYPE_S	14
#define VMXNET3_RX_DTYPE_M	0x00000001	/* descriptor type */
#define VMXNET3_RX_DTYPE_S	15
#define VMXNET3_RX_RES0_M	0x00007fff
#define VMXNET3_RX_RES0_S	16
#define VMXNET3_RX_GEN_M	0x00000001
#define VMXNET3_RX_GEN_S	31

	u_int32_t		rx_word3;
} __packed;

/* buffer types */
#define VMXNET3_BTYPE_HEAD 0	/* head only */
#define VMXNET3_BTYPE_BODY 1	/* body only */

struct vmxnet3_rxcompdesc {
	u_int32_t		rxc_word0;
#define VMXNET3_RXC_IDX_M	0x00000fff	/* Rx descriptor index */
#define VMXNET3_RXC_IDX_S	0
#define VMXNET3_RXC_RES0_M	0x00000003
#define VMXNET3_RXC_RES0_S	12
#define VMXNET3_RXC_EOP		0x00004000	/* end of packet */
#define VMXNET3_RXC_SOP		0x00008000	/* start of packet */
#define VMXNET3_RXC_QID_M	0x000003ff
#define VMXNET3_RXC_QID_S	16
#define VMXNET3_RXC_RSSTYPE_M	0x0000000f
#define VMXNET3_RXC_RSSTYPE_S	26
#define VMXNET3_RXC_NOCSUM	0x40000000	/* no checksum calculated */
#define VMXNET3_RXC_RES1	0x80000000

	u_int32_t		rxc_word1;
#define VMXNET3_RXC_RSSHASH_M	0xffffffff	/* RSS hash value */
#define VMXNET3_RXC_RSSHASH_S	0

	u_int32_t		rxc_word2;
#define VMXNET3_RXC_LEN_M	0x00003fff
#define VMXNET3_RXC_LEN_S	0
#define VMXNET3_RXC_ERROR	0x00004000
#define VMXNET3_RXC_VLAN	0x00008000	/* 802.1Q VLAN frame */
#define VMXNET3_RXC_VLANTAG_M	0x0000ffff	/* VLAN tag */
#define VMXNET3_RXC_VLANTAG_S	16

	u_int32_t		rxc_word3;
#define VMXNET3_RXC_CSUM_M	0x0000ffff	/* TCP/UDP checksum */
#define VMXNET3_RXC_CSUM_S	16
#define VMXNET3_RXC_CSUM_OK	0x00010000	/* TCP/UDP checksum ok */
#define VMXNET3_RXC_UDP		0x00020000
#define VMXNET3_RXC_TCP		0x00040000
#define VMXNET3_RXC_IPSUM_OK	0x00080000	/* IP checksum ok */
#define VMXNET3_RXC_IPV6	0x00100000
#define VMXNET3_RXC_IPV4	0x00200000
#define VMXNET3_RXC_FRAGMENT	0x00400000	/* IP fragment */
#define VMXNET3_RXC_FCS		0x00800000	/* frame CRC correct */
#define VMXNET3_RXC_TYPE_M	0x7f000000
#define VMXNET3_RXC_GEN_M	0x00000001
#define VMXNET3_RXC_GEN_S	31
} __packed;

#define VMXNET3_REV1_MAGIC 0xbabefee1

#define VMXNET3_GOS_UNKNOWN 0x00
#define VMXNET3_GOS_LINUX   0x04
#define VMXNET3_GOS_WINDOWS 0x08
#define VMXNET3_GOS_SOLARIS 0x0c
#define VMXNET3_GOS_FREEBSD 0x10
#define VMXNET3_GOS_PXE     0x14

#define VMXNET3_GOS_32BIT   0x01
#define VMXNET3_GOS_64BIT   0x02

#define VMXNET3_MAX_TX_QUEUES 8
#define VMXNET3_MAX_RX_QUEUES 16
#define VMXNET3_MAX_INTRS (VMXNET3_MAX_TX_QUEUES + VMXNET3_MAX_RX_QUEUES + 1)
#define VMXNET3_NINTR 1

#define VMXNET3_ICTRL_DISABLE_ALL 0x01

#define VMXNET3_RXMODE_UCAST    0x01
#define VMXNET3_RXMODE_MCAST    0x02
#define VMXNET3_RXMODE_BCAST    0x04
#define VMXNET3_RXMODE_ALLMULTI 0x08
#define VMXNET3_RXMODE_PROMISC  0x10

#define VMXNET3_EVENT_RQERROR 0x01
#define VMXNET3_EVENT_TQERROR 0x02
#define VMXNET3_EVENT_LINK    0x04
#define VMXNET3_EVENT_DIC     0x08
#define VMXNET3_EVENT_DEBUG   0x10

#define VMXNET3_MAX_MTU 9000
#define VMXNET3_MIN_MTU 60

struct vmxnet3_driver_shared {
	u_int32_t magic;
	u_int32_t pad1;

	u_int32_t version;		/* driver version */
	u_int32_t guest;		/* guest OS */
	u_int32_t vmxnet3_revision;	/* supported VMXNET3 revision */
	u_int32_t upt_version;		/* supported UPT version */
	u_int64_t upt_features;
	u_int64_t driver_data;
	u_int64_t queue_shared;
	u_int32_t driver_data_len;
	u_int32_t queue_shared_len;
	u_int32_t mtu;
	u_int16_t nrxsg_max;
	u_int8_t ntxqueue;
	u_int8_t nrxqueue;
	u_int32_t reserved1[4];

	/* interrupt control */
	u_int8_t automask;
	u_int8_t nintr;
	u_int8_t evintr;
	u_int8_t modlevel[VMXNET3_MAX_INTRS];
	u_int32_t ictrl;
	u_int32_t reserved2[2];

	/* receive filter parameters */
	u_int32_t rxmode;
	u_int16_t mcast_tablelen;
	u_int16_t pad2;
	u_int64_t mcast_table;
	u_int32_t vlan_filter[4096 / 32];

	struct {
		u_int32_t version;
		u_int32_t len;
		u_int64_t paddr;
	} rss, pm, plugin;

	u_int32_t event;
	u_int32_t reserved3[5];
} __packed;

struct vmxnet3_txq_shared {
	u_int32_t npending;
	u_int32_t intr_threshold;
	u_int64_t reserved1;

	u_int64_t cmd_ring;
	u_int64_t data_ring;
	u_int64_t comp_ring;
	u_int64_t driver_data;
	u_int64_t reserved2;
	u_int32_t cmd_ring_len;
	u_int32_t data_ring_len;
	u_int32_t comp_ring_len;
	u_int32_t driver_data_len;
	u_int8_t intr_idx;
	u_int8_t pad1[7];

	u_int8_t stopped;
	u_int8_t pad2[3];
	u_int32_t error;

	struct UPT1_TxStats stats;

	u_int8_t pad3[88];
} __packed;

struct vmxnet3_rxq_shared {
	u_int8_t update_rxhead;
	u_int8_t pad1[7];
	u_int64_t reserved1;

	u_int64_t cmd_ring[2];
	u_int64_t comp_ring;
	u_int64_t driver_data;
	u_int64_t reserved2;
	u_int32_t cmd_ring_len[2];
	u_int32_t comp_ring_len;
	u_int32_t driver_data_len;
	u_int8_t intr_idx;
	u_int8_t pad2[7];

	u_int8_t stopped;
	u_int8_t pad3[3];
	u_int32_t error;

	struct UPT1_RxStats stats;

	u_int8_t pad4[88];
} __packed;
