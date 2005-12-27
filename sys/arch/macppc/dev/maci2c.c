/*	$OpenBSD: maci2c.c,v 1.4 2005/12/27 17:18:18 deraadt Exp $	*/

/*
 * Copyright (c) 2005 Mark Kettenis
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
#include <sys/device.h>

#include <dev/i2c/i2cvar.h>
#include <dev/ofw/openfirm.h>

#include <arch/macppc/dev/maci2cvar.h>

int	maciic_match(struct device *, void *, void *);
void	maciic_attach(struct device *, struct device *, void *);
int	maciic_print(void *, const char *);

struct cfattach maciic_ca = {
	sizeof (struct device), maciic_match, maciic_attach
};

struct cfdriver maciic_cd = {
	NULL, "maciic", DV_DULL
};

int
maciic_match(struct device *parent, void *cf, void *aux)
{
	return (1);
}

void
maciic_attach(struct device *parent, struct device *self, void *aux)
{
	struct maci2cbus_attach_args *iba = aux;
	struct i2c_attach_args ia;
	char name[32];
	u_int32_t reg;
	int node;

	printf("\n");

	for (node = OF_child(iba->iba_node); node; node = OF_peer(node)) {
		if (OF_getprop(node, "reg", &reg, sizeof reg) != sizeof reg &&
		    OF_getprop(node, "i2c-address", &reg, sizeof reg) != sizeof reg)
			continue;
		ia.ia_tag = iba->iba_tag;
		ia.ia_addr = (reg >> 1);
		ia.ia_name = NULL;
		memset(name, 0, sizeof name);
		if (OF_getprop(node, "compatible", &name,
		    sizeof name) && name[0])
			ia.ia_name = name;
		if (ia.ia_name == NULL && 
		    OF_getprop(node, "name", &name,
		    sizeof name) && name[0])
			ia.ia_name = name;
		if (ia.ia_name)
			config_found(self, &ia, maciic_print);
	}
}

int
maciic_print(void *aux, const char *pnp)
{
	struct i2c_attach_args *ia = aux;

	if (pnp != NULL) {
		printf("\"%s\" at %s", ia->ia_name, pnp);
	}
	printf(" addr 0x%x", ia->ia_addr);

	return (UNCONF);
}
