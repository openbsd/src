/*	$OpenBSD: if_dc_cardbus.c,v 1.2 2000/10/26 22:37:04 aaron Exp $	*/

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

struct	dc_cardbus_softc {
	struct dc_softc sc_dc;
	int	sc_intrline;

	cardbus_devfunc_t sc_ct;
	cardbustag_t sc_tag;
	bus_size_t sc_mapsize;
	int sc_actype;
};

int dc_cardbus_match __P((struct device *, void *, void *));
void dc_cardbus_attach __P((struct device *, struct device *,void *));
int dc_cardbus_detach __P((struct device *, int));
int dc_cardbus_activate __P((struct device *, enum devact));
void dc_cardbus_setup __P((struct dc_cardbus_softc *csc));

struct cfattach dc_cardbus_ca = {
	sizeof(struct dc_cardbus_softc), dc_cardbus_match,
		dc_cardbus_attach, dc_cardbus_detach,
		dc_cardbus_activate
};

struct dc_type dc_cardbus_devs[] = {
	{ PCI_VENDOR_DEC, PCI_PRODUCT_DEC_21142 },
	{ PCI_VENDOR_XIRCOM, PCI_PRODUCT_XIRCOM_X3201_3_21143 },
	{ PCI_VENDOR_ADMTEK, PCI_PRODUCT_ADMTEK_AN985 },
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

	sc->dc_unit = sc->sc_dev.dv_unit;
	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;

	Cardbus_function_enable(ct);

	switch (PCI_VENDOR(ca->ca_id)) {
	case PCI_VENDOR_DEC:
		if (PCI_PRODUCT(ca->ca_id) == PCI_PRODUCT_DEC_21142) {
			sc->dc_type = DC_TYPE_21143;
			sc->dc_flags |= DC_TX_POLL|DC_TX_USE_TX_INTR;
			sc->dc_flags |= DC_REDUCED_MII_POLL;

			sc->dc_pmode = DC_PMODE_MII;
		}
		break;
	case PCI_VENDOR_XIRCOM:
		if (PCI_PRODUCT(ca->ca_id) ==
		    PCI_PRODUCT_XIRCOM_X3201_3_21143) {
			sc->dc_type = DC_TYPE_XIRCOM;
			sc->dc_flags |= DC_TX_INTR_ALWAYS|DC_TX_COALESCE;
			sc->dc_pmode = DC_PMODE_MII;

			bcopy(ca->ca_cis.funce.network.netid,
			    &sc->arpcom.ac_enaddr,
			    sizeof sc->arpcom.ac_enaddr);
		}
		break;
	case PCI_VENDOR_ADMTEK:
		if (PCI_PRODUCT(ca->ca_id) == PCI_PRODUCT_ADMTEK_AN985) {
			sc->dc_type = DC_TYPE_AN983;
			sc->dc_flags |= DC_TX_USE_TX_INTR|DC_TX_ADMTEK_WAR;
			sc->dc_pmode = DC_PMODE_MII;
		}
		break;
	default:
		printf(": unknown device\n");
		return;
	}

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

	sc->dc_cachesize = pci_conf_read(cc, ca->ca_tag, DC_PCI_CFLT) & 0xFF;

	dc_cardbus_setup(csc);
	cardbus_save_bar(ct);

 	/*
	 * set latency timer, do we really need this?
	 */
	reg = cardbus_conf_read(cc, cf, ca->ca_tag, CARDBUS_BHLC_REG);
	if (CARDBUS_LATTIMER(reg) < 0x20) {
		reg &= ~(CARDBUS_LATTIMER_MASK << CARDBUS_LATTIMER_SHIFT);
		reg |= (0x20 << CARDBUS_LATTIMER_SHIFT);
		cardbus_conf_write(cc, cf, ca->ca_tag, CARDBUS_BHLC_REG, reg);
	}

	sc->sc_ih = cardbus_intr_establish(cc, cf,
	    ca->ca_intrline, IPL_NET, dc_intr, csc);
	if (sc->sc_ih == NULL) {
		printf(": can\'t establish interrupt at %d\n",
		    ca->ca_intrline);
		return;
	} else
		printf(" irq %d", ca->ca_intrline);

	dc_reset(sc);

	sc->dc_revision = CARDBUS_REVISION(ca->ca_class);
	dc_attach_common(sc);
}

int
dc_cardbus_detach(self, flags)
	struct device *self;
	int flags;
{
	struct dc_cardbus_softc *csc = (struct dc_cardbus_softc *)self;
	struct dc_softc *sc = &csc->sc_dc;
	struct cardbus_devfunc *ct = csc->sc_ct;
	struct ifnet *ifp = &sc->arpcom.ac_if;
#if 0
	struct mii_softc *msc;
#endif
	int rv = 0;

#if 0
	for (msc = LIST_FIRST(&sc->sc_mii.mii_phys); msc;
	    msc = LIST_FIRST(&sc->sc_mii.mii_phys))
		rv |= mii_detach(msc, flags);
#endif

	/* unmap cardbus resources */
	Cardbus_mapreg_unmap(ct,
	    csc->sc_actype == CARDBUS_IO_ENABLE ? PCI_CBIO : PCI_CBMEM,
	    sc->dc_btag, sc->dc_bhandle, csc->sc_mapsize);

	ether_ifdetach(ifp);
	if_detach(ifp);

	return (rv);
}

int
dc_cardbus_activate(dev, act)
	struct device *dev;
	enum devact act;
{
	struct dc_cardbus_softc *csc = (struct dc_cardbus_softc *)dev;
	struct dc_softc *sc = &csc->sc_dc;
	cardbus_devfunc_t ct = csc->sc_ct;
	int s;

	s = splnet();
	switch (act) {
	case DVACT_ACTIVATE:
		Cardbus_function_enable(ct);
		cardbus_restore_bar(ct);
		dc_cardbus_setup(csc);
		sc->sc_ih = cardbus_intr_establish(ct->ct_cc, ct->ct_cf,
		    csc->sc_intrline, IPL_NET, dc_intr, csc);
		if (sc->sc_ih == NULL) {
			printf(": can\'t establish interrupt at %d\n",
			    csc->sc_intrline);
			Cardbus_function_disable(ct);
			return -1;
		} else
			printf("%s: interrupting at %d",
			    sc->sc_dev.dv_xname, csc->sc_intrline);
		break;

	case DVACT_DEACTIVATE:
		cardbus_save_bar(ct);
		cardbus_intr_disestablish(ct->ct_cc, ct->ct_cf, sc->sc_ih);
		Cardbus_function_disable(ct);
		break;
	}

	splx(s);
	return 0;
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
#if 0
printf("wakeup %x\n", cardbus_conf_read(cc, cf, csc->sc_tag, PCI_CFDA));
#endif
	}

	if (cardbus_get_capability(cc, cf, csc->sc_tag, PCI_CAP_PWRMGMT, &r, 0)) {
		r = cardbus_conf_read(cc, cf, csc->sc_tag, r + 4) & 3;
		if (r) {
			printf("%s: awakening from state D%d\n",
			    csc->sc_dc.sc_dev.dv_xname, r);
			cardbus_conf_write(cc, cf, csc->sc_tag, r + 4, 0);
		}
	}

	(*ct->ct_cf->cardbus_ctrl)(cc, csc->sc_actype);
	(*ct->ct_cf->cardbus_ctrl)(cc, CARDBUS_BM_ENABLE);

	reg = cardbus_conf_read(cc, cf, csc->sc_tag, CARDBUS_COMMAND_STATUS_REG);
	reg |= CARDBUS_COMMAND_IO_ENABLE | CARDBUS_COMMAND_MEM_ENABLE |
	    CARDBUS_COMMAND_MASTER_ENABLE;
	cardbus_conf_write(cc, cf, csc->sc_tag, CARDBUS_COMMAND_STATUS_REG, reg);
	reg = cardbus_conf_read(cc, cf, csc->sc_tag, CARDBUS_COMMAND_STATUS_REG);
}
