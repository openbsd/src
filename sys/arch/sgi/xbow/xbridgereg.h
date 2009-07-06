/*	$OpenBSD: xbridgereg.h,v 1.7 2009/07/06 22:46:43 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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
 * IP27/IP30 Bridge Registers
 */

#define	BRIDGE_REGISTERS_SIZE		0x00030000
#define	BRIDGE_NSLOTS			8
#define	BRIDGE_NINTRS			8

#define	BRIDGE_WIDGET_CONTROL_IO_SWAP		0x00800000
#define	BRIDGE_WIDGET_CONTROL_MEM_SWAP		0x00400000
#define	BRIDGE_WIDGET_CONTROL_LARGE_PAGES	0x00200000

/*
 * DMA Direct Window
 *
 * The direct map register allows the 2GB direct window to map to
 * a given widget address space. The upper bits of the XIO address,
 * identifying the node to access, are provided in the low-order
 * bits of the register.
 */

#define	BRIDGE_DIR_MAP			0x00000084

#define	BRIDGE_DIRMAP_WIDGET_SHIFT	20
#define	BRIDGE_DIRMAP_ADD_512MB		0x00020000	/* add 512MB */
#define	BRIDGE_DIRMAP_BASE_MASK		0x00001fff
#define	BRIDGE_DIRMAP_BASE_SHIFT	31

#define	BRIDGE_PCI_MEM_SPACE_BASE	0x0000000040000000ULL
#define	BRIDGE_PCI_MEM_SPACE_LENGTH	0x0000000040000000ULL
#define	BRIDGE_PCI_IO_SPACE_BASE	0x0000000100000000ULL
#define	BRIDGE_PCI_IO_SPACE_LENGTH	0x0000000100000000ULL

#define	BRIDGE_NIC			0x000000b4
#define	BRIDGE_BUS_TIMEOUT		0x000000c4
#define	BRIDGE_PCI_CFG			0x000000cc
#define	BRIDGE_PCI_ERR_UPPER		0x000000d4
#define	BRIDGE_PCI_ERR_LOWER		0x000000dc

/*
 * Interrupt handling
 */

#define	BRIDGE_ISR			0x00000104
#define	BRIDGE_IER			0x0000010c
#define	BRIDGE_ICR			0x00000114
#define	BRIDGE_INT_MODE			0x0000011c
#define	BRIDGE_INT_DEV			0x00000124
#define	BRIDGE_INT_HOST_ERR		0x0000012c
#define	BRIDGE_INT_ADDR(d)		(0x00000134 + 8 * (d))
/* the following two are XBridge-only */
#define	BRIDGE_INT_FORCE_ALWAYS(d)	(0x00000184 + 8 * (d))
#define	BRIDGE_INT_FORCE_PIN(d)		(0x000001c4 + 8 * (d))

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

#define	BRIDGE_DEVICE(d)		(0x00000204 + 8 * (d))
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


#define	BRIDGE_DEVICE_WBFLUSH(d)	(0x00000244 + 8 * (d))

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

#define	BRIDGE_RRB_EVEN			0x00000284
#define	BRIDGE_RRB_ODD			0x0000028c

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
