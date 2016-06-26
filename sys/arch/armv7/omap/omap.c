/* $OpenBSD: omap.c,v 1.13 2016/06/26 05:16:33 jsg Exp $ */
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

#include <arm/mainbus/mainbus.h>
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
	{ "omgpio",	0 },
	{ "omgpio",	1 },
	{ "omgpio",	2 },
	{ "omgpio",	3 },
	{ "omgpio",	4 },
	{ "omgpio",	5 },
	{ NULL,		0 }
};

struct board_dev beaglebone_devs[] = {
	{ "prcm",	0 },
	{ "sitaracm",	0 },
	{ "intc",	0 },
	{ "edma",	0 },
	{ "dmtimer",	0 },
	{ "dmtimer",	1 },
	{ "omgpio",	0 },
	{ "omgpio",	1 },
	{ "omgpio",	2 },
	{ "omgpio",	3 },
	{ "tiiic",	0 },
	{ "tiiic",	1 },
	{ "tiiic",	2 },
	{ "cpsw",	0 },
	{ NULL,		0 }
};

struct board_dev overo_devs[] = {
	{ "prcm",	0 },
	{ "intc",	0 },
	{ "gptimer",	0 },
	{ "gptimer",	1 },
	{ "omgpio",	0 },
	{ "omgpio",	1 },
	{ "omgpio",	2 },
	{ "omgpio",	3 },
	{ "omgpio",	4 },
	{ "omgpio",	5 },
	{ NULL,		0 }
};

struct board_dev pandaboard_devs[] = {
	{ "omapid",	0 },
	{ "prcm",	0 },
	{ "omgpio",	0 },
	{ "omgpio",	1 },
	{ "omgpio",	2 },
	{ "omgpio",	3 },
	{ "omgpio",	4 },
	{ "omgpio",	5 },
	{ "ehci",	0 },
	{ NULL,		0 }
};

struct armv7_board omap_boards[] = {
	{
		BOARD_ID_OMAP3_BEAGLE,
		beagleboard_devs,
		omap3_init,
	},
	{
		BOARD_ID_AM335X_BEAGLEBONE,
		beaglebone_devs,
		am335x_init,
	},
	{
		BOARD_ID_OMAP3_OVERO,
		overo_devs,
		omap3_init,
	},
	{
		BOARD_ID_OMAP4_PANDA,
		pandaboard_devs,
		omap4_init,
	},
	{ 0, NULL, NULL },
};

struct board_dev *
omap_board_devs(void)
{
	int i;

	for (i = 0; omap_boards[i].board_id != 0; i++) {
		if (omap_boards[i].board_id == board_id)
			return (omap_boards[i].devs);
	}
	return (NULL);
}

void
omap_board_init(void)
{
	int i;

	for (i = 0; omap_boards[i].board_id != 0; i++) {
		if (omap_boards[i].board_id == board_id) {
			omap_boards[i].init();
			break;
		}
	}
}

int
omap_match(struct device *parent, void *cfdata, void *aux)
{
	union mainbus_attach_args *ma = (union mainbus_attach_args *)aux;
	struct cfdata *cf = (struct cfdata *)cfdata;

	if (ma->ma_name == NULL)
		return (0);

	if (strcmp(cf->cf_driver->cd_name, ma->ma_name) != 0)
		return (0);

	return (omap_board_devs() != NULL);
}
