/*	$OpenBSD: ip30.h,v 1.2 2009/10/14 20:21:16 miod Exp $	*/

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
 * Physical memory on Octane starts at 512MB.
 *
 * This allows the small windows of all widgets to appear under physical
 * memory, and the Bridge window (#f) to sport the machine PROM at the
 * physical address where the CPU expects it on reset.
 */

#define	IP30_MEMORY_BASE		0x20000000
#define	IP30_MEMORY_ARCBIOS_LIMIT	0x40000000

/*
 * On-board IOC3 specific GPIO registers wiring
 */

/* LED bar control: 0 to dim, 1 to lit */
#define	IP30_GPIO_WHITE_LED		0
#define	IP30_GPIO_RED_LED		1
/* Classic Octane (1) vs Octane 2 (0), read only */
#define	IP30_GPIO_CLASSIC		2
