/*	$OpenBSD: iocreg.h,v 1.10 2009/11/18 19:03:27 miod Exp $	*/

/*
 * Copyright (c) 2008 Joel Sing.
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
 * Register definitions for SGI IOC3 ASIC.
 */

#define IOC_NDEVS		6

#define IOCDEV_SERIAL_A		0
#define IOCDEV_SERIAL_B		1
#define IOCDEV_LPT		2
#define IOCDEV_KBC		3
#define IOCDEV_RTC		4
#define IOCDEV_EF		5

/* SuperIO registers */
#define IOC3_SIO_IR		0x0000001c	/* SIO interrupt register */
#define IOC3_SIO_IES		0x00000020	/* SIO interrupt enable */
#define IOC3_SIO_IEC		0x00000024	/* SIO interrupt disable */
#define IOC3_SIO_CR		0x00000028	/* SIO control register */
#define	IOC3_MCR		0x00000030	/* MicroLan control register */

/* GPIO registers */
#define	IOC3_GPCR_S		0x00000034	/* GPIO control bit set */
#define	IOC3_GPCR_C		0x00000038	/* GPIO control bit clear */
#define	IOC3_GPDR		0x0000003c	/* GPIO data */
#define	IOC3_GPPR_BASE		0x00000040	/* 9 GPIO pin registers */
#define	IOC3_GPPR(x)		(IOC3_GPPR_BASE + (x) * 4)

/* Keyboard controller registers. */
#define	IOC3_KBC_CTRL_STATUS	0x0000009c
#define	IOC3_KBC_KBD_RX		0x000000a0
#define	IOC3_KBC_AUX_RX		0x000000a4
#define	IOC3_KBC_KBD_TX		0x000000a8
#define	IOC3_KBC_AUX_TX		0x000000ac

/* Non-16550 mode UART registers */
#define	IOC3_UARTA_SSCR		0x000000b8	/* control register */
#define	IOC3_UARTA_STPIR	0x000000bc	/* TX producer index register */
#define	IOC3_UARTA_STCIR	0x000000c0	/* TX consumer index register */
#define	IOC3_UARTA_SRPIR	0x000000c4	/* RX producer index register */
#define	IOC3_UARTA_SRCIR	0x000000c8	/* RX consumer index register */
#define	IOC3_UARTA_SRTR		0x000000cc	/* receive timer register */
#define	IOC3_UARTA_SHADOW	0x000000d0	/* 16550 shadow register */

#define	IOC3_UARTB_SSCR		0x000000d4
#define	IOC3_UARTB_STPIR	0x000000d8
#define	IOC3_UARTB_STCIR	0x000000dc
#define	IOC3_UARTB_SRPIR	0x000000e0
#define	IOC3_UARTB_SRCIR	0x000000e4
#define	IOC3_UARTB_SRTR		0x000000e8
#define	IOC3_UARTB_SHADOW	0x000000ec

/* Ethernet registers */
#define	IOC3_ENET_MCR		0x000000f0	/* Master Control Register */
#define	IOC3_ENET_ISR		0x000000f4	/* Interrupt Status Register */
#define	IOC3_ENET_IER		0x000000f8	/* Interrupt Enable Register */
#define	IOC3_ENET_RCSR		0x000000fc	/* RX Control and Status Reg. */
#define	IOC3_ENET_RBR_H		0x00000100	/* RX Base Register */
#define	IOC3_ENET_RBR_L		0x00000104
#define	IOC3_ENET_RBAR		0x00000108	/* RX Barrier Register */
#define	IOC3_ENET_RCIR		0x0000010c	/* RX Consumer Index Register */
#define	IOC3_ENET_RPIR		0x00000110	/* RX Producer Index Register */
#define	IOC3_ENET_RTR		0x00000114	/* RX Timer Register */
#define	IOC3_ENET_TCSR		0x00000118	/* TX Control and Status Reg. */
#define	IOC3_ENET_RSR		0x0000011c	/* Random Seed Register */
#define	IOC3_ENET_TCDC		0x00000120	/* TX Collision Detect Counter */
#define	IOC3_ENET_BIR		0x00000124
#define	IOC3_ENET_TBR_H		0x00000128	/* TX Base Register */
#define	IOC3_ENET_TBR_L		0x0000012c
#define	IOC3_ENET_TCIR		0x00000130	/* TX Consumer Index Register */
#define	IOC3_ENET_TPIR		0x00000134	/* TX Producer Index Register */
#define	IOC3_ENET_MAR_H		0x00000138	/* MAC Address Register */
#define	IOC3_ENET_MAR_L		0x0000013c
#define	IOC3_ENET_HAR_H		0x00000140	/* Hash filter Address Reg. */
#define	IOC3_ENET_HAR_L		0x00000144
#define	IOC3_ENET_MICR		0x00000148	/* MII Control Register */
#define	IOC3_ENET_MIDR_R	0x0000014c	/* MII Data Register (read) */
#define	IOC3_ENET_MIDR_W	0x00000150	/* MII Data Register (write) */

/* bits in the SIO interrupt register */
#define	IOC3_IRQ_UARTA		0x00000040	/* UART A passthrough */
#define	IOC3_IRQ_UARTB		0x00008000	/* UART B passthrough */
#define	IOC3_IRQ_LPT		0x00040000	/* parallel port passthrough */
#define	IOC3_IRQ_KBC		0x00400000	/* keyboard controller */

/* bits in GPCR */
#define	IOC3_GPCR_UARTA_PIO	0x00000040	/* UARTA in PIO mode */
#define	IOC3_GPCR_UARTB_PIO	0x00000080	/* UARTB in PIO mode */
#define	IOC3_GPCR_MLAN		0x00200000	/* MicroLan enable */

/* bits in SSCR */
#define	IOC3_SSCR_RESET		0x80000000

/* bits in ENET_MCR */
#define	IOC3_ENET_MCR_DUPLEX		0x00000001
#define	IOC3_ENET_MCR_PROMISC		0x00000002
#define	IOC3_ENET_MCR_PADEN		0x00000004
#define	IOC3_ENET_MCR_RXOFF_MASK	0x000001f8
#define	IOC3_ENET_MCR_RXOFF_SHIFT	3
#define	IOC3_ENET_MCR_PARITY_ENABLE	0x00000200
#define	IOC3_ENET_MCR_LARGE_SSRAM	0x00001000
#define	IOC3_ENET_MCR_TX_DMA		0x00002000
#define	IOC3_ENET_MCR_TX		0x00004000
#define	IOC3_ENET_MCR_RX_DMA		0x00008000
#define	IOC3_ENET_MCR_RX		0x00010000
#define	IOC3_ENET_MCR_LOOPBACK		0x00020000
#define	IOC3_ENET_MCR_RESET		0x80000000

/* bits in the ENET interrupt register */
#define	IOC3_ENET_ISR_RX_TIMER		0x00000001	/* RX periodic int */
#define	IOC3_ENET_ISR_RX_THRESHOLD	0x00000002	/* RX q above threshold */
#define	IOC3_ENET_ISR_RX_OFLOW		0x00000004	/* RX q overflow */
#define	IOC3_ENET_ISR_RX_BUF_OFLOW	0x00000008	/* RX buffer overflow */
#define	IOC3_ENET_ISR_RX_MEMORY		0x00000010	/* RX memory acc. err */
#define	IOC3_ENET_ISR_RX_PARITY		0x00000020	/* RX parity error */
#define	IOC3_ENET_ISR_TX_EMPTY		0x00010000	/* TX q empty */
#define	IOC3_ENET_ISR_TX_RETRY		0x00020000
#define	IOC3_ENET_ISR_TX_EXDEF		0x00040000
#define	IOC3_ENET_ISR_TX_LCOLL		0x00080000	/* TX Late Collision */
#define	IOC3_ENET_ISR_TX_GIANT		0x00100000
#define	IOC3_ENET_ISR_TX_BUF_UFLOW	0x00200000	/* TX buf underflow */
#define	IOC3_ENET_ISR_TX_EXPLICIT	0x00400000	/* TX int requested */
#define	IOC3_ENET_ISR_TX_COLL_WRAP	0x00800000
#define	IOC3_ENET_ISR_TX_DEFER_WRAP	0x01000000
#define	IOC3_ENET_ISR_TX_MEMORY		0x02000000	/* TX memory acc. err */
#define	IOC3_ENET_ISR_TX_PARITY		0x04000000	/* TX parity error */

#define	IOC3_ENET_ISR_TX_ALL		0x07ff0000	/* all TX bits */

/* bits in ENET_RCSR */
#define	IOC3_ENET_RCSR_THRESHOLD_MASK	0x000001ff

/* bits in ENET_RPIR */
#define	IOC3_ENET_PIR_SET		0x80000000	/* set new value */

/* bits in ENET_TCSR */
#define	IOC3_ENET_TCSR_IPGT_MASK	0x0000007f	/* interpacket gap */
#define	IOC3_ENET_TCSR_IPGT_SHIFT	0
#define	IOC3_ENET_TCSR_IPGR1_MASK	0x00007f00
#define	IOC3_ENET_TCSR_IPGR1_SHIFT	8
#define	IOC3_ENET_TCSR_IPGR2_MASK	0x007f0000
#define	IOC3_ENET_TCSR_IPGR2_SHIFT	16
#define	IOC3_ENET_TCSR_NOTXCLOCK	0x80000000
#define	IOC3_ENET_TCSR_FULL_DUPLEX \
	((17 << IOC3_ENET_TCSR_IPGR2_SHIFT) | \
	 (11 << IOC3_ENET_TCSR_IPGR1_SHIFT) | \
	 (21 << IOC3_ENET_TCSR_IPGT_SHIFT))
#define	IOC3_ENET_TCSR_HALF_DUPLEX \
	((21 << IOC3_ENET_TCSR_IPGR2_SHIFT) | \
	 (21 << IOC3_ENET_TCSR_IPGR1_SHIFT) | \
	 (21 << IOC3_ENET_TCSR_IPGT_SHIFT))

/* bits in ENET_TCDC */
#define	IOC3_ENET_TCDC_COLLISION_MASK	0x0000ffff
#define	IOC3_ENET_TCDC_DEFER_MASK	0xffff0000
#define	IOC3_ENET_TCDC_DEFER_SHIFT	16

/* bits in ENET_TCIR */
#define	IOC3_ENET_TCIR_IDLE		0x80000000

/* bits in ENET_MICR */
#define	IOC3_ENET_MICR_REG_MASK		0x0000001f
#define	IOC3_ENET_MICR_PHY_MASK		0x000003e0
#define	IOC3_ENET_MICR_PHY_SHIFT	5
#define	IOC3_ENET_MICR_READ		0x00000400
#define	IOC3_ENET_MICR_WRITE		0x00000000
#define	IOC3_ENET_MICR_BUSY		0x00000800

#define	IOC3_ENET_MIDR_MASK		0x0000ffff

/*
 * Offsets and sizes for subdevices handled by mi drivers.
 */

#define	IOC3_UARTA_BASE		0x00020178
#define	IOC3_UARTB_BASE		0x00020170

/*
 * Ethernet SSRAM.
 */

#define	IOC3_SSRAM_BASE		0x00040000
#define	IOC3_SSRAM_SMALL_SIZE	0x00020000
#define	IOC3_SSRAM_LARGE_SIZE	0x00040000

#define	IOC3_SSRAM_PARITY_BIT		0x00010000
#define	IOC3_SSRAM_DATA_MASK		0x0000ffff

/*
 * Offsets of devices connected to the four IOC3 `bytebus'.
 */

#define	IOC3_BYTEBUS_0		0x00080000
#define	IOC3_BYTEBUS_1		0x000a0000
#define	IOC3_BYTEBUS_2		0x000c0000
#define	IOC3_BYTEBUS_3		0x000e0000
