/*	$OpenBSD: xbridgereg.h,v 1.1 2008/04/07 22:47:40 miod Exp $	*/

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

#define	BRIDGE_DIR_MAP			0x00000084
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

/*
 * Mapping control
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
#define	BRIDGE_DEVICE_SWAP			0x00002000
#define	BRIDGE_DEVICE_IO			0x00001000
#define	BRIDGE_DEVICE_BASE_MASK			0x00000fff
#define	BRIDGE_DEVICE_BASE_SHIFT		20

#define	BRIDGE_PCI_MEM_SPACE_BASE	0x0000000040000000ULL
#define	BRIDGE_PCI_MEM_SPACE_LENGTH	0x0000000040000000ULL
#define	BRIDGE_PCI_IO_SPACE_BASE	0x0000000100000000ULL
#define	BRIDGE_PCI_IO_SPACE_LENGTH	0x0000000100000000ULL

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

#define	BRIDGE_DMA_DIRECT_BASE		0x80000000ULL
#define	BRIDGE_DMA_DIRECT_LENGTH	0x80000000ULL
