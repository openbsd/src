/* $OpenBSD: sunxi.c,v 1.12 2016/06/11 07:07:59 jsg Exp $ */
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
	{ "a1xintc",	0 },
	{ "sxitimer",	0 },
	{ "sxitimer",	1 },
	{ "sxitimer",	2 },
	{ "sxidog",	0 },
	{ "sxirtc",	0 },
	{ "ahci",	0 },
	{ "ehci",	0 },
	{ "ehci",	1 },
#if 0
	{ "ohci",	0 },
	{ "ohci",	1 },
#endif
	{ NULL,		0 }
};

struct board_dev sun7i_devs[] = {
	{ "sxipio",	0 },
	{ "sxiccmu",	0 },
	{ "sxidog",	0 },
	{ "sxirtc",	0 },
	{ "ahci",	0 },
	{ "ehci",	0 },
	{ "ehci",	1 },
#if 0
	{ "ohci",	0 },
	{ "ohci",	1 },
#endif
	{ NULL,		0 }
};

struct armv7_board sunxi_boards[] = {
	{
		BOARD_ID_SUN4I_A10,
		sun4i_devs,
		sxia1x_init,
	},
	{
		BOARD_ID_SUN7I_A20,
		sun7i_devs,
		sxia20_init,
	},
	{ 0, NULL, NULL },
};

struct board_dev *
sunxi_board_devs(void)
{
	int i;

	for (i = 0; sunxi_boards[i].board_id != 0; i++) {
		if (sunxi_boards[i].board_id == board_id)
			return (sunxi_boards[i].devs);
	}
	return (NULL);
}

void
sunxi_board_init(void)
{
	bus_space_handle_t ioh;
	int i, match = 0;

	for (i = 0; sunxi_boards[i].board_id != 0; i++) {
		if (sunxi_boards[i].board_id == board_id) {
			sunxi_boards[i].init();
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
