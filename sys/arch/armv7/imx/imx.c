/* $OpenBSD: imx.c,v 1.7 2015/05/19 03:30:54 jsg Exp $ */
/*
 * Copyright (c) 2005,2008 Dale Rahn <drahn@openbsd.com>
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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

void imx6_init();

struct cfattach imx_ca = {
	sizeof(struct armv7_softc), armv7_match, armv7_attach, NULL,
	config_activate_children
};

struct cfdriver imx_cd = {
	NULL, "imx", DV_DULL
};

struct board_dev hummingboard_devs[] = {
	{ "imxocotp",	0 },
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxdog",	0 },
	{ "imxuart",	0 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "imxesdhc",	1 },
	{ "ehci",	0 },
	{ "ehci",	1 },
	{ "imxenet",	0 },
	{ "ahci",	0 },
	{ NULL,		0 }
};

struct board_dev phyflex_imx6_devs[] = {
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxdog",	0 },
	{ "imxocotp",	0 },
	{ "imxuart",	3 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "imxesdhc",	1 },
	{ "imxesdhc",	2 },
	{ "ehci",	0 },
	{ "imxenet",	0 },
	{ "ahci",	0 },
	{ NULL,		0 }
};

struct board_dev sabrelite_devs[] = {
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxdog",	0 },
	{ "imxocotp",	0 },
	{ "imxuart",	1 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "imxesdhc",	2 },
	{ "imxesdhc",	3 },
	{ "ehci",	0 },
	{ "imxenet",	0 },
	{ "ahci",	0 },
	{ NULL,		0 }
};

struct board_dev sabresd_devs[] = {
	{ "imxocotp",	0 },
	{ "imxccm",	0 },
	{ "imxtemp",	0 },
	{ "imxiomuxc",	0 },
	{ "imxdog",	0 },
	{ "imxuart",	0 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "imxesdhc",	1 },
	{ "imxesdhc",	2 },
	{ "imxesdhc",	3 },
	{ "ehci",	0 },
	{ "imxenet",	0 },
	{ "ahci",	0 },
	{ NULL,		0 }
};

struct board_dev udoo_devs[] = {
	{ "imxocotp",	0 },
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxdog",	0 },
	{ "imxuart",	1 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "imxesdhc",	2 },
	{ "ehci",	0 },
	{ "imxenet",	0 },
	{ "ahci",	0 },
	{ NULL,		0 }
};

struct board_dev utilite_devs[] = {
	{ "imxocotp",	0 },
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxdog",	0 },
	{ "imxuart",	3 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "imxesdhc",	2 },
	{ "ehci",	0 },
	{ "imxenet",	0 },
	{ "ahci",	0 },
	{ NULL,		0 }
};

struct board_dev novena_devs[] = {
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxdog",	0 },
	{ "imxocotp",	0 },
	{ "imxuart",	1 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "imxesdhc",	1 },
	{ "imxesdhc",	2 },
	{ "ehci",	0 },
	{ "imxenet",	0 },
	{ "ahci",	0 },
	{ NULL,		0 }
};

struct board_dev wandboard_devs[] = {
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxdog",	0 },
	{ "imxocotp",	0 },
	{ "imxuart",	0 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "imxenet",	0 },
	{ "imxesdhc",	2 },
	{ "imxesdhc",	0 },
	{ "ehci",	0 },
	{ "ahci",	0 },	/* only on quad, afaik. */
	{ NULL,		0 }
};

struct armv7_board imx_boards[] = {
	{
		BOARD_ID_IMX6_CUBOXI,
		"SolidRun CuBox-i",
		hummingboard_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_HUMMINGBOARD,
		"SolidRun HummingBoard",
		hummingboard_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_PHYFLEX,
		"Phytec phyFLEX-i.MX6",
		phyflex_imx6_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_SABRELITE,
		"Freescale i.MX6 SABRE Lite",
		sabrelite_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_SABRESD,
		"Freescale i.MX6 SABRE SD",
		sabresd_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_UDOO,
		"Udoo i.MX6",
		udoo_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_UTILITE,
		"CompuLab Utilite",
		utilite_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_NOVENA,
		"Kosagi Novena",
		novena_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_WANDBOARD,
		"Wandboard i.MX6",
		wandboard_devs,
		imx6_init,
	},
	{ 0, NULL, NULL, NULL },
};

struct board_dev *
imx_board_devs(void)
{
	int i;

	for (i = 0; imx_boards[i].name != NULL; i++) {
		if (imx_boards[i].board_id == board_id)
			return (imx_boards[i].devs);
	}
	return (NULL);
}

void
imx_board_init(void)
{
	int i;

	for (i = 0; imx_boards[i].name != NULL; i++) {
		if (imx_boards[i].board_id == board_id) {
			imx_boards[i].init();
			break;
		}
	}
}

const char *
imx_board_name(void)
{
	int i;

	for (i = 0; imx_boards[i].name != NULL; i++) {
		if (imx_boards[i].board_id == board_id) {
			return (imx_boards[i].name);
			break;
		}
	}
	return (NULL);
}
