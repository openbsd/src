/* $OpenBSD: sunxi.c,v 1.16 2016/08/05 21:39:02 kettenis Exp $ */
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

#include <arm/armv7/armv7var.h>
#include <arm/mainbus/mainbus.h>
#include <armv7/armv7/armv7var.h>
#include <armv7/sunxi/sunxireg.h>

#include <dev/ofw/fdt.h>

int	sunxi_match(struct device *, void *, void *);
void	sxia1x_init();
void	sxia20_init();

struct cfattach sunxi_ca = {
	sizeof(struct armv7_softc), sunxi_match, armv7_attach
};

struct cfdriver sunxi_cd = {
	NULL, "sunxi", DV_DULL
};

struct board_dev sun4i_devs[] = {
	{ "sxipio",	0 },
	{ "sxiccmu",	0 },
	{ "sxitimer",	0 },
	{ "sxitimer",	1 },
	{ "sxitimer",	2 },
	{ "sxidog",	0 },
	{ NULL,		0 }
};

struct board_dev sun5i_devs[] = {
	{ "sxipio",	0 },
	{ "sxiccmu",	0 },
	{ "sxitimer",	0 },
	{ "sxitimer",	1 },
	{ "sxitimer",	2 },
	{ "sxidog",	0 },
	{ NULL,		0 }
};

struct board_dev sun7i_devs[] = {
	{ "sxipio",	0 },
	{ "sxiccmu",	0 },
	{ "sxidog",	0 },
	{ NULL,		0 }
};

struct sunxi_soc {
	char			*compatible;
	struct board_dev	*devs;
	void			(*init)(void);
};

struct sunxi_soc sunxi_socs[] = {
	{
		"allwinner,sun4i-a10",
		sun4i_devs,
		sxia1x_init,
	},
	{
		"allwinner,sun5i-a10s",
		sun5i_devs,
		sxia1x_init,
	},
	{
		"allwinner,sun5i-r8",
		sun5i_devs,
		sxia1x_init,
	},
	{
		"allwinner,sun7i-a20",
		sun7i_devs,
		sxia20_init,
	},
	{ NULL, NULL, NULL },
};

struct board_dev *
sunxi_board_devs(void)
{
	void *node;
	int i;

	node = fdt_find_node("/");
	if (node == NULL)
		return NULL;

	for (i = 0; sunxi_socs[i].compatible != NULL; i++) {
		if (fdt_is_compatible(node, sunxi_socs[i].compatible))
			return sunxi_socs[i].devs;
	}
	return NULL;
}

void
sunxi_board_init(void)
{
	bus_space_handle_t ioh;
	void *node;
	int i, match = 0;

	node = fdt_find_node("/");
	if (node == NULL)
		return;

	for (i = 0; sunxi_socs[i].compatible != NULL; i++) {
		if (fdt_is_compatible(node, sunxi_socs[i].compatible)) {
			sunxi_socs[i].init();
			match = 1;
			break;
		}
	}

	if (match) {
		if (bus_space_map(&armv7_bs_tag, SYSCTRL_ADDR, SYSCTRL_SIZE, 0,
		    &ioh))
			panic("sunxi_attach: bus_space_map failed!");
		/* map the part of SRAM dedicated to EMAC to EMAC */
		bus_space_write_4(&armv7_bs_tag, ioh, 4,
		    bus_space_read_4(&armv7_bs_tag, ioh, 4) | (5 << 2));
	}
}

int
sunxi_match(struct device *parent, void *cfdata, void *aux)
{
	union mainbus_attach_args *ma = (union mainbus_attach_args *)aux;
	struct cfdata *cf = (struct cfdata *)cfdata;

	if (ma->ma_name == NULL)
		return (0);

	if (strcmp(cf->cf_driver->cd_name, ma->ma_name) != 0)
		return (0);

	return (sunxi_board_devs() != NULL);
}
