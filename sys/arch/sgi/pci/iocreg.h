/*	$OpenBSD: iocreg.h,v 1.3 2009/08/18 19:32:47 miod Exp $	*/

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

#define IOC3_SIO_IR		0x0000001c
#define IOC3_SIO_IES		0x00000020
#define IOC3_SIO_IEC		0x00000024
#define IOC3_SIO_CR		0x00000028
#define	IOC3_MCR		0x00000030

/* bits in the SIO interrupt register */
#define	IOC3_IRQ_UARTA		0x00000040	/* UART A passthrough */
#define	IOC3_IRQ_UARTB		0x00008000	/* UART B passthrough */
#define	IOC3_IRQ_LPT		0x00040000	/* parallel port passthrough */
#define	IOC3_IRQ_KBC		0x00400000	/* keyboard controller */

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
