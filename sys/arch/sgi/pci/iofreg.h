/*	$OpenBSD: iofreg.h,v 1.1 2009/08/18 19:34:17 miod Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
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
 * Register definitions for SGI IOC4 ASIC.
 */

#define IOC4_NDEVS		7

#define IOC4DEV_SERIAL_A	0
#define IOC4DEV_SERIAL_B	1
#define IOC4DEV_SERIAL_C	3
#define IOC4DEV_SERIAL_D	4
#define IOC4DEV_KBC		5
#define IOC4DEV_ATAPI		6

#define IOC4_SIO_IR		0x00000008
#define	IOC4_OTHER_IR		0x0000000c
#define IOC4_SIO_IES		0x00000010
#define IOC4_OTHER_IES		0x00000014
#define IOC4_SIO_IEC		0x00000018
#define IOC4_OTHER_IEC		0x0000001c
#define IOC4_SIO_CR		0x00000020
#define	IOC4_MCR		0x00000024

/* bits in the SIO interrupt register */
#define	IOC4_SIRQ_UARTA		0x00000040	/* UART A passthrough */
#define	IOC4_SIRQ_UARTB		0x00004000	/* UART B passthrough */
#define	IOC4_SIRQ_UARTC		0x00400000	/* UART C passthrough */
#define	IOC4_SIRQ_UARTD		0x40000000	/* UART D passthrough */

/* bits in the OTHER interrupt register */
#define	IOC4_OIRQ_ATAPI		0x00000001	/* ATAPI passthrough */
#define	IOC4_OIRQ_KBC		0x00000040	/* keyboard controller */

#define	IOC4_ATAPI_BASE		0x00000100
#define	IOC4_ATAPI_SIZE		0x00000100

#define	IOC4_KBC_BASE		0x00000200
#define	IOC4_KBC_SIZE		0x00000014

#define	IOC4_UARTA_BASE		0x00000380
#define	IOC4_UARTB_BASE		0x00000388
#define	IOC4_UARTC_BASE		0x00000390
#define	IOC4_UARTD_BASE		0x00000398
