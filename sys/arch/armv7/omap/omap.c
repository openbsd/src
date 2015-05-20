/* $OpenBSD: omap.c,v 1.7 2015/05/20 00:14:56 jsg Exp $ */
/*
 * Copyright (c) 2005,2008 Dale Rahn <drahn@openbsd.com>
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

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <armv7/armv7/armv7var.h>

int	omap_match(struct device *, void *, void *);
void	omap3_init();
void	omap4_init();
void	am335x_init();

struct cfattach omap_ca = {
	sizeof(struct armv7_softc), omap_match, armv7_attach
};

struct cfdriver omap_cd = {
	NULL, "omap", DV_DULL
};

struct board_dev beagleboard_devs[] = {
	{ "prcm",	0 },
	{ "intc",	0 },
	{ "gptimer",	0 },
	{ "gptimer",	1 },
	{ "omdog",	0 },
	{ "omgpio",	0 },
	{ "omgpio",	1 },
	{ "omgpio",	2 },
	{ "omgpio",	3 },
	{ "omgpio",	4 },
	{ "omgpio",	5 },
	{ "ommmc",	0 },		/* HSMMC1 */
	{ "com",	2 },		/* UART3 */
	{ NULL,		0 }
};

struct board_dev beaglebone_devs[] = {
	{ "prcm",	0 },
	{ "sitaracm",	0 },
	{ "intc",	0 },
	{ "edma",	0 },
	{ "dmtimer",	0 },
	{ "dmtimer",	1 },
	{ "omdog",	0 },
	{ "omgpio",	0 },
	{ "omgpio",	1 },
	{ "omgpio",	2 },
	{ "omgpio",	3 },
	{ "tiiic",	0 },
	{ "tiiic",	1 },
	{ "tiiic",	2 },
	{ "ommmc",	0 },		/* HSMMC0 */
	{ "ommmc",	1 },		/* HSMMC1 */
	{ "com",	0 },		/* UART0 */
	{ "cpsw",	0 },
	{ NULL,		0 }
};

struct board_dev overo_devs[] = {
	{ "prcm",	0 },
	{ "intc",	0 },
	{ "gptimer",	0 },
	{ "gptimer",	1 },
	{ "omdog",	0 },
	{ "omgpio",	0 },
	{ "omgpio",	1 },
	{ "omgpio",	2 },
	{ "omgpio",	3 },
	{ "omgpio",	4 },
	{ "omgpio",	5 },
	{ "ommmc",	0 },		/* HSMMC1 */
	{ "com",	2 },		/* UART3 */
	{ NULL,		0 }
};

struct board_dev pandaboard_devs[] = {
	{ "omapid",	0 },
	{ "prcm",	0 },
	{ "omdog",	0 },
	{ "omgpio",	0 },
	{ "omgpio",	1 },
	{ "omgpio",	2 },
	{ "omgpio",	3 },
	{ "omgpio",	4 },
	{ "omgpio",	5 },
	{ "ommmc",	0 },		/* HSMMC1 */
	{ "com",	2 },		/* UART3 */
	{ "ehci",	0 },
	{ NULL,		0 }
};

struct armv7_board omap_boards[] = {
	{
		BOARD_ID_OMAP3_BEAGLE,
		"TI OMAP3 BeagleBoard",
		beagleboard_devs,
		omap3_init,
	},
	{
		BOARD_ID_AM335X_BEAGLEBONE,
		"TI AM335x BeagleBone",
		beaglebone_devs,
		am335x_init,
	},
	{
		BOARD_ID_OMAP3_OVERO,
		"Gumstix OMAP3 Overo",
		overo_devs,
		omap3_init,
	},
	{
		BOARD_ID_OMAP4_PANDA,
		"TI OMAP4 PandaBoard",
		pandaboard_devs,
		omap4_init,
	},
	{ 0, NULL, NULL, NULL },
};

struct board_dev *
omap_board_devs(void)
{
	int i;

	for (i = 0; omap_boards[i].name != NULL; i++) {
		if (omap_boards[i].board_id == board_id)
			return (omap_boards[i].devs);
	}
	return (NULL);
}

void
omap_board_init(void)
{
	int i;

	for (i = 0; omap_boards[i].name != NULL; i++) {
		if (omap_boards[i].board_id == board_id) {
			omap_boards[i].init();
			break;
		}
	}
}

const char *
omap_board_name(void)
{
	int i;

	for (i = 0; omap_boards[i].name != NULL; i++) {
		if (omap_boards[i].board_id == board_id) {
			return (omap_boards[i].name);
			break;
		}
	}
	return (NULL);
}

int
omap_match(struct device *parent, void *cfdata, void *aux)
{
	return (omap_board_devs() != NULL);
}
