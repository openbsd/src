/*	$OpenBSD: acpiprt.c,v 1.18 2007/10/08 04:15:15 krw Exp $	*/
/*
 * Copyright (c) 2006 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/ppbreg.h>

#include <machine/i82093reg.h>
#include <machine/i82093var.h>

#include <machine/mpbiosvar.h>

#include "ioapic.h"

int	acpiprt_match(struct device *, void *, void *);
void	acpiprt_attach(struct device *, struct device *, void *);
int	acpiprt_getirq(union acpi_resource *crs, void *arg);
int	acpiprt_getminbus(union acpi_resource *, void *);


struct acpiprt_softc {
	struct device		sc_dev;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	int			sc_bus;
};

struct cfattach acpiprt_ca = {
	sizeof(struct acpiprt_softc), acpiprt_match, acpiprt_attach
};

struct cfdriver acpiprt_cd = {
	NULL, "acpiprt", DV_DULL
};

void	acpiprt_prt_add(struct acpiprt_softc *, struct aml_value *);
int	acpiprt_getpcibus(struct acpiprt_softc *, struct aml_node *);

int
acpiprt_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args	*aa = aux;
	struct cfdata  *cf = match;

	/* sanity */
	if (aa->aaa_name == NULL ||
	    strcmp(aa->aaa_name, cf->cf_driver->cd_name) != 0 ||
	    aa->aaa_table != NULL)
		return (0);

	return (1);
}

void
acpiprt_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpiprt_softc *sc = (struct acpiprt_softc *)self;
	struct acpi_attach_args *aa = aux;
	struct aml_value res;
	int i;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node;
	sc->sc_bus = acpiprt_getpcibus(sc, sc->sc_devnode);

	printf(": bus %d (%s)", sc->sc_bus, sc->sc_devnode->parent->name);

	if (aml_evalnode(sc->sc_acpi, sc->sc_devnode, 0, NULL, &res)) {
		printf(": no PCI interrupt routing table\n");
		return;
	}

	if (res.type != AML_OBJTYPE_PACKAGE) {
		printf(": _PRT is not a package\n");
		aml_freevalue(&res);
		return;
	}

	printf("\n");

	if (sc->sc_bus == -1)
		return;

	for (i = 0; i < res.length; i++)
		acpiprt_prt_add(sc, res.v_package[i]);

	aml_freevalue(&res);
}

int
acpiprt_getirq(union acpi_resource *crs, void *arg)
{
	int *irq = (int *)arg;
	int typ;

	typ = AML_CRSTYPE(crs);
	switch (typ) {
	case SR_IRQ:
		*irq = ffs(aml_letohost16(crs->sr_irq.irq_mask)) - 1;
		break;
	case LR_EXTIRQ:
		*irq = aml_letohost32(crs->lr_extirq.irq[0]);
		break;
	default:
		printf("Unknown interrupt : %x\n", typ);
	}
	return (0);
}

void
acpiprt_prt_add(struct acpiprt_softc *sc, struct aml_value *v)
{
	struct aml_node	*node;
	struct aml_value res, *pp;
	u_int64_t addr;
	int pin, irq, sta;
#if NIOAPIC > 0
	struct mp_intr_map *map;
	struct ioapic_softc *apic;
#endif
	pci_chipset_tag_t pc = NULL;
	pcitag_t tag;
	pcireg_t reg;
	int bus, dev, func, nfuncs;

	if (v->type != AML_OBJTYPE_PACKAGE || v->length != 4) {
		printf("invalid mapping object\n");
		return;
	}

	addr = aml_val2int(v->v_package[0]);
	pin = aml_val2int(v->v_package[1]);
	if (pin > 3) {
		return;
	}

	pp = v->v_package[2];
	if (pp->type == AML_OBJTYPE_NAMEREF) {
		node = aml_searchname(sc->sc_devnode, pp->v_nameref);
		if (node == NULL) {
			printf("Invalid device!\n");
			return;
		}
		pp = node->value;
	}
	if (pp->type == AML_OBJTYPE_OBJREF) {
		pp = pp->v_objref.ref;
	}
	if (pp->type == AML_OBJTYPE_DEVICE) {
		node = pp->node;
		if (aml_evalname(sc->sc_acpi, node, "_STA", 0, NULL, &res))
			printf("no _STA method\n");

		sta = aml_val2int(&res) & STA_ENABLED;
		aml_freevalue(&res);
		if (sta == 0)
			return;

		if (aml_evalname(sc->sc_acpi, node, "_CRS", 0, NULL, &res))
			printf("no _CRS method\n");

		if (res.type != AML_OBJTYPE_BUFFER || res.length < 6) {
			printf("invalid _CRS object\n");
			aml_freevalue(&res);
			return;
		}
		aml_parse_resource(res.length, res.v_buffer,
		    acpiprt_getirq, &irq);
		aml_freevalue(&res);
	} else {
		irq = aml_val2int(v->v_package[3]);
	}

#ifdef ACPI_DEBUG
	printf("%s: %s addr 0x%llx pin %d irq %d\n",
	    DEVNAME(sc), aml_nodename(pp->node), addr, pin, irq);
#endif

#if NIOAPIC > 0
	if (nioapics > 0) {
		apic = ioapic_find_bybase(irq);
		if (apic == NULL) {
			printf("%s: no apic found for irq %d\n", DEVNAME(sc), irq);
			return;
		}

		map = malloc(sizeof(*map), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (map == NULL)
			return;

		map->ioapic = apic;
		map->ioapic_pin = irq - apic->sc_apic_vecbase;
		map->bus_pin = ((addr >> 14) & 0x7c) | (pin & 0x3);
		map->redir = IOAPIC_REDLO_ACTLO | IOAPIC_REDLO_LEVEL;
		map->redir |= (IOAPIC_REDLO_DEL_LOPRI << IOAPIC_REDLO_DEL_SHIFT);

		map->ioapic_ih = APIC_INT_VIA_APIC |
		    ((apic->sc_apicid << APIC_INT_APIC_SHIFT) |
		    (map->ioapic_pin << APIC_INT_PIN_SHIFT));

		apic->sc_pins[map->ioapic_pin].ip_map = map;

		map->next = mp_busses[sc->sc_bus].mb_intrs;
		mp_busses[sc->sc_bus].mb_intrs = map;

		return;
	}
#endif

	bus = sc->sc_bus;
	dev = ACPI_PCI_DEV(addr << 16);
	tag = pci_make_tag(pc, bus, dev, 0);

	reg = pci_conf_read(pc, tag, PCI_BHLC_REG);
	if (PCI_HDRTYPE_MULTIFN(reg))
		nfuncs = 8;
	else
		nfuncs = 1;

	for (func = 0; func < nfuncs; func++) {
		tag = pci_make_tag(pc, bus, dev, func);
		reg = pci_conf_read(pc, tag, PCI_INTERRUPT_REG);
		if (PCI_INTERRUPT_PIN(reg) == pin + 1) {
			reg &= ~(PCI_INTERRUPT_LINE_MASK << PCI_INTERRUPT_LINE_SHIFT);
			reg |= irq << PCI_INTERRUPT_LINE_SHIFT;
			pci_conf_write(pc, tag, PCI_INTERRUPT_REG, reg);
		}
	}
}

int
acpiprt_getminbus(union acpi_resource *crs, void *arg)
{
	int *bbn = arg;
	int typ = AML_CRSTYPE(crs);

	/* Check for embedded bus number */
	if (typ == LR_WORD && crs->lr_word.type == 2)
		*bbn = crs->lr_word._min;
	return 0;
}

int
acpiprt_getpcibus(struct acpiprt_softc *sc, struct aml_node *node)
{
	struct aml_node *parent = node->parent;
	struct aml_value res;
	pci_chipset_tag_t pc = NULL;
	pcitag_t tag;
	pcireg_t reg;
	int bus, dev, func, rv;

	if (parent == NULL)
		return 0;

	/*
	 * If our parent is a a bridge, it might have an address descriptor
	 * that tells us our bus number.
	 */
	if (aml_evalname(sc->sc_acpi, parent, "_CRS.", 0, NULL, &res) == 0) {
		rv = -1;
	  	if (res.type == AML_OBJTYPE_BUFFER)
			aml_parse_resource(res.length, res.v_buffer, 
			    acpiprt_getminbus, &rv);
		aml_freevalue(&res);
		if (rv != -1)
			return rv;
	}

	/*
	 * If our parent is the root of the bus, it should specify the
	 * base bus number.
	 */
	if (aml_evalname(sc->sc_acpi, parent, "_BBN.", 0, NULL, &res) == 0) {
		rv = aml_val2int(&res);
		aml_freevalue(&res);
		return (rv);
	}

	/*
	 * If our parent is a PCI-PCI bridge, get our bus number from its
	 * PCI config space.
	 */
	if (aml_evalname(sc->sc_acpi, parent, "_ADR.", 0, NULL, &res) == 0) {
		bus = acpiprt_getpcibus(sc, parent);
		dev = ACPI_PCI_DEV(aml_val2int(&res) << 16);
		func = ACPI_PCI_FN(aml_val2int(&res) << 16);
		aml_freevalue(&res);

		/*
		 * Some systems return 255 as the device number for
		 * devices that are not really there.
		 */
		if (dev >= pci_bus_maxdevs(pc, bus))
			return (-1);

		tag = pci_make_tag(pc, bus, dev, func);

		/* Check whether the device is really there. */
		reg = pci_conf_read(pc, tag, PCI_ID_REG);
		if (PCI_VENDOR(reg) == PCI_VENDOR_INVALID)
			return (-1);

		/* Fetch bus number from PCI config space. */
		reg = pci_conf_read(pc, tag, PCI_CLASS_REG);
		if (PCI_CLASS(reg) == PCI_CLASS_BRIDGE &&
		    PCI_SUBCLASS(reg) == PCI_SUBCLASS_BRIDGE_PCI) {
			reg = pci_conf_read(pc, tag, PPB_REG_BUSINFO);
			return (PPB_BUSINFO_SECONDARY(reg));
		}
	}

	return (0);
}
