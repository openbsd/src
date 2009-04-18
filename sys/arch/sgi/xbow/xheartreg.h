/*	$OpenBSD: xheartreg.h,v 1.2 2009/04/18 14:48:09 miod Exp $	*/

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
 * IP30 HEART registers
 */

/* physical address in PIU mode */
#define	HEART_PIU_BASE	0x000000000ff00000UL

#define	HEART_MODE		0x0000
#define	HEART_MEMORY_STATUS	0x0020	/* 8 32 bit registers */
#define	HEART_MEMORY_VALID		0x80000000
#define	HEART_MEMORY_SIZE_MASK		0x003f0000
#define	HEART_MEMORY_SIZE_SHIFT		16
#define	HEART_MEMORY_ADDR_MASK		0x000001ff
#define	HEART_MEMORY_ADDR_SHIFT		0
#define	HEART_MEMORY_UNIT_SHIFT		25	/* 32MB */

#define	HEART_MICROLAN		0x00b8

/*
 * Interrupt handling registers.
 * The Heart supports four different interrupt targets, although only
 * the two cpus are used in practice.
 */

#define	HEART_IMR(s)		(0x00010000 + (s) * 8)
#define	HEART_ISR_SET		0x00010020
#define	HEART_ISR_CLR		0x00010028
#define	HEART_ISR		0x00010030

/*
 * ISR bit assignments (partial).
 */

#define	HEART_INTR_ACFAIL	15
#define	HEART_INTR_POWER	14
#define	HEART_INTR_WIDGET_MAX	13
#define	HEART_INTR_WIDGET_MIN	0

#define	HEART_INTR_MAX		15
#define	HEART_INTR_MIN		0
