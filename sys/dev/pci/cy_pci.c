/*	$OpenBSD: cy_pci.c,v 1.8 2001/08/25 10:13:29 art Exp $	*/

/*
 * cy_pci.c
 *
 * Driver for Cyclades Cyclom-8/16/32 multiport serial cards
 * (currently not tested with Cyclom-32 cards)
 *
 * Timo Rossi, 1996
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/cd1400reg.h>
#include <dev/ic/cyreg.h>

int cy_pci_match __P((struct device *, void *, void *));
void cy_pci_attach __P((struct device *, struct device *, void *));

struct cfattach cy_pci_ca = {
	sizeof(struct cy_softc), cy_pci_match, cy_pci_attach
};

int
cy_pci_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_CYCLADES)
		return (0);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_CYCLADES_CYCLOMY_1:
	case PCI_PRODUCT_CYCLADES_CYCLOMY_2:
	case PCI_PRODUCT_CYCLADES_CYCLOM4Y_1:
	case PCI_PRODUCT_CYCLADES_CYCLOM4Y_2:
	case PCI_PRODUCT_CYCLADES_CYCLOM8Y_1:
	case PCI_PRODUCT_CYCLADES_CYCLOM8Y_2:
		break;
	default:
		return (0);
	}

#ifdef CY_DEBUG
	printf("cy: Found Cyclades PCI device, id = 0x%x\n", pa->pa_id);
#endif

	return (1);
}

void
cy_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct cy_softc *sc = (struct cy_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_addr_t memaddr;
	bus_size_t memsize;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_addr_t iobase;
	bus_size_t iosize;
	int plx_ver;
	int cacheable;

        memt = pa->pa_memt;
        iot = pa->pa_iot;

        if (pci_mem_find(pa->pa_pc, pa->pa_tag, 0x18,
            &memaddr, &memsize, &cacheable) != 0) {
                printf("%s: can't find PCI card memory",
		    sc->sc_dev.dv_xname);
                return;
        }

        /* map the memory (non-cacheable) */
        if (bus_space_map(memt, memaddr, memsize, 0, &memh) != 0) {
                printf("%s: couldn't map PCI memory region\n",
		    sc->sc_dev.dv_xname);
                return;
        }

        /* the PCI Cyclom IO space is only used for enabling interrupts */
        if (pci_io_find(pa->pa_pc, pa->pa_tag, 0x14, &iobase, &iosize) != 0) {
                bus_space_unmap(memt, memh, memsize);
                printf("%s: couldn't find PCI io region\n",
		    sc->sc_dev.dv_xname);
                return;
        }

        if (bus_space_map(iot, iobase, iosize, 0, &ioh) != 0) {
                bus_space_unmap(memt, memh, memsize);
                printf("%s: couldn't map PCI io region\n",
		    sc->sc_dev.dv_xname);
                return;
        }

#ifdef CY_DEBUG
        printf("%s: pci mapped mem 0x%lx (size %d), io 0x%x (size %d)\n",
            sc->sc_dev.dv_xname, memaddr, memsize, iobase, iosize);
#endif

        if (cy_probe_common(sc->sc_dev.dv_unit, memt, memh,
	    CY_BUSTYPE_PCI) == 0) {
                bus_space_unmap(memt, memh, memsize);
                bus_space_unmap(iot, ioh, iosize);
                printf("%s: PCI Cyclom card with no CD1400s!?\n",
		    sc->sc_dev.dv_xname);
                return;
        }

	cy_attach(parent, self, aux);

        /* Get PLX version */
	memt = pa->pa_memt;
	iot = pa->pa_iot;
        plx_ver = bus_space_read_1(memt, memh, CY_PLX_VER) & 0x0f;

        /* Enable PCI card interrupts */
        switch (plx_ver) {
        case CY_PLX_9050:
                bus_space_write_2(iot, ioh, CY_PCI_INTENA_9050,
                bus_space_read_2(iot, ioh, CY_PCI_INTENA_9050) | 0x40);
                break;
        case CY_PLX_9060:
        case CY_PLX_9080:
        default:
                bus_space_write_2(iot, ioh, CY_PCI_INTENA,
                bus_space_read_2(iot, ioh, CY_PCI_INTENA) | 0x900);
        }
 
	/* Enable PCI card interrupts */
	if (pci_intr_map(pa, &ih) != 0)
		panic("%s: couldn't map PCI interrupt", sc->sc_dev.dv_xname);

	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_TTY, cy_intr,
	    sc, sc->sc_dev.dv_xname);

        if (sc->sc_ih == NULL)
                panic("%s: couldn't establish interrupt", sc->sc_dev.dv_xname);
}
