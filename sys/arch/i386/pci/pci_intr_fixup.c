/*	$OpenBSD: pci_intr_fixup.c,v 1.3 2000/03/28 03:37:59 mickey Exp $	*/
/*	$NetBSD: pci_intr_fixup.c,v 1.4 2000/01/25 17:20:47 augustss Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1999, by UCHIYAMA Yasushi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

/*
 * PCI Interrupt Router support.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <i386/isa/icu.h>
#include <i386/pci/pci_intr_fixup.h>
#include <i386/pci/pcibios.h>

struct pciintr_link_map {
	int link;
	int clink;
	int irq;
	u_int16_t bitmap;
	int fixup_stage;
	int old_irq;
	SIMPLEQ_ENTRY(pciintr_link_map) list;
};

pciintr_icu_tag_t pciintr_icu_tag;
pciintr_icu_handle_t pciintr_icu_handle;

struct pciintr_link_map *pciintr_link_lookup_pin
	__P((struct pcibios_intr_routing *, int));
struct pciintr_link_map *pciintr_link_lookup_link __P((int));
struct pciintr_link_map *pciintr_link_alloc __P((struct pcibios_intr_routing *,
	int));
struct pcibios_intr_routing *pciintr_pir_lookup __P((int, int));
int	pciintr_link_init __P((void));
int	pciintr_link_fixup __P((void));
int	pciintr_link_route __P((u_int16_t *));
int	pciintr_irq_release __P((u_int16_t *));
int	pciintr_header_fixup __P((pci_chipset_tag_t));

SIMPLEQ_HEAD(, pciintr_link_map) pciintr_link_map_list;

const struct pciintr_icu_table {
	pci_vendor_id_t	piit_vendor;
	pci_product_id_t piit_product;
	int (*piit_init) __P((pci_chipset_tag_t,
		bus_space_tag_t, pcitag_t, pciintr_icu_tag_t *,
		pciintr_icu_handle_t *));
} pciintr_icu_table[] = {
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82371MX,
	  piix_init },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82371AB_ISA,
	  piix_init },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82371FB_ISA,
	  piix_init },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82371SB_ISA,
	  piix_init },

	{ PCI_VENDOR_OPTI,	PCI_PRODUCT_OPTI_82C558,
	  opti82c558_init },
	{ PCI_VENDOR_OPTI,	PCI_PRODUCT_OPTI_82C700,
	  opti82c700_init },

	{ PCI_VENDOR_VIATECH,	PCI_PRODUCT_VIATECH_VT82C586_ISA,
	  via82c586_init, },

	{ PCI_VENDOR_SIS,	PCI_PRODUCT_SIS_85C503,
	  sis85c503_init },

	{ 0,			0,
	  NULL },
};

const struct pciintr_icu_table *pciintr_icu_lookup __P((pcireg_t));

const struct pciintr_icu_table *
pciintr_icu_lookup(id)
	pcireg_t id;
{
	const struct pciintr_icu_table *piit;

	for (piit = pciintr_icu_table;
	     piit->piit_init != NULL;
	     piit++) {
		if (PCI_VENDOR(id) == piit->piit_vendor &&
		    PCI_PRODUCT(id) == piit->piit_product)
			return (piit);
	}

	return (NULL);
}

struct pciintr_link_map *
pciintr_link_lookup_pin(pir, pin)
	struct pcibios_intr_routing *pir;
	int pin;
{

	return (pciintr_link_lookup_link(pir->linkmap[pin].link));
}

struct pciintr_link_map *
pciintr_link_lookup_link(link)
	int link;
{
	struct pciintr_link_map *l;

	for (l = SIMPLEQ_FIRST(&pciintr_link_map_list); l != NULL;
	     l = SIMPLEQ_NEXT(l, list)) {
		if (l->link == link)
			return (l);
	}

	return (NULL);
}

struct pciintr_link_map *
pciintr_link_alloc(pir, pin)
	struct pcibios_intr_routing *pir;
	int pin;
{
	struct pciintr_link_map *l, *lstart;

	l = malloc(sizeof(*l), M_DEVBUF, M_NOWAIT);
	if (l == NULL)
		panic("pciintr_link_alloc");

	memset(l, 0, sizeof(*l));

	l->link = pir->linkmap[pin].link;
	l->bitmap = pir->linkmap[pin].bitmap;

	lstart = SIMPLEQ_FIRST(&pciintr_link_map_list);
	if (lstart == NULL || lstart->link < l->link)
		SIMPLEQ_INSERT_TAIL(&pciintr_link_map_list, l, list);
	else
		SIMPLEQ_INSERT_HEAD(&pciintr_link_map_list, l, list);

	return (l);
}

struct pcibios_intr_routing *
pciintr_pir_lookup(bus, device)
	int bus, device;
{
	struct pcibios_intr_routing *pir;
	int entry;

	if (pcibios_pir_table == NULL)
		return (NULL);

	for (entry = 0; entry < pcibios_pir_table_nentries; entry++) {
		pir = &pcibios_pir_table[entry];
		if (pir->bus == bus && ((pir->device >> 3) & 0x1f) == device)
			return (pir);
	}

	return (NULL);
}

int
pciintr_link_init()
{
	int entry, pin, error, link, clink;
	struct pcibios_intr_routing *pir;
	struct pciintr_link_map *l;

	if (pcibios_pir_table == NULL) {
		/* No PIR table; can't do anything. */
		printf("pciintr_link_init: no PIR table\n");
		return (1);
	}

	error = 0;
	SIMPLEQ_INIT(&pciintr_link_map_list);

	for (entry = 0; entry < pcibios_pir_table_nentries; entry++) {
		pir = &pcibios_pir_table[entry];
		for (pin = 0; pin < 4; pin++) {
			link = pir->linkmap[pin].link;
			if (link == 0) {
				/* No connection for this pin. */
				continue;
			}

			/*
			 * Check the link value by asking the ICU for
			 * the canonical link value.
			 */
			if (pciintr_icu_getclink(pciintr_icu_tag,
			    pciintr_icu_handle, link, &clink) != 0) {
				/*
				 * Table entry is bogus.  Just ignore it.
				 */
#ifdef PCIINTR_DEBUG
				printf("pciintr_link_init: bad table entry: "
				    "bus %d device %d link 0x%02x\n",
				    pir->bus, (pir->device >> 3 & 0x1f), link);
#endif
				continue;
			}

			/*
			 * Multiple devices may be wired to the same
			 * interrupt; check to see if we've seen this
			 * one already.  If not, allocate a new link
			 * map entry and stuff it in the map.
			 */
			l = pciintr_link_lookup_pin(pir, pin);
			if (l == NULL)
				(void) pciintr_link_alloc(pir, pin);
		}
	}

	return (error);
}

int
pciintr_link_fixup()
{
	struct pciintr_link_map *l;
	u_int16_t pciirq, bitmap;
	int i, j, cnt, irq;

	/*
	 * First stage: Attempt to connect PIRQs which aren't
	 * yet connected.
	 */
	pciirq = 0;

	for (l = SIMPLEQ_FIRST(&pciintr_link_map_list); l != NULL;
	     l = SIMPLEQ_NEXT(l, list)) {
		/*
		 * Get the canonical link value for this entry.
		 */
		if (pciintr_icu_getclink(pciintr_icu_tag, pciintr_icu_handle,
		    l->link, &l->clink) != 0) {
			/*
			 * ICU doesn't understand this link value.
			 */
#ifdef PCIINTR_DEBUG
			printf("pciintr_link_fixup: link 0x%02x invalid\n",
			    l->link);
#endif
			l->clink = -1;
			continue;
		}

		/*
		 * Determine if this PIRQ is mapped to an IRQ.
		 */
		if (pciintr_icu_get_intr(pciintr_icu_tag, pciintr_icu_handle,
		    l->clink, &irq) != 0) {
			/*
			 * ICU doesn't understand this PIRQ value.
			 */
			l->clink = -1;
#ifdef PCIINTR_DEBUG
			printf("pciintr_link_fixup: PIRQ %d invalid\n",
			    l->clink);
#endif
			continue;
		}

		if (irq == 0xff) {
			/*
			 * Interrupt isn't connected.  Attempt to assign
			 * it to an IRQ.
			 */
#ifdef PCIINTR_DEBUG
			printf("pciintr_link_fixup: PIRQ %d not connected",
			    l->clink);
#endif
			bitmap = l->bitmap;
			for (i = 0, j = 0xff, cnt = 0; i < 16; i++)
				if (bitmap & (1 << i))
					j = i, cnt++;
			/*
			 * Just do the easy case now; we'll defer the
			 * harder ones to Stage 2.
			 */
			if (cnt == 1) {
				l->irq = j;
				l->old_irq = irq;
				l->fixup_stage = 1;
				pciirq |= 1 << j;
#ifdef PCIINTR_DEBUG
				printf(", assigning IRQ %d", l->irq);
#endif
			}
#ifdef PCIINTR_DEBUG
			printf("\n");
#endif
		} else {
			/*
			 * Interrupt is already connected.  Don't do
			 * anything to it.
			 */
			l->irq = irq;
			pciirq |= 1 << irq;
#ifdef PCIINTR_DEBUG
			printf("pciintr_link_fixup: PIRQ %d already connected "
			    "to IRQ %d\n", l->clink, l->irq);
#endif
		}
	}

#ifdef PCIBIOS_IRQS
	/* In case the user supplied a mask for the PCI irqs we use it. */
	pciirq = PCIBIOS_IRQS;
#endif

	/*
	 * Stage 2: Attempt to connect PIRQs which we didn't
	 * connect in Stage 1.
	 */
	for (l = SIMPLEQ_FIRST(&pciintr_link_map_list); l != NULL;
	     l = SIMPLEQ_NEXT(l, list)) {
		if (l->irq == 0) {
			bitmap = l->bitmap;
			for (i = 0; i < 16; i++) {
				if ((pciirq & (1 << i)) != 0 &&
				    (bitmap & (1 << i)) != 0) {
					/*
					 * This IRQ is a valid PCI
					 * IRQ already connected to
					 * another PIRQ, and also an
					 * IRQ our PIRQ can use; connect
					 * it up!
					 */
					l->irq = i;
					l->old_irq = 0xff;
					l->fixup_stage = 2;
#ifdef PCIINTR_DEBUG
					printf("pciintr_link_fixup: assigning "
					    "IRQ %d to PIRQ %d\n", l->irq,
					    l->clink);
#endif
					break;
				}
			}
		}
	}

	/*
	 * Stage 3: Allow the user to specify interrupt routing
	 * information, overriding what we've done above.
	 */
	/* XXX Not implemented. */

	return (0);
}

int
pciintr_link_route(pciirq)
	u_int16_t *pciirq;
{
	struct pciintr_link_map *l;
	int rv = 0;

	*pciirq = 0;

	for (l = SIMPLEQ_FIRST(&pciintr_link_map_list); l != NULL;
	     l = SIMPLEQ_NEXT(l, list)) {
		if (pciintr_icu_set_intr(pciintr_icu_tag, pciintr_icu_handle,
					 l->clink, l->irq) != 0 ||
		    pciintr_icu_set_trigger(pciintr_icu_tag, pciintr_icu_handle,
					    l->irq, IST_LEVEL) != 0) {
			printf("pciintr_link_route: route of PIRQ %d -> IRQ %d"
			    " failed\n", l->clink, l->irq);
			rv = 1;
		} else {
			/*
			 * Succssfully routed interrupt.  Mark this as
			 * a PCI interrupt.
			 */
			*pciirq |= (1 << l->irq);
		}
	}

	return (rv);
}

int
pciintr_irq_release(pciirq)
	u_int16_t *pciirq;
{
	int i;

	for (i = 0; i < 16; i++) {
		if ((*pciirq & (1 << i)) == 0)
			(void) pciintr_icu_set_trigger(pciintr_icu_tag,
			    pciintr_icu_handle, i, IST_EDGE);
	}

	return (0);
}

int
pciintr_header_fixup(pc)
	pci_chipset_tag_t pc;
{
	const struct pci_quirkdata *qd;
	struct pcibios_intr_routing *pir;
	struct pciintr_link_map *l;
	int pin, bus, device, function, maxdevs, nfuncs, irq, link;
	pcireg_t id, bhlcr, intr;
	pcitag_t tag;

#ifdef PCIBIOSVERBOSE
	printf("--------------------------------------------\n");
	printf("  device vendor product pin PIRQ   IRQ stage\n");
	printf("--------------------------------------------\n");
#endif

	for (bus = 0; bus <= pcibios_max_bus; bus++) {
		maxdevs = pci_bus_maxdevs(pc, bus);
		for (device = 0; device < maxdevs; device++) {
			tag = pci_make_tag(pc, bus, device, 0);
			id = pci_conf_read(pc, tag, PCI_ID_REG);

			/* Invalid vendor ID value? */
			if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
				continue;
			/* XXX Not invalid, but we've done this ~forever. */
			if (PCI_VENDOR(id) == 0)
				continue;

			qd = pci_lookup_quirkdata(PCI_VENDOR(id),
			    PCI_PRODUCT(id));

			bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
			if (PCI_HDRTYPE_MULTIFN(bhlcr) ||
			    (qd != NULL &&
			     (qd->quirks & PCI_QUIRK_MULTIFUNCTION) != 0))
				nfuncs = 8;
			else
				nfuncs = 1;

			for (function = 0; function < nfuncs; function++) {
				tag = pci_make_tag(pc, bus, device, function);
				id = pci_conf_read(pc, tag, PCI_ID_REG);
				intr = pci_conf_read(pc, tag,
				    PCI_INTERRUPT_REG);

				/* Invalid vendor ID value? */
				if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
					continue;
				/*
				 * XXX Not invalid, but we've done this
				 * ~forever.
				 */
				if (PCI_VENDOR(id) == 0)
					continue;

				pin = PCI_INTERRUPT_PIN(intr);
				irq = PCI_INTERRUPT_LINE(intr);

				if (pin == 0) {
					/*
					 * No interrupt used.
					 */
					continue;
				}

				pir = pciintr_pir_lookup(bus, device);
				if (pir == NULL ||
				    (link = pir->linkmap[pin - 1].link) == 0) {
					/*
					 * Interrupt not connected; no
					 * need to change.
					 */
					continue;
				}

				l = pciintr_link_lookup_link(link);
				if (l == NULL) {
					/*
					 * No link map entry?!
					 */
					printf("pciintr_header_fixup: no entry "
					    "for link 0x%02x (%d:%d:%d:%c)\n",
					    link, bus, device, function,
					    '@' + pin);
					continue;
				}

				/*
				 * IRQs 14 and 15 are reserved for
				 * PCI IDE interrupts; don't muck
				 * with them.
				 */
				if (irq == 14 || irq == 15)
					continue;

#ifdef PCIBIOSVERBOSE
				printf("%03d:%02d:%d 0x%04x 0x%04x  %c   "
				    "0x%02x   %02d  %d\n",
				    bus, device, function,
				    PCI_VENDOR(id), PCI_PRODUCT(id),
				    '@' + pin, l->clink, l->irq,
				    l->fixup_stage);
#endif

				intr &= ~(PCI_INTERRUPT_LINE_MASK <<
				    PCI_INTERRUPT_LINE_SHIFT);
				intr |= (l->irq << PCI_INTERRUPT_LINE_SHIFT);
				pci_conf_write(pc, tag, PCI_INTERRUPT_REG,
				    intr);
			}
		}
	}

#ifdef PCIBIOSVERBOSE
	printf("--------------------------------------------\n");
#endif

	return (0);
}

int
pci_intr_fixup(pc, iot, pciirq)
	pci_chipset_tag_t pc;
	bus_space_tag_t iot;
	u_int16_t *pciirq;
{
	const struct pciintr_icu_table *piit = NULL;
	pcitag_t icutag;
	pcireg_t icuid;

	/*
	 * Attempt to initialize our PCI interrupt router.  If
	 * the PIR Table is present in ROM, use the location
	 * specified by the PIR Table, and use the compat ID,
	 * if present.  Otherwise, we have to look for the router
	 * ourselves (the PCI-ISA bridge).
	 */
	if (pcibios_pir_header.signature != 0) {
		icutag = pci_make_tag(pc, pcibios_pir_header.router_bus,
		    (pcibios_pir_header.router_devfunc >> 3) & 0x1f,
		    pcibios_pir_header.router_devfunc & 7);
		icuid = pcibios_pir_header.compat_router;
		if (icuid == 0 ||
		    (piit = pciintr_icu_lookup(icuid)) == NULL) {
			/*
			 * No compat ID, or don't know the compat ID?  Read
			 * it from the configuration header.
			 */
			icuid = pci_conf_read(pc, icutag, PCI_ID_REG);
		}
		if (piit == NULL)
			piit = pciintr_icu_lookup(icuid);
	} else {
		int device, maxdevs = pci_bus_maxdevs(pc, 0);

		/*
		 * Search configuration space for a known interrupt
		 * router.
		 */
		for (device = 0; device < maxdevs; device++) {
			icutag = pci_make_tag(pc, 0, device, 0);
			icuid = pci_conf_read(pc, icutag, PCI_ID_REG);

			/* Invalid vendor ID value? */
			if (PCI_VENDOR(icuid) == PCI_VENDOR_INVALID)
				continue;
			/* XXX Not invalid, but we've done this ~forever. */
			if (PCI_VENDOR(icuid) == 0)
				continue;

			piit = pciintr_icu_lookup(icuid);
			if (piit != NULL)
				break;
		}
	}

	if (piit == NULL) {
		printf("pci_intr_fixup: no compatible PCI ICU found\n");
		return (-1);		/* non-fatal */
	}

	/*
	 * Initialize the PCI ICU.
	 */
	if ((*piit->piit_init)(pc, iot, icutag, &pciintr_icu_tag,
	    &pciintr_icu_handle) != 0)
		return (-1);		/* non-fatal */

	/*
	 * Initialize the PCI interrupt link map.
	 */
	if (pciintr_link_init())
		return (-1);		/* non-fatal */

	/*
	 * Fix up the link->IRQ mappings.
	 */
	if (pciintr_link_fixup() != 0)
		return (-1);		/* non-fatal */

	/*
	 * Now actually program the PCI ICU with the new
	 * routing information.
	 */
	if (pciintr_link_route(pciirq) != 0)
		return (1);		/* fatal */

	/*
	 * Now that we've routed all of the PIRQs, rewrite the PCI
	 * configuration headers to reflect the new mapping.
	 */
	if (pciintr_header_fixup(pc) != 0)
		return (1);		/* fatal */

	/*
	 * Free any unused PCI IRQs for ISA devices.
	 */
	if (pciintr_irq_release(pciirq) != 0)
		return (-1);		/* non-fatal */

	/*
	 * All done!
	 */
	return (0);			/* success! */
}
