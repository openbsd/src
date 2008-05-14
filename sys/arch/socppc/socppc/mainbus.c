/*	$OpenBSD: mainbus.c,v 1.2 2008/05/14 20:54:36 kettenis Exp $	*/

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
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>

int	mainbus_match(struct device *, void *, void *);
void	mainbus_attach(struct device *, struct device *, void *);

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

int	mainbus_search(struct device *, void *, void *);
int	mainbus_print(void *, const char *);

int
mainbus_match(struct device *parent, void *cfdata, void *aux)
{
	return (1);
}

void
mainbus_attach(struct device *parent, struct device *self, void *aux)
{
	printf("\n");

	config_search(mainbus_search, self, self);
}

struct ppc_bus_space mainbus_bus_space = { 0xff400000, 0x00100000, 0 };

int
mainbus_search(struct device *parent, void *cfdata, void *aux)
{
	struct mainbus_attach_args ma;
	struct cfdata *cf = cfdata;

	ma.ma_iot = &mainbus_bus_space;
	ma.ma_name = cf->cf_driver->cd_name;
	config_found(parent, &ma, mainbus_print);

	return (1);
}

int
mainbus_print(void *aux, const char *name)
{
	struct mainbus_attach_args *ma = aux;

	if (name)
		printf("%s at %s", ma->ma_name, name);
		
	return (UNCONF);
}
