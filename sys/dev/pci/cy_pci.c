/*	$OpenBSD: cy_pci.c,v 1.10 2002/09/14 15:00:03 art Exp $	*/

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

int cy_pci_match(struct device *, void *, void *);
void cy_pci_attach(struct device *, struct device *, void *);

struct cy_pci_softc {
	struct cy_softc 	sc_cy;		/* real softc */

	bus_space_tag_t		sc_iot;		/* PLX i/o tag */
	bus_space_handle_t	sc_ioh;		/* PLX i/o handle */
};

struct cfattach cy_pci_ca = {
	sizeof(struct cy_pci_softc), cy_pci_match, cy_pci_attach
};

#define CY_PLX_9050_ICS_IENABLE		0x040
#define CY_PLX_9050_ICS_LOCAL_IENABLE	0x001
#define CY_PLX_9050_ICS_LOCAL_IPOLARITY	0x002
#define CY_PLX_9060_ICS_IENABLE		0x100
#define CY_PLX_9060_ICS_LOCAL_IENABLE	0x800

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
	struct cy_pci_softc *psc = (struct cy_pci_softc *)self;
	struct cy_softc *sc = (struct cy_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	pcireg_t memtype;
	int plx_ver;

	sc->sc_bustype = CY_BUSTYPE_PCI;

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_CYCLADES_CYCLOMY_1:
	case PCI_PRODUCT_CYCLADES_CYCLOM4Y_1:
	case PCI_PRODUCT_CYCLADES_CYCLOM8Y_1:
		memtype = PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT_1M;
		break;
	case PCI_PRODUCT_CYCLADES_CYCLOMY_2:
	case PCI_PRODUCT_CYCLADES_CYCLOM4Y_2:
	case PCI_PRODUCT_CYCLADES_CYCLOM8Y_2:
		memtype = PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT;
		break;
	}

	if (pci_mapreg_map(pa, 0x14, PCI_MAPREG_TYPE_IO, 0,
	    &psc->sc_iot, &psc->sc_ioh, NULL, NULL, 0) != 0) {
		printf("%s: unable to map PLX registers\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	if (pci_mapreg_map(pa, 0x18, memtype, 0, &sc->sc_memt,
	    &sc->sc_memh, NULL, NULL, 0) != 0) {
                printf("%s: couldn't map device registers\n",
		    sc->sc_dev.dv_xname);
                return;
        }

	if ((sc->sc_nr_cd1400s = cy_probe_common(sc->sc_memt, sc->sc_memh,
	    CY_BUSTYPE_PCI)) == 0) {
		printf("%s: PCI Cyclom card with no CD1400s\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	if (pci_intr_map(pa, &ih) != 0)
		panic("%s: couldn't map PCI interrupt", sc->sc_dev.dv_xname);

	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_TTY, cy_intr,
	    sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL)
		panic("%s: couldn't establish interrupt", sc->sc_dev.dv_xname);

	cy_attach(parent, self);

	/* Get PLX version */
	plx_ver = bus_space_read_1(sc->sc_memt, sc->sc_memh, CY_PLX_VER) & 0x0f;

	/* Enable PCI card interrupts */
	switch (plx_ver) {
	case CY_PLX_9050:
		bus_space_write_2(psc->sc_iot, psc->sc_ioh, CY_PCI_INTENA_9050,
		    CY_PLX_9050_ICS_IENABLE | CY_PLX_9050_ICS_LOCAL_IENABLE |
		    CY_PLX_9050_ICS_LOCAL_IPOLARITY);
		break;

	case CY_PLX_9060:
	case CY_PLX_9080:
	default:
		bus_space_write_2(psc->sc_iot, psc->sc_ioh, CY_PCI_INTENA,
		    bus_space_read_2(psc->sc_iot, psc->sc_ioh,
		    CY_PCI_INTENA) | CY_PLX_9060_ICS_IENABLE | 
		    CY_PLX_9060_ICS_LOCAL_IENABLE);
	}
}
