/*	$OpenBSD: gdium_machdep.c,v 1.1 2010/02/05 20:51:22 miod Exp $	*/

/*
 * Copyright (c) 2010 Miodrag Vallat.
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
 * Gdium Liberty specific code and configuration data.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>

#include <dev/pci/pcireg.h>

#include <loongson/dev/bonitovar.h>
#include <loongson/dev/bonito_irq.h>

int	 gdium_intr_map(int, int, int);

const struct bonito_config gdium_bonito = {
	.bc_adbase = 11,

	.bc_gpioIE = LOONGSON_INTRMASK_GPIO,
	.bc_intEdge = LOONGSON_INTRMASK_PCI_SYSERR |
	    LOONGSON_INTRMASK_PCI_PARERR,
	.bc_intSteer = 0,
	.bc_intPol = LOONGSON_INTRMASK_DRAM_PARERR |
	    LOONGSON_INTRMASK_PCI_SYSERR | LOONGSON_INTRMASK_PCI_PARERR,

	.bc_intr_map = gdium_intr_map
};

int
gdium_intr_map(int dev, int fn, int pin)
{
	switch (dev) {
	/* Wireless */
	case 13:	/* C D A B */
		return BONITO_DIRECT_IRQ(LOONGSON_INTR_PCIA + (pin + 1) % 4);
	/* Frame buffer */
	case 14:
		return BONITO_DIRECT_IRQ(LOONGSON_INTR_PCIA);
	/* USB */
	case 15:
#if 0	/* on revision 1 -- how to tell them apart? */
		return BONITO_DIRECT_IRQ(LOONGSON_INTR_PCIA +
		    (pa->pa_intrpin - 1));
#else
		return BONITO_DIRECT_IRQ(LOONGSON_INTR_PCIB);
#endif
	/* Ethernet */
	case 16:
		return BONITO_DIRECT_IRQ(LOONGSON_INTR_PCID);
	/* USB */
	case 17:
#if 0 /* not present on revision 1 */
		break;
#else
		return BONITO_DIRECT_IRQ(LOONGSON_INTR_PCIC);
#endif
	default:
		break;
	}

	return -1;
}
