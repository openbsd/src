/*	$OpenBSD: uni_n.c,v 1.10 2005/09/26 19:53:41 kettenis Exp $	*/

/*
 * Copyright (c) 1998-2001 Dale Rahn.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/autoconf.h>

#include <dev/ofw/openfirm.h>

static int	memcmatch(struct device *, void *, void *);
static void	memcattach(struct device *, struct device *, void *);

struct memc_softc {
	struct device sc_dev;
	char *baseaddr;
};
/* Driver definition */
struct cfdriver memc_cd = {
	NULL, "memc", DV_DULL
};
/* Driver definition */
struct cfattach memc_ca = {
	sizeof(struct memc_softc), memcmatch, memcattach
};

void *uni_n_config(int handle);

int
memcmatch(struct device *parent, void *cf, void *aux)
{
	struct confargs *ca = aux;
	static int memc_attached = 0;

	/* allow only one instance */
	if (memc_attached == 0) {
		if (0 == strcmp (ca->ca_name, "memc"))
			return 1;
	}
	return 0;
}

static void
memcattach(struct device *parent, struct device *self, void *aux)
{
	struct confargs *ca = aux;
	int len;
	char name[64];
	struct memc_softc *sc = (struct memc_softc *)self;

	len = OF_getprop(ca->ca_node, "name", name, sizeof name);
	if (len > 0)
		name[len] = 0;

	if (strcmp(name, "uni-n") == 0 || strcmp(name, "u3") == 0)
		sc->baseaddr = uni_n_config(ca->ca_node);

	printf (": %s\n", name);
}

void *
uni_n_config(int handle)
{
	char name[20];
	char *baseaddr;
	int *ctladdr;
	u_int32_t address;

	if (OF_getprop(handle, "name", name, sizeof name) > 0) {
		/* sanity test */
		if (strcmp (name, "uni-n") == 0 || strcmp (name, "u3") == 0) {
			if (OF_getprop(handle, "reg", &address,
			    sizeof address) > 0) {
				baseaddr = mapiodev(address, NBPG);
				ctladdr = (void *)(baseaddr + 0x20);
				*ctladdr |= 0x02;
				return baseaddr;
			}
		}
	}
	return 0;
}
