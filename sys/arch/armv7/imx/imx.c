/* $OpenBSD: imx.c,v 1.20 2016/06/13 23:43:58 kettenis Exp $ */
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

#include <arm/mainbus/mainbus.h>
#include <armv7/armv7/armv7var.h>

int	imx_match(struct device *, void *, void *);
void	imx6_init();

struct cfattach imx_ca = {
	sizeof(struct armv7_softc), imx_match, armv7_attach, NULL,
	config_activate_children
};

struct cfdriver imx_cd = {
	NULL, "imx", DV_DULL
};

struct board_dev hummingboard_devs[] = {
	{ "imxocotp",	0 },
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "ehci",	0 },
	{ "ehci",	1 },
	{ NULL,		0 }
};

struct board_dev sabrelite_devs[] = {
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxocotp",	0 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "ehci",	0 },
	{ NULL,		0 }
};

struct board_dev sabresd_devs[] = {
	{ "imxocotp",	0 },
	{ "imxccm",	0 },
	{ "imxtemp",	0 },
	{ "imxiomuxc",	0 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "ehci",	0 },
	{ NULL,		0 }
};

struct board_dev udoo_devs[] = {
	{ "imxocotp",	0 },
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "ehci",	0 },
	{ NULL,		0 }
};

struct board_dev utilite_devs[] = {
	{ "imxocotp",	0 },
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "ehci",	0 },
	{ NULL,		0 }
};

struct board_dev novena_devs[] = {
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxocotp",	0 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "ehci",	0 },
	{ NULL,		0 }
};

struct board_dev wandboard_devs[] = {
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxocotp",	0 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "ehci",	0 },
	{ NULL,		0 }
};

struct armv7_board imx_boards[] = {
	{
		BOARD_ID_IMX6_CUBOXI,
		hummingboard_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_HUMMINGBOARD,
		hummingboard_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_SABRELITE,
		sabrelite_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_SABRESD,
		sabresd_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_UDOO,
		udoo_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_UTILITE,
		utilite_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_NOVENA,
		novena_devs,
		imx6_init,
	},
	{
		BOARD_ID_IMX6_WANDBOARD,
		wandboard_devs,
		imx6_init,
	},
	{ 0, NULL, NULL },
};

struct board_dev *
imx_board_devs(void)
{
	int i;

	for (i = 0; imx_boards[i].board_id != 0; i++) {
		if (imx_boards[i].board_id == board_id)
			return (imx_boards[i].devs);
	}
	return (NULL);
}

void
imx_board_init(void)
{
	int i;

	for (i = 0; imx_boards[i].board_id != 0; i++) {
		if (imx_boards[i].board_id == board_id) {
			imx_boards[i].init();
			break;
		}
	}
}

int
imx_match(struct device *parent, void *cfdata, void *aux)
{
	union mainbus_attach_args *ma = (union mainbus_attach_args *)aux;
	struct cfdata *cf = (struct cfdata *)cfdata;

	if (ma->ma_name == NULL)
		return (0);

	if (strcmp(cf->cf_driver->cd_name, ma->ma_name) != 0)
		return (0);

	return (imx_board_devs() != NULL);
}
