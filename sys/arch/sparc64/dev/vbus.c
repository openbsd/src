/*	$OpenBSD: vbus.c,v 1.1 2008/03/08 19:18:27 kettenis Exp $	*/
/*
 * Copyright (c) 2008 Mark Kettenis
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
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <sparc64/dev/vbusvar.h>

#include <dev/clock_subr.h>
extern todr_chip_handle_t todr_handle;

int	vbus_match(struct device *, void *, void *);
void	vbus_attach(struct device *, struct device *, void *);
int	vbus_print(void *, const char *);

struct cfattach vbus_ca = {
	sizeof(struct device), vbus_match, vbus_attach
};

struct cfdriver vbus_cd = {
	NULL, "vbus", DV_DULL
};

int
vbus_match(struct device *parent, void *match, void *aux)
{
	struct mainbus_attach_args *ma = aux;

	if (strcmp(ma->ma_name, "virtual-devices") == 0)
		return (1);

	return (0);
}

void
vbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	int node;

	printf("\n");

	for (node = OF_child(ma->ma_node); node; node = OF_peer(node)) {
		struct vbus_attach_args va;
		char buf[32];

		bzero(&va, sizeof(va));
		va.va_node = node;
		if (OF_getprop(node, "name", buf, sizeof(buf)) <= 0)
			continue;
		va.va_name = buf;
		getprop(node, "reg", sizeof(*va.va_reg),
		    &va.va_nreg, (void **)&va.va_reg);
		getprop(node, "interrupts", sizeof(*va.va_intr),
		    &va.va_nintr, (void **)&va.va_intr);
		config_found(self, &va, vbus_print);
	}

	if (todr_handle == NULL) {
		struct vbus_attach_args va;

		bzero(&va, sizeof(va));
		va.va_name = "rtc";
		config_found(self, &va, vbus_print);
	}
}

int
vbus_print(void *aux, const char *name)
{
	struct vbus_attach_args *va = aux;

	if (name)
		printf("\"%s\" at %s", va->va_name, name);
	return (UNCONF);
}
