/*	$OpenBSD: uni_n.c,v 1.11 2005/10/23 16:50:30 drahn Exp $	*/

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

struct memc_softc {
	struct device sc_dev;
	char *baseaddr;
	struct ppc_bus_space sc_membus_space;

};

int	memcmatch(struct device *, void *, void *);
void	memcattach(struct device *, struct device *, void *);
void	memc_attach_children(struct memc_softc *sc, int memc_node);
int	memc_print(void *aux, const char *name);

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

void
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

	if (strcmp(name, "u3") == 0)
		memc_attach_children(sc, ca->ca_node);
}

void
memc_attach_children(struct memc_softc *sc, int memc_node)
{
	struct confargs ca;
	int node, namelen;
	u_int32_t reg[20];
	int32_t	intr[8];
	char	name[32];

        sc->sc_membus_space.bus_base = ca.ca_baseaddr;

	ca.ca_iot = &sc->sc_membus_space;
	ca.ca_dmat = 0; /* XXX */
	ca.ca_baseaddr = 0; /* XXX */
	sc->sc_membus_space.bus_base = ca.ca_baseaddr;

        for (node = OF_child(memc_node); node; node = OF_peer(node)) {
		namelen = OF_getprop(node, "name", name, sizeof(name));
		if (namelen < 0)
			continue;
		if (namelen >= sizeof(name))
			continue;
		name[namelen] = 0;

		ca.ca_name = name;
		ca.ca_node = node;
		ca.ca_nreg  = OF_getprop(node, "reg", reg, sizeof(reg));
		ca.ca_reg = reg;
		ca.ca_nintr = 0; /* XXX */
		ca.ca_intr = intr; /* XXX */

		config_found((struct device *)sc, &ca, memc_print);
	}
}

int
memc_print(void *aux, const char *name)
{
	struct confargs *ca = aux;
	/* we dont want extra stuff printing */
	if (name)
		printf("%s at %s", ca->ca_name, name);
	if (ca->ca_nreg > 0)
		printf(" offset 0x%x", ca->ca_reg[0]);
	return UNCONF;
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
