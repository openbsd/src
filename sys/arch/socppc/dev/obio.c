/*	$OpenBSD: obio.c,v 1.4 2009/09/02 20:29:39 kettenis Exp $	*/

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

#include <dev/ofw/openfirm.h>

int	obio_match(struct device *, void *, void *);
void	obio_attach(struct device *, struct device *, void *);

struct cfattach obio_ca = {
	sizeof(struct device), obio_match, obio_attach
};

struct cfdriver obio_cd = {
	NULL, "obio", DV_DULL
};

int	obio_search(struct device *, void *, void *);
int	obio_print(void *, const char *);

int
obio_match(struct device *parent, void *cfdata, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	char buf[32];

	if (OF_getprop(ma->ma_node, "device_type", buf, sizeof(buf)) <= 0)
		return (0);

	if (strcmp(buf, "soc") == 0)
		return (1);

	return (0);
}

void
obio_attach(struct device *parent, struct device *self, void *aux)
{
	printf("\n");

	config_search(obio_search, self, aux);
}

int
obio_search(struct device *parent, void *cfdata, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	struct obio_attach_args oa;
	struct cfdata *cf = cfdata;

	bzero(&oa, sizeof oa);
	oa.oa_iot = ma->ma_iot;
	oa.oa_dmat = ma->ma_dmat;
	oa.oa_offset = cf->cf_offset;
	oa.oa_ivec = cf->cf_ivec;
	oa.oa_phy = cf->cf_phy;
	oa.oa_name = cf->cf_driver->cd_name;
	config_found(parent, &oa, obio_print);

	return (1);
}

int
obio_print(void *aux, const char *name)
{
	struct obio_attach_args *oa = aux;

	if (name)
		printf("%s at %s", oa->oa_name, name);
	if (oa->oa_offset)
		printf(" offset 0x%05x", oa->oa_offset);
	if (oa->oa_ivec != -1)
		printf(" ivec %d", oa->oa_ivec);

	return (UNCONF);
}
