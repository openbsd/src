/*	$OpenBSD: vexpress.c,v 1.2 2015/06/14 05:01:31 jsg Exp $	*/

/*
 * Copyright (c) 2015 Jonathan Gray <jsg@openbsd.org>
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

#include <arm/cpufunc.h>
#include <armv7/armv7/armv7var.h>

int	vexpress_match(struct device *, void *, void *);
void	vexpress_a9_init();
void	vexpress_a15_init();

struct cfattach vexpress_ca = {
	sizeof(struct armv7_softc), vexpress_match, armv7_attach, NULL,
	config_activate_children
};

struct cfdriver vexpress_cd = {
	NULL, "vexpress", DV_DULL
};

struct board_dev vexpress_devs[] = {
	{ "sysreg",	0 },
	{ "pluart",	0 },
	{ "plrtc",	0 },
	{ "virtio",	0 },
	{ "virtio",	1 },
	{ "virtio",	2 },
	{ "virtio",	3 },
	{ NULL,		0 }
};

struct armv7_board vexpress_boards[] = {
	{
		BOARD_ID_VEXPRESS,
		"ARM Versatile Express",
		vexpress_devs,
		NULL,
	},
	{ 0, NULL, NULL, NULL },
};

struct board_dev *
vexpress_board_devs(void)
{
	int i;

	for (i = 0; vexpress_boards[i].name != NULL; i++) {
		if (vexpress_boards[i].board_id == board_id)
			return (vexpress_boards[i].devs);
	}
	return (NULL);
}

extern vaddr_t physical_start;

int
vexpress_legacy_map(void)
{
	return ((cpufunc_id() & CPU_ID_CORTEX_A9_MASK) == CPU_ID_CORTEX_A9);
}

void
vexpress_board_init(void)
{
	if (board_id != BOARD_ID_VEXPRESS)
		return;

	if (vexpress_legacy_map())
		vexpress_a9_init();
	else
		vexpress_a15_init();
}

const char *
vexpress_board_name(void)
{
	int i;

	for (i = 0; vexpress_boards[i].name != NULL; i++) {
		if (vexpress_boards[i].board_id == board_id) {
			return (vexpress_boards[i].name);
			break;
		}
	}
	return (NULL);
}

int
vexpress_match(struct device *parent, void *cfdata, void *aux)
{
	return (vexpress_board_devs() != NULL);
}
