/*	$OpenBSD: if_rln_pcmcia.c,v 1.9 2000/02/05 13:55:45 d Exp $	*/
/*
 * David Leonard <d@openbsd.org>, 1999. Public domain.
 *
 * Proxim RangeLAN2 PC-Card and compatibles
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/rln.h>
#include <dev/ic/rlnvar.h>
#include <dev/ic/rlnreg.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

struct rln_pcmcia_softc {
	struct rln_softc psc_rln;		/* real "rln" softc */

	struct pcmcia_io_handle psc_pcioh;	/* PCMCIA i/o information */
	int sc_io_window;			/* i/o window for the card */
	struct pcmcia_function *psc_pf;		/* our PCMCIA function */
	void *psc_ih;				/* our interrupt handle */
};

static int	rln_pcmcia_match __P((struct device *, void *, void *));
static struct	rln_pcmcia_product * rln_pcmcia_product_lookup
     __P((struct pcmcia_attach_args *));
static void	rln_pcmcia_attach __P((struct device *, struct device *, void *));
static int	rln_pcmcia_detach __P((struct device *, int));
static int	rln_pcmcia_activate __P((struct device *, enum devact));
static int	rlnintr_pcmcia __P((void *arg));

struct cfattach rln_pcmcia_ca = {
	sizeof(struct rln_pcmcia_softc), rln_pcmcia_match, rln_pcmcia_attach,
	rln_pcmcia_detach, rln_pcmcia_activate
};

static struct rln_pcmcia_product {
	u_int16_t	manufacturer;
	u_int16_t	product;
	const char	*cis[4];
	u_int8_t	flags;
} rln_pcmcia_products[] = {
	/* Digital RoamAbout 2400 FH, from d@openbsd.org */
	{ PCMCIA_VENDOR_PROXIM,
	  PCMCIA_PRODUCT_PROXIM_ROAMABOUT_2400FH,
	  PCMCIA_CIS_PROXIM_ROAMABOUT_2400FH,
	  0 },
	/* AMP Wireless, from jimduchek@ou.edu */
	{ PCMCIA_VENDOR_COMPEX,
	  PCMCIA_PRODUCT_COMPEX_AMP_WIRELESS,
	  PCMCIA_CIS_COMPEX_AMP_WIRELESS,
	  0 },
	/* Proxim RangeLAN2 7401, from louis@bertrandtech.on.ca */
	{ PCMCIA_VENDOR_PROXIM,
	  PCMCIA_PRODUCT_PROXIM_RANGELAN2_7401,
	  PCMCIA_CIS_PROXIM_RANGELAN2_7401,
	  0 },
	/* Generic and clone cards matched by CIS alone */
	{ PCMCIA_VENDOR_INVALID,
	  PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_PROXIM_RL2_7200,
	  0 },
	{ PCMCIA_VENDOR_INVALID,
	  PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_PROXIM_RL2_7400,
	  0 },
	{ PCMCIA_VENDOR_INVALID,
	  PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_PROXIM_SYMPHONY,
	  0 }
};
#define NPRODUCTS (sizeof rln_pcmcia_products / sizeof rln_pcmcia_products[0])

/* Match the card information with known card types */
static struct rln_pcmcia_product *
rln_pcmcia_product_lookup(pa)
	struct pcmcia_attach_args *pa;
{
	int i, j;
	struct rln_pcmcia_product *rpp;

	for (i = 0; i < NPRODUCTS; i++) {
		rpp = &rln_pcmcia_products[i];
		if (rpp->manufacturer != PCMCIA_VENDOR_INVALID &&
		    rpp->manufacturer != pa->manufacturer)
			continue;
		if (rpp->product != PCMCIA_PRODUCT_INVALID &&
		    rpp->product != pa->product)
			continue;
		for (j = 0; j < 4; j++) {
			if (rpp->cis[j] == NULL)
				return rpp;
			if (strcmp(pa->card->cis1_info[j], rpp->cis[j]) != 0)
				break;
		}
		if (j == 4)
			return rpp;
	}
	return NULL;
}

/* Do we know this card? */
static int
rln_pcmcia_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pcmcia_attach_args *pa = aux;

	return (rln_pcmcia_product_lookup(pa) != NULL);
}

/* Attach and configure */
void
rln_pcmcia_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct rln_pcmcia_softc *psc = (void *) self;
	struct rln_softc *sc = &psc->psc_rln;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	struct rln_pcmcia_product *rpp;

	psc->psc_pf = pa->pf;
	cfe = psc->psc_pf->cfe_head.sqh_first;

	/* Guess the transfer width we will be using */
	if (cfe->flags & PCMCIA_CFE_IO16)
		sc->sc_width = 16;
	else if (cfe->flags & PCMCIA_CFE_IO8)
		sc->sc_width = 8;
	else
		sc->sc_width = 0;

#ifdef DIAGNOSTIC
	/* We only expect one i/o region and no memory region */
	if (cfe->num_memspace != 0)
		printf(": unexpected number of memory spaces (%d)\n",
		    cfe->num_memspace);
	if (cfe->num_iospace != 1)
		printf(": unexpected number of i/o spaces (%d)\n",
		    cfe->num_iospace);
	else if (cfe->iospace[0].length != RLN_NPORTS)
		printf(": unexpected size of i/o space (0x%x)\n",
		    cfe->iospace[0].length);
	if (sc->sc_width == 0)
		printf(": unknown bus width\n");
#endif /* DIAGNOSTIC */

	pcmcia_function_init(psc->psc_pf, cfe);

	/* Allocate i/o space */
	if (pcmcia_io_alloc(psc->psc_pf, 0, RLN_NPORTS,
	    RLN_NPORTS, &psc->psc_pcioh)) {
		printf(": can't allocate i/o space\n");
		return;
	}

	sc->sc_iot = psc->psc_pcioh.iot;
	sc->sc_ioh = psc->psc_pcioh.ioh;

	/* Map i/o space */
	if (pcmcia_io_map(psc->psc_pf, ((sc->sc_width == 8) ? PCMCIA_WIDTH_IO8 :
	    (sc->sc_width == 16) ? PCMCIA_WIDTH_IO16 : PCMCIA_WIDTH_AUTO),
	    0, RLN_NPORTS, &psc->psc_pcioh, &psc->sc_io_window)) {
		printf(": can't map i/o space\n");
		return;
	}
	printf(" port 0x%lx/%d", psc->psc_pcioh.addr, RLN_NPORTS);

	/* Enable the card */
	if (pcmcia_function_enable(psc->psc_pf)) {
		printf(": function enable failed\n");
		return;
	}

#ifdef notyet
	sc->enable = rln_pcmcia_enable;
	sc->disable = rln_pcmcia_disable;
#endif

	rpp = rln_pcmcia_product_lookup(pa);

	/* Check if the device has a separate antenna module */
	sc->sc_cardtype = 0;
	switch (psc->psc_pf->ccr_base) {
	case 0x0100:
		sc->sc_cardtype |= RLN_CTYPE_ONE_PIECE;
		break;
	case 0x0800:
		sc->sc_cardtype &= ~RLN_CTYPE_ONE_PIECE;
		break;
#ifdef DIAGNOSTIC
	default:
		printf(": cannot tell if one or two piece (ccr addr %x)\n",
			sc->sc_dev.dv_xname, psc->psc_pf->ccr_base);
#endif
	}

	/* The PC-card needs to be told to use 'irq' 15 */
	sc->sc_irq = 15;

	/*
	 * We need to get an interrupt before configuring, since
	 * polling registers (the alternative) to reading card
	 * responses, causes hard lock-ups.
	 */
	psc->psc_ih = pcmcia_intr_establish(psc->psc_pf, IPL_NET,
		rlnintr_pcmcia, sc);
	if (psc->psc_ih == NULL)
		printf(": couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
	sc->sc_ih = NULL;

#ifdef DIAGNOSTIC
	if (rpp->manufacturer == PCMCIA_VENDOR_INVALID)
		printf(" manf %04x prod %04x", pa->manufacturer, pa->product);
#endif

	rln_reset(sc);
	rlnconfig(sc);
	printf("\n");
}

static int
rln_pcmcia_detach(dev, flags)
	struct device *dev;
	int flags;
{
	struct rln_pcmcia_softc *psc = (struct rln_pcmcia_softc *)dev;
	struct rln_softc *sc = (struct rln_softc *)dev;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int rv = 0;

	pcmcia_io_unmap(psc->psc_pf, psc->sc_io_window);
	pcmcia_io_free(psc->psc_pf, &psc->psc_pcioh);

	ether_ifdetach(ifp);
	if_detach(ifp);

	return (rv);
}

static int
rln_pcmcia_activate(dev, act)
	struct device *dev;
	enum devact act;
{
	struct rln_pcmcia_softc *psc = (struct rln_pcmcia_softc *)dev;
	struct rln_softc *sc = (struct rln_softc *)dev;
        struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int s;

	s = splnet();
	switch (act) {
	case DVACT_ACTIVATE:
		pcmcia_function_enable(psc->psc_pf);
		printf("%s:", sc->sc_dev.dv_xname);
		psc->psc_ih =
		    pcmcia_intr_establish(psc->psc_pf, IPL_NET, rlnintr_pcmcia,
		        psc);
		printf("\n");
		rlninit(sc);
		break;

	case DVACT_DEACTIVATE:
		ifp->if_timer = 0;
		if (ifp->if_flags & IFF_RUNNING)
			rlnstop(sc);
		pcmcia_function_disable(psc->psc_pf);
		pcmcia_intr_disestablish(psc->psc_pf, psc->psc_ih);
		break;
	}
	splx(s);
	return (0);
}

/* Interrupt handler */
static int
rlnintr_pcmcia(arg)
	void *arg;
{
	struct rln_softc *sc = (struct rln_softc *)arg;
	struct rln_pcmcia_softc *psc = (struct rln_pcmcia_softc *)sc;
	int opt;
	int ret;

	/* Need to immediately read/write the option register for PC-card */
	opt = pcmcia_ccr_read(psc->psc_pf, PCMCIA_CCR_OPTION);
	pcmcia_ccr_write(psc->psc_pf, PCMCIA_CCR_OPTION, opt);

	/* Call actual interrupt handler */
	ret = rlnintr(sc);

	return (ret);
}
