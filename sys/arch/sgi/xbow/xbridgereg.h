/*	$OpenBSD: xbridgereg.h,v 1.11 2009/12/26 20:16:19 miod Exp $	*/

/*
 * Copyright (c) 2008, 2009 Miodrag Vallat.
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
 * IP27/IP30/IP35 Bridge and XBridge Registers
 * IP35 PIC Registers
 */

#define	BRIDGE_REGISTERS_SIZE		0x00030000
#define	BRIDGE_BUS_OFFSET		0x00800000

#define	BRIDGE_NBUSES			1
#define	PIC_NBUSES			2

#define	BRIDGE_NSLOTS			8
#define	PIC_NSLOTS			4
#define	MAX_SLOTS			BRIDGE_NSLOTS

#define	BRIDGE_NINTRS			8

#define	PIC_WIDGET_STATUS_PCIX_SPEED_MASK	0x0000000c00000000UL
#define	PIC_WIDGET_STATUS_PCIX_SPEED_SHIFT	34
#define	PIC_WIDGET_STATUS_PCIX_MODE		0x0000000200000000UL

#define	BRIDGE_WIDGET_CONTROL_IO_SWAP		0x00800000
#define	BRIDGE_WIDGET_CONTROL_MEM_SWAP		0x00400000
#define	BRIDGE_WIDGET_CONTROL_LARGE_PAGES	0x00200000
#define	BRIDGE_WIDGET_CONTROL_SPEED_MASK	0x00000030
#define	BRIDGE_WIDGET_CONTROL_SPEED_SHIFT	4

/* Response Buffer Address */
#define	BRIDGE_WIDGET_RESP_UPPER	0x00000060
#define	BRIDGE_WIDGET_RESP_LOWER	0x00000068

/*
 * DMA Direct Window
 *
 * The direct map register allows the 2GB direct window to map to
 * a given widget address space. The upper bits of the XIO address,
 * identifying the node to access, are provided in the low-order
 * bits of the register.
 */

#define	BRIDGE_DIR_MAP			0x00000080

#define	BRIDGE_DIRMAP_WIDGET_SHIFT	20
#define	BRIDGE_DIRMAP_ADD_512MB		0x00020000	/* add 512MB */
#define	BRIDGE_DIRMAP_BASE_MASK		0x00001fff
#define	BRIDGE_DIRMAP_BASE_SHIFT	31

#define	BRIDGE_PCI0_MEM_SPACE_BASE	0x0000000040000000ULL
#define	BRIDGE_PCI_MEM_SPACE_LENGTH	0x0000000040000000ULL
#define	BRIDGE_PCI1_MEM_SPACE_BASE	0x00000000c0000000ULL
#define	BRIDGE_PCI_IO_SPACE_BASE	0x0000000100000000ULL
#define	BRIDGE_PCI_IO_SPACE_LENGTH	0x0000000100000000ULL

#define	BRIDGE_NIC			0x000000b0

#define	BRIDGE_BUS_TIMEOUT		0x000000c0
#define	BRIDGE_BUS_PCI_RETRY_CNT_SHIFT		0
#define	BRIDGE_BUS_PCI_RETRY_CNT_MASK	0x000003ff
#define	BRIDGE_BUS_GIO_TIMEOUT		0x00001000
#define	BRIDGE_BUS_PCI_RETRY_HOLD_SHIFT		16
#define	BRIDGE_BUS_PCI_RETRY_HOLD_MASK	0x001f0000

#define	BRIDGE_PCI_CFG			0x000000c8
#define	BRIDGE_PCI_ERR_UPPER		0x000000d0
#define	BRIDGE_PCI_ERR_LOWER		0x000000d8

/*
 * Interrupt handling
 */

#define	BRIDGE_ISR			0x00000100
#define	BRIDGE_IER			0x00000108
#define	BRIDGE_ICR			0x00000110
#define	BRIDGE_INT_MODE			0x00000118
#define	BRIDGE_INT_DEV			0x00000120
#define	BRIDGE_INT_HOST_ERR		0x00000128
#define	BRIDGE_INT_ADDR(d)		(0x00000130 + 8 * (d))
/* the following two are XBridge-only */
#define	BRIDGE_INT_FORCE_ALWAYS(d)	(0x00000180 + 8 * (d))
#define	BRIDGE_INT_FORCE_PIN(d)		(0x000001c0 + 8 * (d))

/*
 * BRIDGE_ISR bits (bits 32 and beyond are PIC only)
 */

/* PCI-X split completion message parity error */
#define	BRIDGE_ISR_PCIX_SPLIT_MSG_PARITY	0x0000200000000000ULL
/* PCI-X split completion error message */
#define	BRIDGE_ISR_PCIX_SPLIT_EMSG		0x0000100000000000ULL
/* PCI-X split completion timeout */
#define	BRIDGE_ISR_PCIX_SPLIT_TO		0x0000080000000000ULL
/* PCI-X unexpected completion cycle */
#define	BRIDGE_ISR_PCIX_UNEX_COMP		0x0000040000000000ULL
/* internal RAM parity error */
#define	BRIDGE_ISR_INT_RAM_PERR			0x0000020000000000ULL
/* PCI/PCI-X arbitration error */
#define	BRIDGE_ISR_PCIX_ARB_ERR			0x0000010000000000ULL
/* PCI-X read request timeout */
#define	BRIDGE_ISR_PCIX_REQ_TMO			0x0000008000000000ULL
/* PCI-X target abort */
#define	BRIDGE_ISR_PCIX_TABORT			0x0000004000000000ULL
/* PCI-X PERR */
#define	BRIDGE_ISR_PCIX_PERR			0x0000002000000000ULL
/* PCI-X SERR */
#define	BRIDGE_ISR_PCIX_SERR			0x0000001000000000ULL
/* PCI-X PIO retry counter exceeded */
#define	BRIDGE_ISR_PCIX_MRETRY			0x0000000800000000ULL
/* PCI-X master timeout */
#define	BRIDGE_ISR_PCIX_MTMO			0x0000000400000000ULL
/* PCI-X data cycle parity error */
#define	BRIDGE_ISR_PCIX_D_PARITY		0x0000000200000000ULL
/* PCI-X address or attribute cycle parity error */
#define	BRIDGE_ISR_PCIX_A_PARITY		0x0000000100000000ULL
/* multiple errors occured - bridge only */
#define	BRIDGE_ISR_MULTIPLE_ERR			0x0000000080000000ULL
/* PMU access fault */
#define	BRIDGE_ISR_PMU_ESIZE_FAULT		0x0000000040000000ULL
/* unexpected xtalk incoming response */
#define	BRIDGE_ISR_UNEXPECTED_RESP		0x0000000020000000ULL
/* xtalk incoming response framing error */
#define	BRIDGE_ISR_BAD_XRESP_PACKET		0x0000000010000000ULL
/* xtalk incoming request framing error */
#define	BRIDGE_ISR_BAD_XREQ_PACKET		0x0000000008000000ULL
/* xtalk incoming response command word error bit set */
#define	BRIDGE_ISR_RESP_XTALK_ERR		0x0000000004000000ULL
/* xtalk incoming request command word error bit set */
#define	BRIDGE_ISR_REQ_XTALK_ERR		0x0000000002000000ULL
/* request packet has invalid address for this widget */
#define	BRIDGE_ISR_INVALID_ADDRESS		0x0000000001000000ULL
/* request operation not supported by the bridge */
#define	BRIDGE_ISR_UNSUPPORTED_XOP		0x0000000000800000ULL
/* request packet overflow */
#define	BRIDGE_ISR_XREQ_FIFO_OFLOW		0x0000000000400000ULL
/* LLP receiver sequence number error */
#define	BRIDGE_ISR_LLP_REC_SNERR		0x0000000000200000ULL
/* LLP receiver check bit error */
#define	BRIDGE_ISR_LLP_REC_CBERR		0x0000000000100000ULL
/* LLP receiver retry count exceeded */
#define	BRIDGE_ISR_LLP_RCTY			0x0000000000080000ULL
/* LLP transmitter side required retry */
#define	BRIDGE_ISR_LLP_TX_RETRY			0x0000000000040000ULL
/* LLP transmitter retry count exceeded */
#define	BRIDGE_ISR_LLP_TCTY			0x0000000000020000ULL
/* (ATE) SSRAM parity error - bridge only */
#define	BRIDGE_ISR_SSRAM_PERR			0x0000000000010000ULL
/* PCI abort condition */
#define	BRIDGE_ISR_PCI_ABORT			0x0000000000008000ULL
/* PCI bridge detected parity error */
#define	BRIDGE_ISR_PCI_PARITY			0x0000000000004000ULL
/* PCI address or command parity error */
#define	BRIDGE_ISR_PCI_SERR			0x0000000000002000ULL
/* PCI device parity error */
#define	BRIDGE_ISR_PCI_PERR			0x0000000000001000ULL
/* PCI device selection timeout */
#define	BRIDGE_ISR_PCI_MASTER_TMO		0x0000000000000800ULL
/* PCI retry count exceeded */
#define	BRIDGE_ISR_PCI_RETRY_CNT		0x0000000000000400ULL
/* PCI to xtalk read request timeout */
#define	BRIDGE_ISR_XREAD_REQ_TMO		0x0000000000000200ULL
/* GIO non-contiguous byte enable in xtalk packet - bridge only */
#define	BRIDGE_ISR_GIO_BENABLE_ERR		0x0000000000000100ULL
#define	BRIDGE_ISR_HWINTR_MASK			0x00000000000000ffULL

#define	BRIDGE_ISR_ERRMASK			0x00000000fffffe00ULL
#define	PIC_ISR_ERRMASK				0x00003fff7ffffe00ULL

/*
 * BRIDGE_ICR bits, for Bridge and XBridge chips only (error interrupts
 * being cleared in groups)
 */

#define	BRIDGE_ICR_MULTIPLE		0x00000040
#define	BRIDGE_ICR_CRP			0x00000020
#define	BRIDGE_ICR_RESP_BUF		0x00000010
#define	BRIDGE_ICR_REQ_DSP		0x00000008
#define	BRIDGE_ICR_LLP			0x00000004
#define	BRIDGE_ICR_SSRAM		0x00000002
#define	BRIDGE_ICR_PCI			0x00000001
#define	BRIDGE_ICR_ALL			0x0000007f

/*
 * PCI Resource Mapping control
 *
 * There are three ways to map a given device:
 * - memory mapping in the long window, at BRIDGE_PCI_MEM_SPACE_BASE,
 *   shared by all devices.
 * - I/O mapping in the long window, at BRIDGE_PCI_IO_SPACE_BASE,
 *   shared by all devices, but only on widget revision 4 or later.
 * - programmable memory or I/O mapping at a selectable place in the
 *   short window, with an 1MB granularity. The size of this
 *   window is 2MB for the windows at 2MB and 4MB, and 1MB onwards.
 *
 * ARCBios will setup mappings in the short window for us, and
 * the selected address will match BAR0.
 */

#define	BRIDGE_DEVICE(d)		(0x00000200 + 8 * (d))
/* flags applying to the device itself */
/* byteswap DMA done through ATE */
#define	BRIDGE_DEVICE_SWAP_PMU			0x00100000
/* byteswap DMA done through the direct window */
#define	BRIDGE_DEVICE_SWAP_DIR			0x00080000
/* flags applying to the mapping in this devio register */
#define	BRIDGE_DEVICE_PREFETCH			0x00040000
#define	BRIDGE_DEVICE_PRECISE			0x00020000
#define	BRIDGE_DEVICE_COHERENT			0x00010000
#define	BRIDGE_DEVICE_BARRIER			0x00008000
/* byteswap PIO */
#define	BRIDGE_DEVICE_SWAP			0x00002000
/* set if memory space, clear if I/O space */
#define	BRIDGE_DEVICE_IO_MEM			0x00001000
#define	BRIDGE_DEVICE_BASE_MASK			0x00000fff
#define	BRIDGE_DEVICE_BASE_SHIFT		20

#define	BRIDGE_DEVIO_BASE			0x00200000
#define	BRIDGE_DEVIO_LARGE			0x00200000
#define	BRIDGE_DEVIO_SHORT			0x00100000

#define	BRIDGE_DEVIO_OFFS(d) \
	(BRIDGE_DEVIO_BASE + \
	 BRIDGE_DEVIO_LARGE * ((d) < 2 ? (d) : 2) + \
	 BRIDGE_DEVIO_SHORT * ((d) < 2 ? 0 : (d) - 2))
#define	BRIDGE_DEVIO_SIZE(d) \
	((d) < 2 ? BRIDGE_DEVIO_LARGE : BRIDGE_DEVIO_SHORT)
#define	PIC_DEVIO_OFFS(bus,d) \
	(BRIDGE_DEVIO_OFFS(d) + ((bus) != 0 ? BRIDGE_BUS_OFFSET : 0))


#define	BRIDGE_DEVICE_WBFLUSH(d)	(0x00000240 + 8 * (d))

/*
 * Read Response Buffer configuration registers
 *
 * There are 16 RRB, which are shared among the PCI devices.
 * The following registers provide four bits per RRB, describing
 * their RRB assignment.
 *
 * Since these four bits only assign two bits to map to a PCI slot,
 * the low-order bit is implied by the RRB register: one controls the
 * even-numbered PCI slots, while the other controls the odd-numbered
 * PCI slots.
 */

#define	BRIDGE_RRB_EVEN			0x00000280
#define	BRIDGE_RRB_ODD			0x00000288

#define	RRB_VALID			0x8
#define	RRB_VCHAN			0x4
#define	RRB_DEVICE_MASK			0x3
#define	RRB_SHIFT			4

/*
 * Address Translation Entries
 */

#define	BRIDGE_INTERNAL_ATE		128
#define	XBRIDGE_INTERNAL_ATE		1024

#define	BRIDGE_ATE_SSHIFT		12	/* 4KB */
#define	BRIDGE_ATE_LSHIFT		14	/* 16KB */
#define	BRIDGE_ATE_SSIZE		(1ULL << BRIDGE_ATE_SSHIFT)
#define	BRIDGE_ATE_LSIZE		(1ULL << BRIDGE_ATE_LSHIFT)
#define	BRIDGE_ATE_SMASK		(BRIDGE_ATE_SSIZE - 1)
#define	BRIDGE_ATE_LMASK		(BRIDGE_ATE_LSIZE - 1)

#define	BRIDGE_ATE(a)			(0x00010000 + (a) * 8)

#define	ATE_NV				0x0000000000000000ULL
#define	ATE_V				0x0000000000000001ULL
#define	ATE_COH				0x0000000000000002ULL
#define	ATE_PRECISE			0x0000000000000004ULL
#define	ATE_PREFETCH			0x0000000000000008ULL
#define	ATE_BARRIER			0x0000000000000010ULL
#define	ATE_BSWAP			0x0000000000000020ULL	/* XBridge */
#define	ATE_WIDGET_MASK			0x0000000000000f00ULL
#define	ATE_WIDGET_SHIFT		8
#define	ATE_ADDRESS_MASK		0x0000fffffffff000ULL
#define	ATE_RMF_MASK			0x00ff000000000000ULL	/* Bridge */

/*
 * Configuration space
 *
 * Access to the first bus is done in the first area, sorted by
 * device number and function number.
 * Access to other buses is done in the second area, after programming
 * BRIDGE_PCI_CFG to the appropriate bus and slot number.
 */

#define	BRIDGE_PCI_CFG_SPACE		0x00020000
#define	BRIDGE_PCI_CFG1_SPACE		0x00028000

/*
 * DMA addresses
 * The Bridge can do DMA either through a direct 2GB window, or through
 * a 1GB translated window, using its ATE memory.
 */

#define	BRIDGE_DMA_TRANSLATED_BASE	0x40000000ULL
#define	XBRIDGE_DMA_TRANSLATED_SWAP	0x20000000ULL
#define	ATE_ADDRESS(a,s) \
		(BRIDGE_DMA_TRANSLATED_BASE + ((a) << (s)))
#define	ATE_INDEX(a,s) \
		(((a) - BRIDGE_DMA_TRANSLATED_BASE) >> (s))

#define	BRIDGE_DMA_DIRECT_BASE		0x80000000ULL
#define	BRIDGE_DMA_DIRECT_LENGTH	0x80000000ULL
