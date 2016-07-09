/* $OpenBSD: imx.c,v 1.22 2016/07/09 18:14:18 kettenis Exp $ */
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

#include <dev/ofw/fdt.h>

int	imx_match(struct device *, void *, void *);
void	imx6_init();

struct cfattach imx_ca = {
	sizeof(struct armv7_softc), imx_match, armv7_attach
};

struct cfdriver imx_cd = {
	NULL, "imx", DV_DULL
};

struct board_dev imx_devs[] = {
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
	{ NULL,		0 }
};

const char *imx_compatible[] = {
	"fsl,imx6sl",
	"fsl,imx6sx",
	"fsl,imx6dl",
	"fsl,imx6q",
	NULL
};

struct board_dev *
imx_board_devs(void)
{
	void *node;
	int i;

	node = fdt_find_node("/");
	if (node == NULL)
		return NULL;

	for (i = 0; imx_compatible[i] != NULL; i++) {
		if (fdt_is_compatible(node, imx_compatible[i]))
			return imx_devs;
	}
	return NULL;
}

void
imx_board_init(void)
{
	imx6_init();
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
