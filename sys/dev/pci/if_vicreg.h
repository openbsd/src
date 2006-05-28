/*	$OpenBSD: if_vicreg.h,v 1.2 2006/05/28 00:20:21 brad Exp $	*/

/*
 * Copyright (c) 2006 Reyk Floeter <reyk@openbsd.org>
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

#ifndef _DEV_IC_VICREG_H
#define _DEV_IC_VICREG_H

#define VIC_MAGIC		0xbabe864f

/*
 * Register address offsets
 */

#define VIC_DATA_ADDR		0x0000		/* Shared data address */
#define VIC_DATA_LENGTH		0x0004		/* Shared data length */
#define VIC_Tx_ADDR		0x0008		/* Tx pointer address */

/* Command register */
#define VIC_CMD			0x000c		/* Command register */
#define VIC_CMD_INTR_ACK	0x00000001	/* Acknowledge interrupt */
#define VIC_CMD_MCASTFIL	0x00000002	/* Multicast address filter */
#define VIC_CMD_MCASTFIL_LENGTH	2
#define VIC_CMD_IFF		0x00000004	/* Interface flags */
#define VIC_CMD_IFF_PROMISC	0x0001		/* Promiscous enabled */
#define VIC_CMD_IFF_BROADCAST	0x0002		/* Broadcast enabled */
#define VIC_CMD_IFF_MULTICAST	0x0004		/* Multicast enabled */
#define VIC_CMD_RESERVED_8	0x00000008
#define VIC_CMD_RESERVED_10	0x00000010
#define VIC_CMD_INTR_DISABLE	0x00000020	/* Enable interrupts */
#define VIC_CMD_INTR_ENABLE	0x00000040	/* Disable interrupts */
#define VIC_CMD_RESERVED_80	0x00000080
#define VIC_CMD_Tx_DONE		0x00000100	/* Tx done register */
#define VIC_CMD_NUM_Rx_BUF	0x00000200	/* Number of Rx buffers */
#define VIC_CMD_NUM_Tx_BUF	0x00000400	/* Number of Tx buffers */
#define VIC_CMD_NUM_PINNED_BUF	0x00000800	/* Number of pinned buffers */
#define VIC_CMD_HWCAP		0x00001000	/* Capability register */
#define VIC_CMD_HWCAP_SG	0x0001		/* Scatter-gather transmits */
#define VIC_CMD_HWCAP_CSUM_IPv4	0x0002		/* Hardware checksum of TCP/UDP */
#define VIC_CMD_HWCAP_CSUM_ALL	0x0004		/* Hardware checksum support */
#define VIC_CMD_HWCAP_CSUM							\
	(VIC_CMD_HWCAP_CSUM_IPv4 | VIC_CMD_HWCAP_CSUM_ALL)
#define VIC_CMD_HWCAP_DMA_HIGH	0x0008		/* High DMA mapping possible */
#define VIC_CMD_HWCAP_TOE	0x0010		/* TCP offload engine available */
#define VIC_CMD_HWCAP_TSO	0x0020		/* TCP segmentation offload */
#define VIC_CMD_HWCAP_TSO_SW	0x0040		/* Software TCP segmentation */
#define VIC_CMD_HWCAP_VPROM	0x0080		/* Virtual PROM available */
#define VIC_CMD_HWCAP_VLAN_Tx	0x0100		/* Hardware VLAN MTU Rx */
#define VIC_CMD_HWCAP_VLAN_Rx	0x0200		/* Hardware VLAN MTU Tx */
#define VIC_CMD_HWCAP_VLAN_SW	0x0400		/* Software VLAN MTU */
#define VIC_CMD_HWCAP_VLAN							\
	(VIC_CMD_HWCAP_VLAN_Tx | VIC_CMD_HWCAP_VLAN_Rx | VIC_CMD_HWCAP_VLAN_SW)
#define VIC_CMD_HWCAP_BITS							\
	"\20\01SG\02CSUM4\03CSUM\04HDMA\05TOE"					\
	"\06TSO\07TSOSW\10VPROM\13VLANTx\14VLANRx\15VLANSW"
#define VIC_CMD_FEATURE		0x00002000	/* Additional feature register */
#define VIC_CMD_FEATURE_0_Tx	0x0001
#define VIC_CMD_FEATURE_TSO	0x0002

#define VIC_LLADDR		0x0010		/* MAC address register */
#define VIC_VERSION_MINOR	0x0018		/* Minor version register */
#define VIC_VERSION_MAJOR	0x001c		/* Major version register */
#define VIC_VERSION_MAJOR_M	0xffff0000

/* Status register */
#define VIC_STATUS		0x0020
#define VIC_STATUS_CONNECTED	0x00000001
#define VIC_STATUS_ENABLED	0x00000002

#define VIC_TOE_ADDR		0x0024		/* TCP offload address */

/* Virtual PROM address */
#define VIC_VPROM		0x0028
#define VIC_VPROM_LENGTH	6

/*
 * Shared DMA data structures
 */

struct vic_sg {
	u_int32_t	sg_addr_low;
	u_int16_t	sg_addr_high;
	u_int16_t	sg_length;
} __packed;

#define VIC_SG_MAX		6
#define VIC_SG_ADDR_MACH	0
#define VIC_SG_ADDR_PHYS	1
#define VIC_SG_ADDR_VIRT	3

struct vic_sgarray {
	u_int16_t	sa_addr_type;
	u_int16_t	sa_length;
	struct vic_sg	sa_sg[VIC_SG_MAX];
};

struct vic_rxdesc {
	u_int64_t	rx_physaddr;
	u_int32_t	rx_buflength;
	u_int32_t	rx_length;
	u_int16_t	rx_owner;
	u_int16_t	rx_flags;
	void 		*rx_priv;
} __packed;

#define VIC_RX_FLAGS_CSUMHW_OK	0x0001

struct vic_txdesc {
	u_int16_t		tx_flags;
	u_int16_t		tx_owner;
	void			*tx_priv;
	u_int32_t		tx_tsomss;
	struct vic_sgarray	tx_sa;
} __packed;

#define VIC_TX_FLAGS_KEEP	0x0001
#define VIC_TX_FLAGS_TXURN	0x0002
#define VIC_TX_FLAGS_CSUMHW	0x0004
#define VIC_TX_FLAGS_TSO	0x0008
#define VIC_TX_FLAGS_PINNED	0x0010
#define VIC_TX_FLAGS_QRETRY	0x1000

struct vic_stats {
	u_int32_t		vs_tx_count;
	u_int32_t		vs_tx_packets;
	u_int32_t		vs_tx_0copy;
	u_int32_t		vs_tx_copy;
	u_int32_t		vs_tx_maxpending;
	u_int32_t		vs_tx_stopped;
	u_int32_t		vs_tx_overrun;
	u_int32_t		vs_intr;
	u_int32_t		vs_rx_packets;
	u_int32_t		vs_rx_underrun;
} __packed;

struct vic_data {
	u_int32_t		vd_magic;

	u_int32_t		vd_rx_length;
	u_int32_t		vd_rx_nextidx;
	u_int32_t		vd_rx_length2;
	u_int32_t		vd_rx_nextidx2;

	u_int32_t		vd_irq;
	u_int32_t		vd_iff;

	u_int32_t		vd_mcastfil[VIC_CMD_MCASTFIL_LENGTH];

	u_int32_t		vd_reserved1[1];

	u_int32_t		vd_tx_length;
	u_int32_t		vd_tx_curidx;
	u_int32_t		vd_tx_nextidx;
	u_int32_t		vd_tx_stopped;
	u_int32_t		vd_tx_triggerlvl;
	u_int32_t		vd_tx_queued;
	u_int32_t		vd_tx_minlength;

	u_int32_t		vd_reserved2[6];

	u_int32_t		vd_rx_saved_nextidx;
	u_int32_t		vd_rx_saved_nextidx2;
	u_int32_t		vd_tx_saved_nextidx;

	u_int32_t		vd_length;
	u_int32_t		vd_rx_offset;
	u_int32_t		vd_rx_offset2;
	u_int32_t		vd_tx_offset;
	u_int32_t		vd_debug;
	u_int32_t		vd_tx_physaddr;
	u_int32_t		vd_tx_physaddr_length;
	u_int32_t		vd_tx_maxlength;

	struct vic_stats	vd_stats;
} __packed;

#define VIC_OWNER_DRIVER	0
#define VIC_OWNER_DRIVER_PEND	1
#define VIC_OWNER_NIC		2
#define VIC_OWNER_NIC_PEND	3


#define VIC_JUMBO_FRAMELEN	9018
#define VIC_JUMBO_MTU		(VIC_JUMBO_FRAMELEN - ETHER_HDR_LEN - ETHER_CRC_LEN)

#endif /* _DEV_IC_VICREG_H */
