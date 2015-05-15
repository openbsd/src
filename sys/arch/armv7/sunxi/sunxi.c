/* $OpenBSD: sunxi.c,v 1.4 2015/05/15 15:35:43 jsg Exp $ */
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
#include <armv7/armv7/armv7var.h>
#include <armv7/sunxi/sunxireg.h>

struct cfattach sunxi_ca = {
	sizeof(struct armv7_softc), armv7_match, armv7_attach
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
	{ "sxiuart",	0 },
	{ "sxiuart",	1 },
	{ "sxiuart",	2 },
	{ "sxiuart",	3 },
	{ "sxiuart",	4 },
	{ "sxiuart",	5 },
	{ "sxiuart",	6 },
	{ "sxiuart",	7 },
	{ "sxie",	0 },
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
	{ "sxitimer",	0 },
	{ "sxitimer",	1 },
	{ "sxitimer",	2 },
	{ "sxidog",	0 },
	{ "sxirtc",	0 },
	{ "sxiuart",	0 },
	{ "sxiuart",	1 },
	{ "sxiuart",	2 },
	{ "sxiuart",	3 },
	{ "sxiuart",	4 },
	{ "sxiuart",	5 },
	{ "sxiuart",	6 },
	{ "sxiuart",	7 },
	{ "sxie",	0 },
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
		"Allwinner A1x",
		sun4i_devs,
		sxia1x_init,
	},
	{
		BOARD_ID_SUN7I_A20,
		"Allwinner A20",
		sun7i_devs,
		sxia20_init,
	},
	{ 0, NULL, NULL, NULL },
};


struct board_dev *
sunxi_board_attach(void)
{
	struct board_dev *devs = NULL;
	bus_space_handle_t ioh;
	int i;

	for (i = 0; sunxi_boards[i].name != NULL; i++) {
		if (sunxi_boards[i].board_id == board_id) {
			sunxi_boards[i].init();
			devs = sunxi_boards[i].devs;
			break;
		}
	}

	if (devs) {
		if (bus_space_map(&armv7_bs_tag, SYSCTRL_ADDR, SYSCTRL_SIZE, 0,
		    &ioh))
			panic("sunxi_attach: bus_space_map failed!");
		/* map the part of SRAM dedicated to EMAC to EMAC */
		bus_space_write_4(&armv7_bs_tag, ioh, 4,
		    bus_space_read_4(&armv7_bs_tag, ioh, 4) | (5 << 2));
	}

	return (devs);
}

const char *
sunxi_board_name(void)
{
	int i;

	for (i = 0; sunxi_boards[i].name != NULL; i++) {
		if (sunxi_boards[i].board_id == board_id) {
			return (sunxi_boards[i].name);
			break;
		}
	}
	return (NULL);
}
