/*	$OpenBSD: puc.c,v 1.18 2010/08/06 21:04:14 kettenis Exp $	*/
/*	$NetBSD: puc.c,v 1.3 1999/02/06 06:29:54 cgd Exp $	*/

/*
 * Copyright (c) 1996, 1998, 1999
 *	Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/*
 * PCI "universal" communication card device driver, glues com, lpt,
 * and similar ports to PCI via bridge chip often much larger than
 * the devices being glued.
 *
 * Author: Christopher G. Demetriou, May 14, 1998 (derived from NetBSD
 * sys/dev/pci/pciide.c, revision 1.6).
 *
 * These devices could be (and some times are) described as
 * communications/{serial,parallel}, etc. devices with known
 * programming interfaces, but those programming interfaces (in
 * particular the BAR assignments for devices, etc.) in fact are not
 * particularly well defined.
 *
 * After I/we have seen more of these devices, it may be possible
 * to generalize some of these bits.  In particular, devices which
 * describe themselves as communications/serial/16[45]50, and
 * communications/parallel/??? might be attached via direct
 * 'com' and 'lpt' attachments to pci.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pucvar.h>

#include <dev/pci/pcidevs.h>

struct puc_pci_softc {
	struct puc_softc	sc_psc;

	pci_chipset_tag_t	pc;
	pci_intr_handle_t	ih;
};

int	puc_pci_match(struct device *, void *, void *);
void	puc_pci_attach(struct device *, struct device *, void *);
int	puc_pci_detach(struct device *, int);
const char *puc_pci_intr_string(struct puc_attach_args *);
void	*puc_pci_intr_establish(struct puc_attach_args *, int,
    int (*)(void *), void *, char *);

struct cfattach puc_pci_ca = {
	sizeof(struct puc_pci_softc), puc_pci_match,
	puc_pci_attach, puc_pci_detach, config_activate_children
};

struct cfdriver puc_cd = {
	NULL, "puc", DV_DULL
};

const char *puc_port_type_name(int);

int
puc_pci_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;
	const struct puc_device_description *desc;
	pcireg_t bhlc, subsys;

	bhlc = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);
	if (PCI_HDRTYPE_TYPE(bhlc) != 0)
		return (0);

	subsys = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

	desc = puc_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), PCI_VENDOR(subsys), PCI_PRODUCT(subsys));
	if (desc != NULL)
		return (10);

	/*
	 * Match class/subclass, so we can tell people to compile kernel
	 * with options that cause this driver to spew.
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_COMMUNICATIONS &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_PCI)
		return (1);

	return (0);
}

const char *
puc_pci_intr_string(struct puc_attach_args *paa)
{
	struct puc_pci_softc *sc = paa->puc;

	return (pci_intr_string(sc->pc, sc->ih));
}

void *
puc_pci_intr_establish(struct puc_attach_args *paa, int type,
    int (*func)(void *), void *arg, char *name)
{
	struct puc_pci_softc *sc = paa->puc;
	struct puc_softc *psc = &sc->sc_psc;
	
	psc->sc_ports[paa->port].intrhand =
	    pci_intr_establish(sc->pc, sc->ih, type, func, arg, name);

	return (psc->sc_ports[paa->port].intrhand);
}

void
puc_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct puc_pci_softc *psc = (struct puc_pci_softc *)self;
	struct puc_softc *sc = &psc->sc_psc;
	struct pci_attach_args *pa = aux;
	struct puc_attach_args paa;
	pcireg_t subsys;
	int i;

	subsys = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	sc->sc_desc = puc_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), PCI_VENDOR(subsys), PCI_PRODUCT(subsys));
	if (sc->sc_desc == NULL) {
		/*
		 * This was a class/subclass match, so tell people to compile
		 * kernel with options that cause this driver to spew.
		 */
#ifdef PUC_PRINT_REGS
		printf(":\n");
		pci_conf_print(pa->pa_pc, pa->pa_tag, NULL);
#else
		printf(": unknown PCI communications device\n");
		printf("%s: compile kernel with PUC_PRINT_REGS and larger\n",
		    sc->sc_dev.dv_xname);
		printf("%s: message buffer (via 'options MSGBUFSIZE=...'),\n",
		    sc->sc_dev.dv_xname);
		printf("%s: and report the result with sendbug(1)\n",
		    sc->sc_dev.dv_xname);
#endif
		return;
	}

	puc_print_ports(sc->sc_desc);

	/*
	 * XXX This driver assumes that 'com' ports attached to it
	 * XXX can not be console.  That isn't unreasonable, because PCI
	 * XXX devices are supposed to be dynamically mapped, and com
	 * XXX console ports want fixed addresses.  When/if baseboard
	 * XXX 'com' ports are identified as PCI/communications/serial
	 * XXX devices and are known to be mapped at the standard
	 * XXX addresses, if they can be the system console then we have
	 * XXX to cope with doing the mapping right.  Then this will get
	 * XXX really ugly.  Of course, by then we might know the real
	 * XXX definition of PCI/communications/serial, and attach 'com'
	 * XXX directly on PCI.
	 */
	for (i = 0; i < PUC_NBARS; i++) {
		pcireg_t type;
		int bar;

		sc->sc_bar_mappings[i].mapped = 0;
		bar = PCI_MAPREG_START + 4 * i;
		if (!pci_mapreg_probe(pa->pa_pc, pa->pa_tag, bar, &type))
			continue;

		sc->sc_bar_mappings[i].mapped = (pci_mapreg_map(pa, bar, type,
		    0, &sc->sc_bar_mappings[i].t, &sc->sc_bar_mappings[i].h,
		    &sc->sc_bar_mappings[i].a, &sc->sc_bar_mappings[i].s, 0)
		      == 0);
		if (sc->sc_bar_mappings[i].mapped)
			continue;

		printf("%s: couldn't map BAR at offset 0x%lx\n",
		    sc->sc_dev.dv_xname, (long)bar);
	}

	/* Map interrupt. */
	psc->pc = pa->pa_pc;
	if (pci_intr_map(pa, &psc->ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
		return;
	}

	paa.puc = sc;
	paa.hwtype = 0;	/* autodetect */
	paa.intr_string = &puc_pci_intr_string;
	paa.intr_establish = &puc_pci_intr_establish;

	/*
	 * If this is a serial card with a known specific chip, provide
	 * the UART type.
	 */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_PLX &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_PLX_CRONYX_OMEGA)
		paa.hwtype = 0x08;	/* XXX COM_UART_ST16C654 */

	puc_common_attach(sc, &paa);
}

void
puc_common_attach(struct puc_softc *sc, struct puc_attach_args *paa)
{
	int i, bar;

	/*
	 * XXX the sub-devices establish the interrupts, for the
	 * XXX following reasons:
	 * XXX
	 * XXX    * we can't really know what IPLs they'd want
	 * XXX
	 * XXX    * the MD dispatching code can ("should") dispatch
	 * XXX      chained interrupts better than we can.
	 * XXX
	 * XXX It would be nice if we could indicate to the MD interrupt
	 * XXX handling code that the interrupt line used by the device
	 * XXX was a PCI (level triggered) interrupt.
	 * XXX
	 * XXX It's not pretty, but hey, what is?
	 */

	/* Configure each port. */
	for (i = 0; PUC_PORT_VALID(sc->sc_desc, i); i++) {
		/* Skip unknown ports */
		if (sc->sc_desc->ports[i].type != PUC_PORT_TYPE_COM &&
		    sc->sc_desc->ports[i].type != PUC_PORT_TYPE_LPT)
			continue;
		/* make sure the base address register is mapped */
		bar = PUC_PORT_BAR_INDEX(sc->sc_desc->ports[i].bar);
		if (!sc->sc_bar_mappings[bar].mapped) {
			printf("%s: %s port uses unmapped BAR (0x%x)\n",
			    sc->sc_dev.dv_xname,
			    puc_port_type_name(sc->sc_desc->ports[i].type),
			    sc->sc_desc->ports[i].bar);
			continue;
		}

		/* set up to configure the child device */
		paa->port = i;
		paa->type = sc->sc_desc->ports[i].type;
		paa->flags = sc->sc_desc->ports[i].flags;
		paa->a = sc->sc_bar_mappings[bar].a;
		paa->t = sc->sc_bar_mappings[bar].t;

		if (bus_space_subregion(sc->sc_bar_mappings[bar].t,
		    sc->sc_bar_mappings[bar].h, sc->sc_desc->ports[i].offset,
		    sc->sc_bar_mappings[bar].s - sc->sc_desc->ports[i].offset,
		    &paa->h)) {
			printf("%s: couldn't get subregion for port %d\n",
			    sc->sc_dev.dv_xname, i);
			continue;
		}

#if 0
		if (autoconf_verbose)
			printf("%s: port %d: %s @ (index %d) 0x%x "
			    "(0x%lx, 0x%lx)\n", sc->sc_dev.dv_xname, paa->port,
			    puc_port_type_name(paa->type), bar, (int)paa->a,
			    (long)paa->t, (long)paa->h);
#endif

		/* and configure it */
		sc->sc_ports[i].dev = config_found_sm(&sc->sc_dev, paa,
		    puc_print, puc_submatch);
	}
}

int
puc_pci_detach(struct device *self, int flags)
{
	struct puc_pci_softc *sc = (struct puc_pci_softc *)self;
	struct puc_softc *psc = &sc->sc_psc;
	int i, rv;

	for (i = PUC_MAX_PORTS; i--; ) {
		if (psc->sc_ports[i].intrhand)
			pci_intr_disestablish(sc->pc,
			    psc->sc_ports[i].intrhand);
		if (psc->sc_ports[i].dev)
			if ((rv = config_detach(psc->sc_ports[i].dev, flags)))
				return (rv);
	}

	for (i = PUC_NBARS; i--; )
		if (psc->sc_bar_mappings[i].mapped)
			bus_space_unmap(psc->sc_bar_mappings[i].t,
			    psc->sc_bar_mappings[i].h,
			    psc->sc_bar_mappings[i].s);

	return (0);
}

int
puc_print(void *aux, const char *pnp)
{
	struct puc_attach_args *paa = aux;

	if (pnp)
		printf("%s at %s", puc_port_type_name(paa->type), pnp);
	printf(" port %d", paa->port);
	return (UNCONF);
}

int
puc_submatch(struct device *parent, void *vcf, void *aux)
{
	struct cfdata *cf = (struct cfdata *)vcf;
	struct puc_attach_args *aa = aux;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != aa->port)
		return 0;
	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

const struct puc_device_description *
puc_find_description(u_int16_t vend, u_int16_t prod,
    u_int16_t svend, u_int16_t sprod)
{
	int i;

	for (i = 0; !(puc_devs[i].rval[0] == 0 && puc_devs[i].rval[1] == 0 &&
	    puc_devs[i].rval[2] == 0 && puc_devs[i].rval[3] == 0); i++)
		if ((vend & puc_devs[i].rmask[0]) == puc_devs[i].rval[0] &&
		    (prod & puc_devs[i].rmask[1]) == puc_devs[i].rval[1] &&
		    (svend & puc_devs[i].rmask[2]) == puc_devs[i].rval[2] &&
		    (sprod & puc_devs[i].rmask[3]) == puc_devs[i].rval[3])
			return (&puc_devs[i]);

	return (NULL);
}

const char *
puc_port_type_name(int type)
{

	switch (type) {
	case PUC_PORT_TYPE_COM:
		return "com";
	case PUC_PORT_TYPE_LPT:
		return "lpt";
	default:
		return "unknown";
	}
}

void
puc_print_ports(const struct puc_device_description *desc)
{
	int i, ncom, nlpt;

	printf(": ports: ");
	for (i = ncom = nlpt = 0; PUC_PORT_VALID(desc, i); i++) {
		switch (desc->ports[i].type) {
		case PUC_PORT_TYPE_COM:
			ncom++;
		break;
		case PUC_PORT_TYPE_LPT:
			nlpt++;
		break;
		default:
			printf("port %d unknown type %d ", i,
			    desc->ports[i].type);
		}
	}
	if (ncom)
		printf("%d com", ncom);
	if (nlpt) {
		if (ncom)
			printf(", ");
		printf("%d lpt", nlpt);
	}
	printf("\n");
}
