/*	$OpenBSD: if_nxreg.h,v 1.28 2007/05/28 19:44:15 reyk Exp $	*/

/*
 * Copyright (c) 2007 Reyk Floeter <reyk@openbsd.org>
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

/*
 * NetXen NX2031/NX2035 register definitions partially based on:
 * http://www.netxen.com/products/downloads/
 *     Ethernet_Driver_Ref_Guide_Open_Source.pdf
 */

#ifndef _NX_REG_H
#define _NX_REG_H

/*
 * Common definitions
 */

#define NX_MAX_PORTS		4
#define NX_MAX_MTU		ETHER_MTU

#define NX_MAX_TX_DESC		128	/* XXX 4096 */
#define NX_MAX_RX_DESC		128	/* XXX 32768 */
#define NX_MAX_JUMBO_DESC	64	/* XXX 1024 */
#define NX_MAX_TSO_DESC		32	/* XXX 64 */
#define NX_MAX_STATUS_DESC	NX_MAX_RX_DESC

#define NX_JUMBO_MTU		8000	/* less than 9k */
#define NX_DMA_ALIGN		8	/* 64bit alignment */
#define NX_POLL_SENSOR		10	/* read temp sensor every 10s */

#define NX_WAIT			1
#define NX_NOWAIT		0

/* This driver supported the 3.4.31 (3.4.xx) NIC firmware */
#define NX_FIRMWARE_MAJOR	3
#define NX_FIRMWARE_MINOR	4
#define NX_FIRMWARE_BUILD	31

#define NX_FIRMWARE_VER		(					\
	(NX_FIRMWARE_MAJOR << 16) | (NX_FIRMWARE_MINOR << 8) |		\
	NX_FIRMWARE_BUILD						\
)

/* Used to indicate various states of the NIC and its firmware */
enum nx_state {
	NX_S_FAIL	= -1,	/* Failed to initialize the device */
	NX_S_OFFLINE	= 0,	/* Firmware is not active yet */
	NX_S_RESET	= 2,	/* Firmware is in reset state */
	NX_S_BOOT	= 3,	/* Chipset is booting the firmware */
	NX_S_LOADED	= 4,	/* Firmware is loaded but not initialized */
	NX_S_RELOADED	= 5,	/* Firmware is reloaded and initialized */
	NX_S_READY	= 6	/* Device has been initialized and is ready */
};

/*
 * Hardware descriptors
 */

struct nx_txdesc {
	u_int64_t		tx_word0;
#define  NX_TXDESC0_TCPHDROFF_S	0		/* TCP header offset */
#define  NX_TXDESC0_TCPHDROFF_M	0x00000000000000ffULL
#define  NX_TXDESC0_IPHDROFF_S	8		/* IP header offset */
#define  NX_TXDESC0_IPHDROFF_M	0x000000000000ff00
#define  NX_TXDESC0_F_S		16		/* flags */
#define  NX_TXDESC0_F_M		0x00000000007f0000ULL
#define   NX_TXDESC0_F_VLAN	(1<<8)		/* VLAN tagged */
#define   NX_TXDESC0_F_TSO	(1<<1)		/* TSO enabled */
#define   NX_TXDESC0_F_CKSUM	(1<<0)		/* checksum enabled */
#define  NX_TXDESC0_OP_S	23		/* opcode */
#define  NX_TXDESC0_OP_M	0x000000001f800000ULL
#define   NX_TXDESC0_OP_TX_TSO	(1<<4)		/* TCP packet, do TSO */
#define   NX_TXDESC0_OP_TX_IP	(1<<3)		/* IP packet, compute cksum */
#define   NX_TXDESC0_OP_TX_UDP	(1<<2)		/* UDP packet, compute cksum */
#define   NX_TXDESC0_OP_TX_TCP	(1<<1)		/* TCP packet, compute cksum */
#define   NX_TXDESC0_OP_TX	(1<<0)		/* raw Ethernet packet */
#define  NX_TXDESC0_RES0_S	29		/* Reserved */
#define  NX_TXDESC0_RES0_M	0x00000000e0000000ULL
#define  NX_TXDESC0_NBUF_S	32		/* number of buffers */
#define  NX_TXDESC0_NBUF_M	0x000000ff00000000ULL
#define  NX_TXDESC0_LENGTH_S	40		/* length */
#define  NX_TXDESC0_LENGTH_M	0xffffff0000000000ULL
	u_int64_t		tx_addr2;	/* address of buffer 2 */
	u_int64_t		tx_word2;
#define  NX_TXDESC2_HANDLE_S	0		/* handle of the buffer */
#define  NX_TXDESC2_HANDLE_M	0x000000000000ffffULL
#define  NX_TXDESC2_MSS_S	16		/* MSS for the packet */
#define  NX_TXDESC2_MSS_M	0x00000000ffff0000ULL
#define  NX_TXDESC2_PORT_S	32		/* interface port */
#define  NX_TXDESC2_PORT_M	0x0000000f00000000ULL
#define  NX_TXDESC2_CTXID_S	36		/* context ID */
#define  NX_TXDESC2_CTXID_M	0x000000f000000000ULL
#define  NX_TXDESC2_HDRLENGTH_S	40		/* MAC+IP+TCP length for TSO */
#define  NX_TXDESC2_HDRLENGTH_M	0x0000ff0000000000ULL
#define  NX_TXDESC2_IPSECID_S	48		/* IPsec offloading SA/ID */
#define  NX_TXDESC2_IPSECID_M	0xffff000000000000ULL
	u_int64_t		tx_addr3;	/* address of buffer 3 */
	u_int64_t		tx_addr1;	/* address of buffer 1 */
	u_int64_t		tx_buflength;
#define  NX_TXDESC_BUFLENGTH1_S	0		/* length of buffer 1 */
#define  NX_TXDESC_BUFLENGTH1_M	0x000000000000ffffULL
#define  NX_TXDESC_BUFLENGTH2_S	16		/* length of buffer 2 */
#define  NX_TXDESC_BUFLENGTH2_M	0x00000000ffff0000ULL
#define  NX_TXDESC_BUFLENGTH3_S	32		/* length of buffer 3 */
#define  NX_TXDESC_BUFLENGTH3_M	0x0000ffff00000000ULL
#define  NX_TXDESC_BUFLENGTH4_S	48		/* length of buffer 4 */
#define  NX_TXDESC_BUFLENGTH4_M	0xffff000000000000ULL
	u_int64_t		tx_addr4;	/* address of buffer 4 */
	u_int64_t		tx_word7;	/* reserved */
} __packed;

struct nx_rxdesc {
	u_int16_t		rx_handle;	/* handle of the buffer */
	u_int16_t		rx_reserved;
	u_int32_t		rx_length;	/* length of the buffer */
	u_int64_t		rx_addr;	/* address of buffer */
} __packed;

struct nx_statusdesc {
	u_int16_t		rx_port;
#define  NX_STSDESC_PORT_S	0		/* interface port */
#define  NX_STSDESC_PORT_M	0x000f
#define  NX_STSDESC_STS_S	4		/* completion status */
#define  NX_STSDESC_STS_M	0x00f0
#define   NX_STSDESC_STS_NOCHK	1		/* checksum not verified */
#define   NX_STSDESC_STS_CHKOK	2		/* checksum verified ok */
#define  NX_STSDESC_TYPE_S	8		/* type/index of the ring */
#define  NX_STSDESC_TYPE_M	0x0f00
#define  NX_STSDESC_OPCODE_S	12		/* opcode */
#define  NX_STSDESC_OPCODE_M	0xf000
#define   NX_STSDESC_OPCODE	0xa		/* received packet */
	u_int16_t		rx_length;	/* total length of the packet */
	u_int16_t		rx_handle;	/* handle of the buffer */
	u_int16_t		rx_owner;
#define  NX_STSDESC_OWNER_S	0		/* owner of the descriptor */
#define  NX_STSDESC_OWNER_M	0x0003
#define   NX_STSDESC_OWNER_HOST	1		/* owner is the host (t.b.d) */
#define   NX_STSDESC_OWNER_CARD	2		/* owner is the card */
#define  NX_STSDESC_PROTO_S	2		/* protocol type */
#define  NX_STSDESC_PROTO_M	0x003c
} __packed;

struct nx_rxcontext {
	u_int64_t		rxc_ringaddr;
	u_int32_t		rxc_ringsize;
	u_int32_t		rxc_reserved;
} __packed;

#define NX_NRXCONTEXT		3
#define NX_RX_CONTEXT		0
#define NX_JUMBO_CONTEXT	1
#define NX_TSO_CONTEXT		2

/* DMA-mapped ring context for the Rx, Tx, and Status rings */
struct nx_ringcontext {
	u_int64_t		rc_txconsumeroff;
	u_int64_t		rc_txringaddr;
	u_int32_t		rc_txringsize;
	u_int32_t		rc_reserved;

	struct nx_rxcontext	rc_rxcontext[NX_NRXCONTEXT];

	u_int64_t		rc_statusringaddr;
	u_int32_t		rc_statusringsize;

	u_int32_t		rc_id;		/* context identifier */

	/* d3 state register, dummy dma address */
	u_int64_t		rc_reserved1;
	u_int64_t		rc_reserved2;

	u_int32_t		rc_txconsumer;
} __packed;

/*
 * Memory layout
 */

#define NXBAR0			PCI_MAPREG_START
#define NXBAR4			(PCI_MAPREG_START + 16)

/* PCI memory setup */
#define NXPCIMEM_SIZE_128MB	0x08000000	/* 128MB size */
#define NXPCIMEM_SIZE_32MB	0x02000000	/* 32MB size */

/* PCI memory address ranges */
#define NXADDR_DDR_NET		0x0000000000000000ULL
#define NXADDR_DDR_NET_END	0x000000000fffffffULL
#define NXADDR_PCIE		0x0000000800000000ULL
#define NXADDR_PCIE_END		0x0000000fffffffffULL
#define NXADDR_OCM0		0x0000000200000000ULL
#define NXADDR_OCM0_END		0x00000002000fffffULL
#define NXADDR_OCM1		0x0000000200400000ULL
#define NXADDR_OCM1_END		0x00000002004fffffULL
#define NXADDR_QDR_NET		0x0000000300000000ULL
#define NXADDR_QDR_NET_END	0x00000003001fffffULL

/* Memory mapping in the default PCI window */
#define NXPCIMAP_DDR_NET	0x00000000
#define NXPCIMAP_DDR_MD		0x02000000
#define NXPCIMAP_QDR_NET	0x04000000
#define NXPCIMAP_DIRECT_CRB	0x04400000
#define NXPCIMAP_OCM0		0x05000000
#define NXPCIMAP_OCM1		0x05100000
#define NXPCIMAP_CRB		0x06000000

/* Offsets inside NXPCIMAP_CRB */
#define NXMEMMAP_PCIE_0		0x00100000
#define NXMEMMAP_NIU		0x00600000
#define NXMEMMAP_PPE_0		0x01100000
#define NXMEMMAP_PPE_1		0x01200000
#define NXMEMMAP_PPE_2		0x01300000
#define NXMEMMAP_PPE_3		0x01400000
#define NXMEMMAP_PPE_D		0x01500000
#define NXMEMMAP_PPE_I		0x01600000
#define NXMEMMAP_PCIE_1		0x02100000
#define NXMEMMAP_SW		0x02200000
#define NXMEMMAP_SIR		0x03200000
#define NXMEMMAP_ROMUSB		0x03300000

/* NXPCIMAP_CRB window (total offsets) */
#define NXMEMMAP_WINDOW_SIZE	0x02000000
#define NXMEMMAP_WINDOW0_START	0x06000000
#define NXMEMMAP_WINDOW0_END	0x07ffffff
#define NXMEMMAP_WINDOW1_START	0x08000000
#define NXMEMMAP_WINDOW1_END	0x09ffffff

#define NXMEMMAP_HWTRANS_M	0xfff00000

/* Window 0 register map  */
#define NXPCIE(_x)		((_x) + 0x06100000)	/* PCI Express */
#define NXNIU(_x)		((_x) + 0x06600000)	/* Network Int Unit */
#define NXPPE_0(_x)		((_x) + 0x07100000)	/* PEGNET 0 */
#define NXPPE_1(_x)		((_x) + 0x07200000)	/* PEGNET 0 */
#define NXPPE_2(_x)		((_x) + 0x07300000)	/* PEGNET 0 */
#define NXPPE_3(_x)		((_x) + 0x07400000)	/* PEGNET 0 */
#define NXPPE_D(_x)		((_x) + 0x07500000)	/* PEGNET D-Cache */
#define NXPPE_I(_x)		((_x) + 0x07600000)	/* PEGNET I-Cache */

/* Window 1 register map */
#define NXPCIE_1(_x)		((_x) + 0x08100000)	/* PCI Express' */
#define NXSW(_x)		((_x) + 0x08200000)	/* Software defined */
#define NXSIR(_x)		((_x) + 0x09200000)	/* 2nd interrupt */
#define NXROMUSB(_x)		((_x) + 0x09300000)	/* ROMUSB */

/* The IMEZ/HMEZ NICs have multiple PCI functions with different registers */
#define NXPCIE_FUNC(_r, _f)	(NXPCIE(_r) + ((_f) * 0x20))

/* Flash layout */
#define NXFLASHMAP_CRBINIT_0	0x00000000	/* CRBINIT */
#define  NXFLASHMAP_CRBINIT_M	0x7fffffff	/* ROM memory barrier */
#define  NXFLASHMAP_CRBINIT_MAX	1023		/* Max CRBINIT entries */
#define NXFLASHMAP_INFO		0x00004000	/* board configuration */
#define NXFLASHMAP_INITCODE	0x00006000	/* chipset-specific code */
#define NXFLASHMAP_BOOTLOADER	0x00010000	/* boot loader */
#define  NXFLASHMAP_BOOTLDSIZE	0x4000		/* boot loader size */
#define NXFLASHMAP_FIRMWARE_0	0x00043000	/* compressed firmware image */
#define NXFLASHMAP_FIRMWARE_1	0x00200000	/* backup firmware image */
#define NXFLASHMAP_PXE		0x003d0000	/* PXE image */
#define NXFLASHMAP_USER		0x003e8000	/* user-specific ares */
#define NXFLASHMAP_VPD		0x003e8c00	/* vendor private data */
#define NXFLASHMAP_LICENSE	0x003e9000	/* firmware license (?) */
#define NXFLASHMAP_CRBINIT_1	0x003f0000	/* backup of CRBINIT */

/*
 * Doorbell messages
 */

/* Register in the doorbell memory region */
#define NXDB			0x00000000
#define  NXDB_PEGID_M		0x00000003	/* Chipset unit */
#define  NXDB_PEGID_S		0
#define   NXDB_PEGID_RX		1		/* Rx unit */
#define   NXDB_PEGID_TX		2		/* Tx unit */
#define  NXDB_PRIVID		0x00000004	/* Must be set */
#define  NXDB_COUNT_M		0x0003fff8	/* Doorbell count */
#define  NXDB_COUNT_S		3
#define  NXDB_CTXID_M		0x0ffc0000	/* Context ID */
#define  NXDB_CTXID_S		18
#define  NXDB_OPCODE_M		0xf0000000	/* Doorbell opcode */
#define  NXDB_OPCODE_S		28
#define   NXDB_OPCODE_RCV_PROD	0
#define   NXDB_OPCODE_JRCV_PROD	1
#define   NXDB_OPCODE_TSO_PROD	2
#define   NXDB_OPCODE_CMD_PROD	3
#define   NXDB_OPCODE_UPD_CONS	4
#define   NXDB_OPCODE_RESET_CTX	5

/*
 * PCI Express Registers
 */

/* Interrupt Vector */
#define NXISR_INT_VECTOR		NXPCIE(0x00010100)
#define  NXISR_INT_VECTOR_TARGET3	(1<<10)	/* interrupt for function 3 */
#define  NXISR_INT_VECTOR_TARGET2	(1<<9)	/* interrupt for function 2 */
#define  NXISR_INT_VECTOR_TARGET1	(1<<8)	/* interrupt for function 1 */
#define  NXISR_INT_VECTOR_TARGET0	(1<<7)	/* interrupt for function 0 */
#define   NXISR_INT_VECTOR_PORT(_n)	(NXISR_INT_VECTOR_TARGET0 << (_n))
#define  NXISR_INT_VECTOR_RC_INT	(1<<5)	/* root complex interrupt */

/* Interrupt Mask */
#define NXISR_INT_MASK			NXPCIE(0x00010104)
#define  NXISR_INT_MASK_TARGET3		(1<<10)	/* mask for function 3 */
#define  NXISR_INT_MASK_TARGET2		(1<<9)	/* mask for function 2 */
#define  NXISR_INT_MASK_TARGET1		(1<<8)	/* mask for function 1 */
#define  NXISR_INT_MASK_TARGET0		(1<<7)	/* mask for function 0 */
#define   NXISR_INT_MASK_PORT(_n)	(NXISR_INT_MASK_TARGET0 << (_n))
#define  NXISR_INT_MASK_RC_INT		(1<<5)	/* root complex mask */
#define NXISR_INT_MASK_ENABLE		0x0000077f
#define NXISR_INT_MASK_DISABLE		0x000007ff

/* Interrupt target mask and status */
#define NXISR_TARGET_STATUS		NXPCIE(0x00010118)
#define NXISR_TARGET_MASK		NXPCIE(0x00010128)
#define  NXISR_TARGET_MASK_ENABLE	0x00000bff
#define  NXISR_TARGET_MASK_DISABLE	0xffffffff

/* Memory windows */
#define NXDDR_WINDOW(_f)		NXPCIE_FUNC(0x00010200, _f)
#define  NXDDR_WINDOW_1			(1<<25)	/* Set this flag for Win 1 */
#define  NXDDR_WINDOW_S			25
#define  NXDDR_WINDOW_M			0x000003ff
#define  NXDDR_WINDOW_SIZE		0x02000000
#define NXQDR_WINDOW(_f)		NXPCIE_FUNC(0x00010208, _f)
#define  NXQDR_WINDOW_1			(1<<25)	/* Set this flag for Win 1 */
#define  NXQDR_WINDOW_S			22
#define  NXQDR_WINDOW_M			0x0000003f
#define  NXQDR_WINDOW_SIZE		0x00400000
#define NXCRB_WINDOW(_f)		NXPCIE_FUNC(0x00010210, _f)
#define  NXCRB_WINDOW_1			(1<<25)	/* Set this flag for Win 1 */
#define  NXCRB_WINDOW_S			25
#define  NXCRB_WINDOW_M			0x00000004
#define  NXCRB_WINDOW_SIZE		0x02000000

/* Lock registers (semaphores between chipset and driver) */
#define NXSEM_FLASH_LOCK	NXPCIE_1(0x0001c010)	/* Flash lock */
#define  NXSEM_FLASH_LOCK_M	0xffffffff
#define  NXSEM_FLASH_LOCKED	(1<<0)			/* R/O: is locked */
#define NXSEM_FLASH_UNLOCK	NXPCIE_1(0x0001c014)	/* Flash unlock */
#define NXSEM_PHY_LOCK		NXPCIE_1(0x0001c018)	/* PHY lock */
#define  NXSEM_PHY_LOCK_M	0xffffffff
#define  NXSEM_PHY_LOCKED	(1<<0)			/* R/O: is locked */
#define NXSEM_PHY_UNLOCK	PXPCIE_1(0x0001c01c)	/* PHY unlock */

/*
 * Network Interface Unit (NIU) registers
 */

/* Mode Register (see also NXNIU_RESET_SYS_FIFOS) */
#define NXNIU_MODE			NXNIU(0x00000000)
#define  NXNIU_MODE_XGE			(1<<2)	/* XGE interface enabled */
#define  NXNIU_MODE_GBE			(1<<1)	/* 4 GbE interfaces enabled */
#define  NXNIU_MODE_FC			(1<<0)	/* *Fibre Channel enabled */
#define NXNIU_MODE_DEF			NUI_XGE_ENABLE

/* 10G - 1G Mode Enable Register */
#define NXNIU_XG_SINGLE_TERM		NXNIU(0x00000004)
#define  NXNIU_XG_SINGLE_TERM_ENABLE	(1<<0)	/* Enable 10G + 1G mode */
#define NXNIU_XG_SINGLE_TERM_DEF	0		/* Disabled */

/* XGE Reset Register */
#define NXNIU_XG_RESET			NXNIU(0x0000001c)
#define  NXNIU_XG_RESET_CD		(1<<1)	/* Reset channels CD */
#define  NXNIU_XG_RESET_AB		(1<<0)	/* Reset channels AB */
#define NXNIU_XG_RESET_DEF		(NXNIU_XG_RESET_AB|NXNIU_XG_RESET_CD)

/* Interrupt Mask Register */
#define NXNIU_INT_MASK			NXNIU(0x00000040)
#define  NXNIU_INT_MASK_XG		(1<<6)	/* XGE Interrupt Mask */
#define  NXNIU_INT_MASK_RES5		(1<<5)	/* Reserved bit */
#define  NXNIU_INT_MASK_RES4		(1<<4)	/* Reserved bit */
#define  NXNIU_INT_MASK_GB3		(1<<3)	/* GbE 3 Interrupt Mask */
#define  NXNIU_INT_MASK_GB2		(1<<2)	/* GbE 2 Interrupt Mask */
#define  NXNIU_INT_MASK_GB1		(1<<1)	/* GbE 1 Interrupt Mask */
#define  NXNIU_INT_MASK_GB0		(1<<0)	/* GbE 0 Interrupt Mask */
#define NXNIU_INT_MASK_DEF		(				\
	NXNIU_INT_MASK_XG|NXNIU_INT_MASK_RES5|NXNIU_INT_MASK_RES4|	\
	NXNIU_INT_MASK_GB3|NXNIU_INT_MASK_GB2|NXNIU_INT_MASK_GB1|	\
	NXNIU_INT_MASK_GB0)			/* Reserved bits enabled */

/* Reset System FIFOs Register (needed before changing NXNIU_MODE) */
#define NXNIU_RESET_SYS_FIFOS		NXNIU(0x00000088)
#define  NXNIU_RESET_SYS_FIFOS_RX	(1<<31)	/* Reset all Rx FIFOs */
#define  NXNIU_RESET_SYS_FIFOS_TX	(1<<0)	/* Reset all Tx FIFOs */
#define NXNIU_RESET_SYS_FIFOS_DEF	0	/* Disabled */

/* Flow control registers */
#define NXNIU_XGE_PAUSE_CONTROL		NXNIU(0x00000098)
#define  NXNIU_XGE_PAUSE_S(_n)		((_n) * 3)
#define  NXNIU_XGE_PAUSE_M		0x00000007
#define  NXNIU_XGE_PAUSE_DISABLED	(1<<0)	/* Tx Pause (always Rx) */
#define  NXNIU_XGE_PAUSE_REQUEST	(1<<1)	/* Request pause */
#define  NXNIU_XGE_PAUSE_ONOFF		(1<<2)	/* Request pause on/off */
#define NXNIU_XGE_PAUSE_LEVEL		NXNIU(0x000000dc)

/*
 * Port-specific NIU registers, will be mapped to a subregion
 */

#define NXNIU_PORT_SIZE			0x00010000
#define NXNIU_PORT(_r, _n)		NXNIU((_r) + (_n) * NXNIU_PORT_SIZE)

#define NXNIU_FC(_n)			NXNIU_PORT(0x00010000, _n)
#define NXNIU_GBE(_n)			NXNIU_PORT(0x00030000, _n)
#define NXNIU_XGE(_n)			NXNIU_PORT(0x00070000, _n)

/* XGE Configuration 0 Register */
#define NX_XGE_CONFIG0			0x0000
#define  NXNIU_XGE_CONFIG0_SOFTRST_FIFO	(1<<31)	/* Soft reset FIFOs */
#define  NXNIU_XGE_CONFIG0_SOFTRST_MAC	(1<<4)	/* Soft reset XGE MAC */
#define  NXNIU_XGE_CONFIG0_RX_ENABLE	(1<<2)	/* Enable frame Rx */
#define  NXNIU_XGE_CONFIG0_TX_ENABLE	(1<<0)	/* Enable frame Tx */
#define NXNIU_XGE_CONFIG0_DEF		0	/* Disabled */

/* XGE Configuration 1 Register */
#define NX_XGE_CONFIG1			0x0004
#define  NXNIU_XGE_CONFIG1_PROMISC	(1<<13)	/* Pass all Rx frames */
#define  NXNIU_XGE_CONFIG1_MCAST_ENABLE	(1<<12) /* Rx all multicast frames */
#define  NXNIU_XGE_CONFIG1_SEQ_ERROR	(1<<10) /* Sequence error detection */
#define  NXNIU_XGE_CONFIG1_NO_PAUSE	(1<<8)	/* Ignore pause frames */
#define  NXNIU_XGE_CONFIG1_LOCALERR	(1<<6)	/* Wire local error */
#define   NXNIU_XGE_CONFIG1_LOCALERR_FE	0	/* Signal with 0xFE */
#define   NXNIU_XGE_CONFIG1_LOCALERR_I	1	/* Signal with Ierr */
#define  NXNIU_XGE_CONFIG1_NO_MAXSIZE	(1<<5)	/* Ignore max Rx size */
#define  NXNIU_XGE_CONFIG1_CRC_TX	(1<<1)	/* Append CRC to Tx frames */
#define  NXNIU_XGE_CONFIG1_CRC_RX	(1<<0)	/* Remove CRC from Rx frames */
#define NXNIU_XGE_CONFIG1_DEF		0	/* Disabled */

/* XGE Station Address (lladdr) Register */
#define NX_XGE_STATION_ADDR_HI		0x000c	/* High lladdr */	
#define NX_XGE_STATION_ADDR_LO		0x0010	/* low lladdr */

/*
 * Software defined registers (used by the firmware or the driver)
 */

/* Chipset state registers */
#define NXSW_ROM_LOCK_ID	NXSW(0x2100)	/* Used for locking the ROM */
#define  NXSW_ROM_LOCK_DRV	0x0d417340	/* Driver ROM lock ID */
#define NXSW_PHY_LOCK_ID	NXSW(0x2120)	/* Used for locking the PHY */
#define  NXSW_PHY_LOCK_DRV	0x44524956	/* Driver PHY lock ID */
#define NXSW_FW_VERSION_MAJOR	NXSW(0x2150)	/* Major f/w version */
#define NXSW_FW_VERSION_MINOR	NXSW(0x2154)	/* Minor f/w version */
#define NXSW_FW_VERSION_BUILD	NXSW(0x2158)	/* Build/Sub f/w version */
#define NXSW_BOOTLD_CONFIG	NXSW(0x21fc)
#define  NXSW_BOOTLD_CONFIG_ROM	0x00000000	/* Load firmware from flasg */
#define  NXSW_BOOTLD_CONFIG_RAM	0x12345678	/* Load firmware from memory */

/* Misc SW registers */
#define NXSW_CMD_PRODUCER_OFF	NXSW(0x2208)	/* Producer CMD ring index */
#define NXSW_CMD_CONSUMER_OFF	NXSW(0x220c)	/* Consumer CMD ring index */
#define NXSW_CMD_ADDR_HI	NXSW(0x2218)	/* CMD ring phys address */
#define NXSW_CMD_ADDR_LO	NXSW(0x221c)	/* CMD ring phys address */
#define NXSW_CMD_RING_SIZE	NXSW(0x22c8)	/* Entries in the CMD ring */
#define NXSW_CMDPEG_STATE	NXSW(0x2250)	/* State of the firmware */
#define  NXSW_CMDPEG_STATE_M	0xffff		/* State mask */
#define  NXSW_CMDPEG_INIT_START	0xff00		/* Start of initialization */
#define  NXSW_CMDPEG_INIT_DONE	0xff01		/* Initialization complete */
#define  NXSW_CMDPEG_INIT_ACK	0xf00f		/* Initialization ACKed */
#define  NXSW_CMDPEG_INIT_FAIL	0xffff		/* Initialization failed */
#define NXSW_GLOBAL_INT_COAL	NXSW(0x2264)	/* Interrupt coalescing */
#define NXSW_INT_COAL_MODE	NXSW(0x2268)	/* Reserved */
#define NXSW_MAX_RCV_BUFS	NXSW(0x226c)	/* Interrupt tuning register */
#define NXSW_TX_INT_THRESHOLD	NXSW(0x2270)	/* Interrupt tuning register */
#define NXSW_RX_PKT_TIMER	NXSW(0x2274)	/* Interrupt tuning register */
#define NXSW_TX_PKT_TIMER	NXSW(0x2278)	/* Interrupt tuning register */
#define NXSW_RX_PKT_CNT		NXSW(0x227c)	/* Rx packet count register */
#define NXSW_RX_TMR_CNT		NXSW(0x2280)	/* Rx timer count register */
#define NXSW_XG_STATE		NXSW(0x2294)	/* PHY state register */
#define  NXSW_XG_LINK_UP	(1<<4)		/* 10G PHY state up */
#define  NXSW_XG_LINK_DOWN	(1<<5)		/* 10G PHY state down */
#define NXSW_MPORT_MODE		NXSW(0x22c4)	/* Multi port mode */
#define  NXSW_MPORT_MODE_M	0xffff		/* Mode mask */
#define  NXSW_MPORT_MODE_1FUNC	0x1111		/* Single function mode */
#define  NXSW_MPORT_MODE_NFUNC	0x2222		/* Multi function mode */

#define NXSW_TEMP		NXSW(0x23b4)	/* Temperature sensor */
#define  NXSW_TEMP_STATE_M	0x0000ffff	/* Temp state mask */
#define  NXSW_TEMP_STATE_S	0		/* Temp state shift */
#define   NXSW_TEMP_STATE_NONE	0x0000		/* Temp state is UNSPEC */
#define   NXSW_TEMP_STATE_OK	0x0001		/* Temp state is OK */
#define   NXSW_TEMP_STATE_WARN	0x0002		/* Temp state is WARNING */
#define   NXSW_TEMP_STATE_CRIT	0x0003		/* Temp state is CRITICAL */
#define  NXSW_TEMP_VAL_M	0xffff0000	/* Temp deg celsius mask */
#define  NXSW_TEMP_VAL_S	16		/* Temp deg celsius shift */
#define NXSW_DRIVER_VER		NXSW(0x24a0)	/* Host driver version */

/*
 * Port-specific SW registers, cannot be mapped to a subregion because
 * they're using different offsets between the registers. Ugh, we have to
 * define a mapping table to avoid a ton of ugly if's in the code.
 */
enum nxsw_portreg {
	NXSW_RCV_PRODUCER_OFF	= 0,		/* Producer Rx ring index */
	NXSW_RCV_CONSUMER_OFF,			/* Consumer Rx ring index */
	NXSW_GLOBALRCV_RING,			/* Address of Rx buffer */
	NXSW_RCV_RING_SIZE,			/* Entries in the Rx ring */

	NXSW_JRCV_PRODUCER_OFF,			/* Producer jumbo ring index */
	NXSW_JRCV_CONSUMER_OFF,			/* Consumer jumbo ring index */
	NXSW_GLOBALJRCV_RING,			/* Address of jumbo buffer */
	NXSW_JRCV_RING_SIZE,			/* Entries in the jumbo ring */

	NXSW_TSO_PRODUCER_OFF,			/* Producer TSO ring index */
	NXSW_TSO_CONSUMER_OFF,			/* Consumer TSO ring index */
	NXSW_GLOBALOTSO_RING,			/* Address of TSO buffer */
	NXSW_TSO_RING_SIZE,			/* Entries in the TSO ring */

	NXSW_STATUS_RING,			/* Address of status ring */
	NXSW_STATUS_PROD,			/* Producer status index */
	NXSW_STATUS_CONS,			/* Consumer status index */
	NXSW_RCVPEG_STATE,			/* State of the NX2031 */
#define  NXSW_RCVPEG_STATE_M	0xffff		/* State mask */
#define  NXSW_RCVPEG_INIT_START	0xff00		/* Start of initialization */
#define  NXSW_RCVPEG_INIT_DONE	0xff01		/* Initialization complete */
#define  NXSW_RCVPEG_INIT_ACK	0xf00f		/* Initialization ACKed */
#define  NXSW_RCVPEG_INIT_FAIL	0xffff		/* Initialization failed */
	NXSW_STATUS_RING_SIZE,			/* Entries in the status ring */

	NXSW_CONTEXT_ADDR_LO,			/* Low address of context */
	NXSW_CONTEXT,				/* Context register */
#define  NXSW_CONTEXT_M		0xffff		/* Context register mask */
#define  NXSW_CONTEXT_SIG	0xdee0		/* Context signature */
#define  NXSW_CONTEXT_RESET	0xbad0		/* Context reset */
	NXSW_CONTEXT_ADDR_HI,			/* High address of context */

	NXSW_PORTREG_MAX
};
#define NXSW_PORTREGS		{					\
	{ NXSW(0x2300), NXSW(0x2344), NXSW(0x23d8), NXSW(0x242c) },	\
	{ NXSW(0x2304), NXSW(0x2348), NXSW(0x23dc), NXSW(0x2430) },	\
	{ NXSW(0x2308), NXSW(0x234c), NXSW(0x23f0), NXSW(0x2434) },	\
	{ NXSW(0x230c), NXSW(0x2350), NXSW(0x23f4), NXSW(0x2438) },	\
									\
	{ NXSW(0x2310), NXSW(0x2354), NXSW(0x23f8), NXSW(0x243c) },	\
	{ NXSW(0x2314), NXSW(0x2358), NXSW(0x23fc), NXSW(0x2440) },	\
	{ NXSW(0x2318), NXSW(0x235c), NXSW(0x2400), NXSW(0x2444) },	\
	{ NXSW(0x231c), NXSW(0x2360), NXSW(0x2404), NXSW(0x2448) },	\
									\
	{ NXSW(0x2320), NXSW(0x2364), NXSW(0x2408), NXSW(0x244c) },	\
	{ NXSW(0x2324), NXSW(0x2368), NXSW(0x240c), NXSW(0x2450) },	\
	{ NXSW(0x2328), NXSW(0x236c), NXSW(0x2410), NXSW(0x2454) },	\
	{ NXSW(0x232c), NXSW(0x2370), NXSW(0x2414), NXSW(0x2458) },	\
									\
	{ NXSW(0x2330), NXSW(0x2374), NXSW(0x2418), NXSW(0x245c) },	\
	{ NXSW(0x2334), NXSW(0x2378), NXSW(0x241c), NXSW(0x2460) },	\
	{ NXSW(0x2338), NXSW(0x237c), NXSW(0x2420), NXSW(0x2464) },	\
	{ NXSW(0x233c), NXSW(0x2380), NXSW(0x2424), NXSW(0x2468) },	\
	{ NXSW(0x2340), NXSW(0x2384), NXSW(0x2428), NXSW(0x246c) },	\
									\
	{ NXSW(0x2388), NXSW(0x2390), NXSW(0x2398), NXSW(0x23a0) },	\
	{ NXSW(0x238c), NXSW(0x2394), NXSW(0x239c), NXSW(0x23a4) },	\
	{ NXSW(0x23c0), NXSW(0x23c4), NXSW(0x23c8), NXSW(0x23cc) }	\
}

/*
 * Port-specific SW registers, will be mapped to a subregion
 */

/*
 * Secondary Interrupt Registers
 */

/* I2Q Register */
#define NXI2Q_CLR_PCI_HI		NXSIR(0x00000034)
#define  NXI2Q_CLR_PCI_HI_PHY		(1<<13)	/* PHY interrupt */
#define NXI2Q_CLR_PCI_HI_DEF		0	/* Cleared */

/*
 * ROMUSB registers
 */

/* Status Register */
#define NXROMUSB_GLB_STATUS		NXROMUSB(0x00000004)	/* ROM Status */
#define  NXROMUSB_GLB_STATUS_DONE	(1<<1)			/* Ready */

/* Reset Unit Register */
#define NXROMUSB_GLB_SW_RESET		NXROMUSB(0x00000008)
#define  NXROMUSB_GLB_SW_RESET_EFC_SIU	(1<<30)	/* EFC_SIU reset */
#define  NXROMUSB_GLB_SW_RESET_NIU	(1<<29)	/* NIU software reset */
#define  NXROMUSB_GLB_SW_RESET_U0QMSQG	(1<<28)	/* Network side QM_SQG reset */
#define  NXROMUSB_GLB_SW_RESET_U1QMSQG	(1<<27)	/* Storage side QM_SQG reset */
#define  NXROMUSB_GLB_SW_RESET_C2C1	(1<<26)	/* Chip to Chip 1 reset */
#define  NXROMUSB_GLB_SW_RESET_C2C0	(1<<25)	/* Chip to Chip 2 reset */
#define  NXROMUSB_GLB_SW_RESET_U1PEGI	(1<<11)	/* Storage Pegasus I-Cache */
#define  NXROMUSB_GLB_SW_RESET_U1PEGD	(1<<10)	/* Storage Pegasus D-Cache */
#define  NXROMUSB_GLB_SW_RESET_U1PEG3	(1<<9)	/* Storage Pegasus3 reset */
#define  NXROMUSB_GLB_SW_RESET_U1PEG2	(1<<8)	/* Storage Pegasus2 reset */
#define  NXROMUSB_GLB_SW_RESET_U1PEG1	(1<<7)	/* Storage Pegasus1 reset */
#define  NXROMUSB_GLB_SW_RESET_U1PEG0	(1<<6)	/* Storage Pegasus0 reset */
#define  NXROMUSB_GLB_SW_RESET_U0PEGI	(1<<11)	/* Network Pegasus I-Cache */
#define  NXROMUSB_GLB_SW_RESET_U0PEGD	(1<<10)	/* Network Pegasus D-Cache */
#define  NXROMUSB_GLB_SW_RESET_U0PEG3	(1<<9)	/* Network Pegasus3 reset */
#define  NXROMUSB_GLB_SW_RESET_U0PEG2	(1<<8)	/* Network Pegasus2 reset */
#define  NXROMUSB_GLB_SW_RESET_U0PEG1	(1<<7)	/* Network Pegasus1 reset */
#define  NXROMUSB_GLB_SW_RESET_U0PEG0	(1<<6)	/* Network Pegasus0 reset */
#define  NXROMUSB_GLB_SW_RESET_PPE	0xf0	/* Protocol Processing Engine */
#define  NXROMUSB_GLB_SW_RESET_XDMA	0x8000ff;
#define NXROMUSB_GLB_SW_RESET_DEF	0xffffffff

/* Casper Reset Register */
#define NXROMUSB_GLB_CAS_RESET		NXROMUSB(0x00000038)
#define  NXROMUSB_GLB_CAS_RESET_ENABLE	(1<<0)	/* Enable Casper reset */
#define  NXROMUSB_GLB_CAS_RESET_DISABLE	0
#define NXROMUSB_GLB_CAS_RESET_DEF	0	/* Disabled */

/* Reset register */
#define NXROMUSB_GLB_PEGTUNE		NXROMUSB(0x0000005c)
#define  NXROMUSB_GLB_PEGTUNE_DONE	(1<<0)

/* Chip clock control register */
#define NXROMUSB_GLB_CHIPCLKCONTROL	NXROMUSB(0x000000a8)
#define  NXROMUSB_GLB_CHIPCLKCONTROL_ON	0x00003fff

/* ROM Register */
#define NXROMUSB_ROM_CONTROL		NXROMUSB(0x00010000)
#define NXROMUSB_ROM_OPCODE		NXROMUSB(0x00010004)
#define  NXROMUSB_ROM_OPCODE_READ	0x0000000b
#define NXROMUSB_ROM_ADDR		NXROMUSB(0x00010008)
#define NXROMUSB_ROM_WDATA		NXROMUSB(0x0001000c)
#define NXROMUSB_ROM_ABYTE_CNT		NXROMUSB(0x00010010)
#define NXROMUSB_ROM_DUMMY_BYTE_CNT	NXROMUSB(0x00010014)
#define NXROMUSB_ROM_RDATA		NXROMUSB(0x00010018)
#define NXROMUSB_ROM_AGT_TAG		NXROMUSB(0x0001001c)
#define NXROMUSB_ROM_TIME_PARM		NXROMUSB(0x00010020)
#define NXROMUSB_ROM_CLK_DIV		NXROMUSB(0x00010024)
#define NXROMUSB_ROM_MISS_INSTR		NXROMUSB(0x00010028)

/*
 * Flash data structures
 */

enum nxb_board_types {
	NXB_BOARDTYPE_P1BD		= 0,
	NXB_BOARDTYPE_P1SB		= 1,
	NXB_BOARDTYPE_P1SMAX		= 2,
	NXB_BOARDTYPE_P1SOCK		= 3,

	NXB_BOARDTYPE_P2SOCK31		= 8,
	NXB_BOARDTYPE_P2SOCK35		= 9,

	NXB_BOARDTYPE_P2SB35_4G		= 10,
	NXB_BOARDTYPE_P2SB31_10G	= 11,
	NXB_BOARDTYPE_P2SB31_2G		= 12,
	NXB_BOARDTYPE_P2SB31_10GIMEZ	= 13,
	NXB_BOARDTYPE_P2SB31_10GHMEZ	= 14,
	NXB_BOARDTYPE_P2SB31_10GCX4	= 15
};

#define NXB_MAX_PORTS	NX_MAX_PORTS		/* max supported ports */

struct nxb_info {
	u_int32_t	ni_hdrver;		/* Board info version */
#define  NXB_VERSION	0x00000001		/* board information version */

	u_int32_t	ni_board_mfg;
	u_int32_t	ni_board_type;
	u_int32_t	ni_board_num;

	u_int32_t	ni_chip_id;
	u_int32_t	ni_chip_minor;
	u_int32_t	ni_chip_major;
	u_int32_t	ni_chip_pkg;
	u_int32_t	ni_chip_lot;

	u_int32_t	ni_port_mask;
	u_int32_t	ni_peg_mask;
	u_int32_t	ni_icache;
	u_int32_t	ni_dcache;
	u_int32_t	ni_casper;

	u_int32_t	ni_lladdr0_low;
	u_int32_t	ni_lladdr1_low;
	u_int32_t	ni_lladdr2_low;
	u_int32_t	ni_lladdr3_low;

	u_int32_t	ni_mnsync_mode;
	u_int32_t	ni_mnsync_shift_cclk;
	u_int32_t	ni_mnsync_shift_mclk;
	u_int32_t	ni_mnwb_enable;
	u_int32_t	ni_mnfreq_crystal;
	u_int32_t	ni_mnfreq_speed;
	u_int32_t	ni_mnorg;
	u_int32_t	ni_mndepth;
	u_int32_t	ni_mnranks0;
	u_int32_t	ni_mnranks1;
	u_int32_t	ni_mnrd_latency0;
	u_int32_t	ni_mnrd_latency1;
	u_int32_t	ni_mnrd_latency2;
	u_int32_t	ni_mnrd_latency3;
	u_int32_t	ni_mnrd_latency4;
	u_int32_t	ni_mnrd_latency5;
	u_int32_t	ni_mnrd_latency6;
	u_int32_t	ni_mnrd_latency7;
	u_int32_t	ni_mnrd_latency8;
	u_int32_t	ni_mndll[18];
	u_int32_t	ni_mnddr_mode;
	u_int32_t	ni_mnddr_extmode;
	u_int32_t	ni_mntiming0;
	u_int32_t	ni_mntiming1;
	u_int32_t	ni_mntiming2;

	u_int32_t	ni_snsync_mode;
	u_int32_t	ni_snpt_mode;
	u_int32_t	ni_snecc_enable;
	u_int32_t	ni_snwb_enable;
	u_int32_t	ni_snfreq_crystal;
	u_int32_t	ni_snfreq_speed;
	u_int32_t	ni_snorg;
	u_int32_t	ni_sndepth;
	u_int32_t	ni_sndll;
	u_int32_t	ni_snrd_latency;

	u_int32_t	ni_lladdr0_high;
	u_int32_t	ni_lladdr1_high;
	u_int32_t	ni_lladdr2_high;
	u_int32_t	ni_lladdr3_high;

	u_int32_t	ni_magic;
#define  NXB_MAGIC	0x12345678		/* magic value */

	u_int32_t	ni_mnrd_imm;
	u_int32_t	ni_mndll_override;
} __packed;

#define NXB_MAX_PORT_LLADDRS	32

struct nxb_imageinfo {
	u_int32_t	nim_bootld_ver;
	u_int32_t	nim_bootld_size;
	u_int32_t	nim_image_ver;
#define  NXB_IMAGE_MAJOR_S	0
#define  NXB_IMAGE_MAJOR_M	0x000000ff
#define  NXB_IMAGE_MINOR_S	8
#define  NXB_IMAGE_MINOR_M	0x0000ff00
#define  NXB_IMAGE_BUILD_S	16
#define  NXB_IMAGE_BUILD_M	0xffff0000
	u_int32_t	nim_image_size;
} __packed;

struct nxb_userinfo {
	u_int8_t		nu_flash_md5[1024];

	struct nxb_imageinfo	nu_image;

	u_int32_t		nu_primary;
	u_int32_t		nu_secondary;
	u_int64_t		nu_lladdr[NXB_MAX_PORTS * NXB_MAX_PORT_LLADDRS];
	u_int32_t		nu_subsys_id;
	u_int8_t		nu_serial_num[32];
	u_int32_t		nu_bios_ver;

	/* Followed by user-specific data */
} __packed;

/* Appended to the on-disk firmware image, values in network byte order */
struct nxb_firmware_header {
	u_int32_t		 fw_hdrver;
#define NX_FIRMWARE_HDRVER	 0	/* version of the firmware header */
	struct nxb_imageinfo	 fw_image;
#define fw_image_ver		 fw_image.nim_image_ver
#define fw_image_size		 fw_image.nim_image_size
#define fw_bootld_ver		 fw_image.nim_bootld_ver
#define fw_bootld_size		 fw_image.nim_bootld_size
} __packed;

#endif /* _NX_REG_H */
