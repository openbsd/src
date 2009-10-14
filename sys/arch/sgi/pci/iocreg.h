/*	$OpenBSD: iocreg.h,v 1.4 2009/10/14 20:21:14 miod Exp $	*/

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

#define	IOC3_LPT_BASE		0x00000080
#define	IOC3_LPT_SIZE		0x0000001c

#define	IOC3_KBC_BASE		0x0000009c
#define	IOC3_KBC_SIZE		0x00000014

#define	IOC3_EF_BASE		0x000000f0
#define	IOC3_EF_SIZE		0x00000060

#define	IOC3_RTC_BASE		0x00020168

#define	IOC3_UARTA_BASE		0x00020178
#define	IOC3_UARTB_BASE		0x00020170

#define	IOC3_BYTEBUS_0		0x00080000
#define	IOC3_BYTEBUS_1		0x000a0000
#define	IOC3_BYTEBUS_2		0x000c0000
#define	IOC3_BYTEBUS_3		0x000e0000
