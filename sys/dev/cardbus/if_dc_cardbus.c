/*	$OpenBSD: if_dc_cardbus.c,v 1.12 2002/07/23 17:34:14 drahn Exp $	*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/cardbus/cardbusdevs.h>

#include <dev/ic/dcreg.h>

/* PCI configuration regs */
#define	PCI_CBIO	0x10
#define	PCI_CBMEM	0x14
#define	PCI_CFDA	0x40

#define	DC_CFDA_SUSPEND	0x80000000
#define	DC_CFDA_STANDBY	0x40000000

struct dc_cardbus_softc {
	struct dc_softc		sc_dc;
	int			sc_intrline;

	cardbus_devfunc_t	sc_ct;
	cardbustag_t		sc_tag;
	bus_size_t		sc_mapsize;
	int			sc_actype;
};

int dc_cardbus_match(struct device *, void *, void *);
void dc_cardbus_attach(struct device *, struct device *,void *);
int dc_cardbus_detach(struct device *, int);

void dc_cardbus_setup(struct dc_cardbus_softc *csc);

struct cfattach dc_cardbus_ca = {
	sizeof(struct dc_cardbus_softc), dc_cardbus_match, dc_cardbus_attach,
	    dc_cardbus_detach
};

struct dc_type dc_cardbus_devs[] = {
	{ PCI_VENDOR_DEC, PCI_PRODUCT_DEC_21142 },
	{ PCI_VENDOR_XIRCOM, PCI_PRODUCT_XIRCOM_X3201_3_21143 },
	{ PCI_VENDOR_ADMTEK, PCI_PRODUCT_ADMTEK_AN985 },
	{ PCI_VENDOR_ACCTON, PCI_PRODUCT_ACCTON_EN2242 },
	{ CARDBUS_VENDOR_ABOCOM, CARDBUS_PRODUCT_ABOCOM_FE2500 },
	{ CARDBUS_VENDOR_ABOCOM, CARDBUS_PRODUCT_ABOCOM_PCM200 },
	{ 0 }
};

int
dc_cardbus_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cardbus_attach_args *ca = aux;
	struct dc_type *t;

	for (t = dc_cardbus_devs; t->dc_vid != 0; t++) {
		if ((PCI_VENDOR(ca->ca_id) == t->dc_vid) &&
		    (PCI_PRODUCT(ca->ca_id) == t->dc_did))
			return (1);
	}

	return (0);
}

void
dc_cardbus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct dc_cardbus_softc *csc = (struct dc_cardbus_softc *)self;
	struct dc_softc *sc = &csc->sc_dc;
	struct cardbus_attach_args *ca = aux;
	struct cardbus_devfunc *ct = ca->ca_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	cardbusreg_t reg;
	bus_addr_t addr;

	sc->sc_dmat = ca->ca_dmat;
	sc->dc_unit = sc->sc_dev.dv_unit;
	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;

	Cardbus_function_enable(ct);

	if (Cardbus_mapreg_map(ct, PCI_CBIO,
	    PCI_MAPREG_TYPE_IO, 0, &sc->dc_btag, &sc->dc_bhandle, &addr,
	    &csc->sc_mapsize) == 0) {

		csc->sc_actype = CARDBUS_IO_ENABLE;
	} else if (Cardbus_mapreg_map(ct, PCI_CBMEM,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->dc_btag, &sc->dc_bhandle, &addr, &csc->sc_mapsize) == 0) {
		csc->sc_actype = CARDBUS_MEM_ENABLE;
	} else {
		printf(": can\'t map device registers\n");
		return;
	}

	csc->sc_intrline = ca->ca_intrline;

	sc->dc_cachesize = cardbus_conf_read(cc, cf, ca->ca_tag, DC_PCI_CFLT)
	    & 0xFF;

	dc_cardbus_setup(csc);

	switch (PCI_VENDOR(ca->ca_id)) {
	case PCI_VENDOR_DEC:
		if (PCI_PRODUCT(ca->ca_id) == PCI_PRODUCT_DEC_21142) {
			sc->dc_type = DC_TYPE_21143;
			sc->dc_flags |= DC_TX_POLL|DC_TX_USE_TX_INTR;
			sc->dc_flags |= DC_REDUCED_MII_POLL;
			dc_eeprom_width(sc);
			dc_read_srom(sc, sc->dc_romwidth);
			dc_parse_21143_srom(sc);
		}
		break;
	case PCI_VENDOR_XIRCOM:
		if (PCI_PRODUCT(ca->ca_id) ==
		    PCI_PRODUCT_XIRCOM_X3201_3_21143) {
			sc->dc_type = DC_TYPE_XIRCOM;
			sc->dc_flags |= DC_TX_INTR_ALWAYS|DC_TX_COALESCE;
			sc->dc_pmode = DC_PMODE_MII;

			bcopy(ca->ca_cis.funce.network.netid,
			    &sc->sc_arpcom.ac_enaddr,
			    sizeof sc->sc_arpcom.ac_enaddr);
		}
		break;
	case PCI_VENDOR_ADMTEK:
	case PCI_VENDOR_ACCTON:
	case CARDBUS_VENDOR_ABOCOM:
		if (PCI_PRODUCT(ca->ca_id) == PCI_PRODUCT_ADMTEK_AN985 ||
		    PCI_PRODUCT(ca->ca_id) == PCI_PRODUCT_ACCTON_EN2242 ||
		    PCI_PRODUCT(ca->ca_id) == CARDBUS_PRODUCT_ABOCOM_FE2500 ||
		    PCI_PRODUCT(ca->ca_id) == CARDBUS_PRODUCT_ABOCOM_PCM200) {
			sc->dc_type = DC_TYPE_AN983;
			sc->dc_flags |= DC_TX_USE_TX_INTR|DC_TX_ADMTEK_WAR;
			sc->dc_pmode = DC_PMODE_MII;
			dc_eeprom_width(sc);
			dc_read_srom(sc, sc->dc_romwidth);
		}
		break;
	default:
		printf(": unknown device\n");
		return;
	}

 	/*
	 * set latency timer, do we really need this?
	 */
	reg = cardbus_conf_read(cc, cf, ca->ca_tag, PCI_BHLC_REG);
	if (PCI_LATTIMER(reg) < 0x20) {
		reg &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
		reg |= (0x20 << PCI_LATTIMER_SHIFT);
		cardbus_conf_write(cc, cf, ca->ca_tag, PCI_BHLC_REG, reg);
	}

	sc->sc_ih = cardbus_intr_establish(cc, cf, ca->ca_intrline, IPL_NET,
	    dc_intr, csc);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt at %d\n",
		    ca->ca_intrline);
		return;
	} else
		printf(" irq %d", ca->ca_intrline);

	dc_reset(sc);

	sc->dc_revision = PCI_REVISION(ca->ca_class);
	dc_attach(sc);
}

int
dc_cardbus_detach(self, flags)
	struct device *self;
	int flags;
{
	struct dc_cardbus_softc *csc = (struct dc_cardbus_softc *)self;
	struct dc_softc *sc = &csc->sc_dc;
	struct cardbus_devfunc *ct = csc->sc_ct;
	int rv = 0;

	rv = dc_detach(sc);
	if (rv)
		return (rv);

	cardbus_intr_disestablish(ct->ct_cc, ct->ct_cf, sc->sc_ih);

	/* unmap cardbus resources */
	Cardbus_mapreg_unmap(ct,
	    csc->sc_actype == CARDBUS_IO_ENABLE ? PCI_CBIO : PCI_CBMEM,
	    sc->dc_btag, sc->dc_bhandle, csc->sc_mapsize);

	return (rv);
}

void
dc_cardbus_setup(csc)
	struct dc_cardbus_softc *csc;
{
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	cardbusreg_t reg;
	int r;

	/* wakeup the card if needed */
	reg = cardbus_conf_read(cc, cf, csc->sc_tag, PCI_CFDA);
	if (reg | (DC_CFDA_SUSPEND|DC_CFDA_STANDBY)) {
		cardbus_conf_write(cc, cf, csc->sc_tag, PCI_CFDA,
		    reg & ~(DC_CFDA_SUSPEND|DC_CFDA_STANDBY));
	}

	if (cardbus_get_capability(cc, cf, csc->sc_tag, PCI_CAP_PWRMGMT, &r,
	    0)) {
		r = cardbus_conf_read(cc, cf, csc->sc_tag, r + 4) & 3;
		if (r) {
			printf("%s: awakening from state D%d\n",
			    csc->sc_dc.sc_dev.dv_xname, r);
			cardbus_conf_write(cc, cf, csc->sc_tag, r + 4, 0);
		}
	}

	(*ct->ct_cf->cardbus_ctrl)(cc, csc->sc_actype);
	(*ct->ct_cf->cardbus_ctrl)(cc, CARDBUS_BM_ENABLE);

	reg = cardbus_conf_read(cc, cf, csc->sc_tag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	    PCI_COMMAND_MASTER_ENABLE;
	cardbus_conf_write(cc, cf, csc->sc_tag, PCI_COMMAND_STATUS_REG, reg);
	reg = cardbus_conf_read(cc, cf, csc->sc_tag, PCI_COMMAND_STATUS_REG);
}
