/*	$OpenBSD: sio.c,v 1.30 2004/12/06 19:51:41 brad Exp $	*/
/*	$NetBSD: sio.c,v 1.15 1996/12/05 01:39:36 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/eisa/eisavar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <alpha/pci/siovar.h>

#include "isadma.h"

struct sio_softc {
	struct device	sc_dv;

	bus_space_tag_t sc_iot, sc_memt;
	bus_dma_tag_t	sc_dmat;
	int		sc_haseisa;
};

#ifdef __BROKEN_INDIRECT_CONFIG
int	siomatch(struct device *, void *, void *);
#else
int	siomatch(struct device *, struct cfdata *, void *);
#endif
void	sioattach(struct device *, struct device *, void *);

extern int sio_intr_alloc(isa_chipset_tag_t *, int, int, int *);


struct cfattach sio_ca = {
	sizeof(struct sio_softc), siomatch, sioattach,
};

struct cfdriver sio_cd = {
	NULL, "sio", DV_DULL,
};

#ifdef __BROKEN_INDIRECT_CONFIG
int	pcebmatch(struct device *, void *, void *);
#else
int	pcebmatch(struct device *, struct cfdata *, void *);
#endif

struct cfattach pceb_ca = {
	sizeof(struct sio_softc), pcebmatch, sioattach,
};

struct cfdriver pceb_cd = {
	NULL, "pceb", DV_DULL,
};

union sio_attach_args {
	const char *sa_name;			/* XXX should be common */
	struct isabus_attach_args sa_iba;
	struct eisabus_attach_args sa_eba;
};

int	sioprint(void *, const char *pnp);
void	sio_isa_attach_hook(struct device *, struct device *,
	    struct isabus_attach_args *);
void	sio_eisa_attach_hook(struct device *, struct device *,
	    struct eisabus_attach_args *);
int	sio_eisa_maxslots(void *);
int	sio_eisa_intr_map(void *, u_int, eisa_intr_handle_t *);
void	sio_bridge_callback(struct device *);

int
siomatch(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_CONTAQ &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CONTAQ_82C693 &&
	    pa->pa_function == 0)
		return (1);

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_SIO)
		return (1);

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ALI &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ALI_M1533)
		return(1);

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ALI &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ALI_M1543)
		return(1);
	return (0);
}

int
pcebmatch(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PCEB)
		return (1);

	return (0);
}

void
sioattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sio_softc *sc = (struct sio_softc *)self;
	struct pci_attach_args *pa = aux;

	printf("\n");

	sc->sc_iot = pa->pa_iot;
	sc->sc_memt = pa->pa_memt;
        sc->sc_dmat = pa->pa_dmat;
	sc->sc_haseisa = (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
		PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PCEB);

	evcount_attach(&sio_intr_count, self->dv_xname, NULL, &evcount_intr);

	config_defer(self, sio_bridge_callback);
}

void
sio_bridge_callback(self)
	struct device *self;
{
	struct sio_softc *sc = (struct sio_softc *)self;
	struct alpha_eisa_chipset ec;
	struct alpha_isa_chipset ic;
	union sio_attach_args sa;

	if (sc->sc_haseisa) {
		ec.ec_v = NULL;
		ec.ec_attach_hook = sio_eisa_attach_hook;
		ec.ec_maxslots = sio_eisa_maxslots;
		ec.ec_intr_map = sio_eisa_intr_map;
		ec.ec_intr_string = sio_intr_string;
		ec.ec_intr_establish = sio_intr_establish;
		ec.ec_intr_disestablish = sio_intr_disestablish;

		sa.sa_eba.eba_busname = "eisa";
		sa.sa_eba.eba_iot = sc->sc_iot;
		sa.sa_eba.eba_memt = sc->sc_memt;
		sa.sa_eba.eba_ec = &ec;
		config_found(&sc->sc_dv, &sa.sa_eba, sioprint);
	}

	ic.ic_v = NULL;
	ic.ic_attach_hook = sio_isa_attach_hook;
	ic.ic_intr_establish = sio_intr_establish;
	ic.ic_intr_disestablish = sio_intr_disestablish;
	ic.ic_intr_alloc = sio_intr_alloc;

	sa.sa_iba.iba_busname = "isa";
	sa.sa_iba.iba_iot = sc->sc_iot;
	sa.sa_iba.iba_memt = sc->sc_memt;
#if NISADMA > 0
	sa.sa_iba.iba_dmat =
		alphabus_dma_get_tag(sc->sc_dmat, ALPHA_BUS_ISA);
#endif
	sa.sa_iba.iba_ic = &ic;
	config_found(&sc->sc_dv, &sa.sa_iba, sioprint);
}

int
sioprint(aux, pnp)
	void *aux;
	const char *pnp;
{
        register union sio_attach_args *sa = aux;

        if (pnp)
                printf("%s at %s", sa->sa_name, pnp);
        return (UNCONF);
}

void
sio_isa_attach_hook(parent, self, iba)
	struct device *parent, *self;
	struct isabus_attach_args *iba;
{
	/* Nothing to do. */
}

void
sio_eisa_attach_hook(parent, self, eba)
	struct device *parent, *self;
	struct eisabus_attach_args *eba;
{

	/* Nothing to do. */
}

int
sio_eisa_maxslots(v)
	void *v;
{

	return 16;		/* as good a number as any.  only 8, maybe? */
}

int
sio_eisa_intr_map(v, irq, ihp)
	void *v;
	u_int irq;
	eisa_intr_handle_t *ihp;
{

#define	ICU_LEN		16	/* number of ISA IRQs (XXX) */

	if (irq >= ICU_LEN) {
		printf("sio_eisa_intr_map: bad IRQ %d\n", irq);
		*ihp = -1;
		return 1;
	}
	if (irq == 2) {
		printf("sio_eisa_intr_map: changed IRQ 2 to IRQ 9\n");
		irq = 9;
	}

	*ihp = irq;
	return 0;
}
