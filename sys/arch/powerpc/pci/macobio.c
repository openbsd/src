/*	$NetBSD: obio.c,v 1.6 1999/05/01 10:36:08 tsubai Exp $	*/

/*-
 * Copyright (C) 1998	Internet Research Institute, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by
 *	Internet Research Institute, Inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/autoconf.h>

static void obio_attach __P((struct device *, struct device *, void *));
static int obio_match __P((struct device *, void *, void *));
static int obio_print __P((void *, const char *));

struct obio_softc {
	struct device sc_dev;
	int sc_node;
	struct ppc_bus_space sc_membus_space;
};
struct cfdriver obio_cd = {
	NULL, "macobio", DV_DULL,
};


struct cfattach obio_ca = {
	sizeof(struct obio_softc), obio_match, obio_attach
};

int
obio_match(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_APPLE)
		switch (PCI_PRODUCT(pa->pa_id)) {

		case 0x02:	/* gc */
		case 0x07:	/* ohare */
		case 0x10:	/* mac-io "Heathrow" */
		case 0x17:	/* mac-io "Paddington" */
		case 0x22:	/* mac-io "Keylargo" */
			return 1;
		}

	return 0;
}

#define HEATHROW_FCR_OFFSET 0x38
u_int32_t *heathrow_FCR = NULL;

/*
 * Attach all the sub-devices we can find
 */
void
obio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct obio_softc *sc = (struct obio_softc *)self;
	struct pci_attach_args *pa = aux;
	struct confargs ca;
	int node, child, namelen;
	u_int32_t reg[20];
	int32_t intr[5];
	char name[32];

	printf("obio ver %x", (PCI_PRODUCT(pa->pa_id)));

	switch (PCI_PRODUCT(pa->pa_id)) {

	/* XXX should not use name */
	case 0x02:
		node = OF_finddevice("/bandit/gc");
		break;

	case 0x07:
		node = OF_finddevice("/bandit/ohare");
		break;

	case 0x10:	/* heathrow */
	case 0x17:	/* paddington */
		node = OF_finddevice("mac-io");
		if (node == -1)
			node = OF_finddevice("/pci/mac-io");
		if (OF_getprop(node, "assigned-addresses", reg, sizeof(reg))
			== (sizeof (reg[0]) * 5))
		{
			/* always ??? */
			heathrow_FCR = mapiodev(reg[2] + HEATHROW_FCR_OFFSET,
				4);
		}
		break;
	case 0x22:	/* keylargo */
		node = OF_finddevice("mac-io");
		if (node == -1)
			node = OF_finddevice("/pci/mac-io");

		break;
	default:
		printf("obio_attach: unknown obio controller\n");
		return;
	}
	sc->sc_node = node;

	if (OF_getprop(node, "assigned-addresses", reg, sizeof(reg)) < 12)
		return;

	ca.ca_baseaddr = reg[2];

	sc->sc_membus_space.bus_base = ca.ca_baseaddr;

	sc->sc_membus_space.bus_reverse = 1;

	ca.ca_iot = &sc->sc_membus_space;

	printf(": addr 0x%x\n", ca.ca_baseaddr);

	for (child = OF_child(node); child; child = OF_peer(child)) {
		namelen = OF_getprop(child, "name", name, sizeof(name));
		if (namelen < 0)
			continue;
		if (namelen >= sizeof(name))
			continue;

		name[namelen] = 0;
		ca.ca_name = name;
		ca.ca_node = child;

		ca.ca_nreg  = OF_getprop(child, "reg", reg, sizeof(reg));
		ca.ca_nintr = OF_getprop(child, "AAPL,interrupts", intr,
				sizeof(intr));
		if (ca.ca_nintr == -1)
			ca.ca_nintr = OF_getprop(child, "interrupts", intr,
					sizeof(intr));

		ca.ca_reg = reg;
		ca.ca_intr = intr;

		config_found(self, &ca, obio_print);
	}
}

int
obio_print(aux, obio)
	void *aux;
	const char *obio;
{
	struct confargs *ca = aux;

#if 0
/* no reason to clutter the screen with unneccessary printfs */
	if (obio)
		printf("%s at %s", ca->ca_name, obio);

	if (ca->ca_nreg > 0)
		printf(" offset 0x%x", ca->ca_reg[0]);

	return UNCONF;
#endif
	return QUIET;
}

typedef int mac_intr_handle_t;

typedef void     *(intr_establish_t) __P((void *, mac_intr_handle_t,
            int, int, int (*func)(void *), void *, char *));
typedef void     (intr_disestablish_t) __P((void *, void *));


void *
undef_mac_establish(lcv, irq, type, level, ih_fun, ih_arg, name)
	void * lcv;
	int irq;
	int type;
	int level;
	int (*ih_fun) __P((void *));
	void *ih_arg;
	char *name;
{
	printf("mac_intr_establish called, not yet inited\n");
	return 0;
}
void
mac_intr_disestab(lcp, arg)
	void *lcp;
	void *arg;
{
	printf("mac_intr_disestablish called, not yet inited\n");
}

intr_establish_t *mac_intr_establish_func = undef_mac_establish;
intr_disestablish_t *mac_intr_disestablish_func = mac_intr_disestab;

void *
mac_intr_establish(lcv, irq, type, level, ih_fun, ih_arg, name)
	void * lcv;
	int irq;
	int type;
	int level;
	int (*ih_fun) __P((void *));
	void *ih_arg;
	char *name;
{
	return (*mac_intr_establish_func)(lcv, irq, type, level, ih_fun,
		ih_arg, name);
}
void
mac_intr_disestablish(lcp, arg)
	void *lcp;
	void *arg;
{
	(*mac_intr_disestablish_func)(lcp, arg);
}
