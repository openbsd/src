/*	$OpenBSD: if_rl2_pcmcia.c,v 1.1 1999/06/21 23:21:45 d Exp $	*/
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

#include <dev/ic/rl2.h>
#include <dev/ic/rl2var.h>
#include <dev/ic/rl2reg.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

struct rl2_pcmcia_softc {
	struct rl2_softc sc_rl2;		/* real "rl2" softc */

	struct pcmcia_io_handle sc_pcioh;	/* PCMCIA i/o information */
	int sc_io_window;			/* i/o window for the card */
	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	void *sc_ih;				/* our interrupt handle */
};

static int  rl2_pcmcia_match __P((struct device *, void *, void *));
static struct rl2_pcmcia_product * rl2_pcmcia_product_lookup __P((
		struct pcmcia_attach_args *));
static void rl2_pcmcia_attach __P((struct device *, struct device *, void *));
static int rl2intr_pcmcia __P((void *arg));

#ifdef notyet
static int  rl2_pcmcia_enable __P((struct rl2_softc *));
static void rl2_pcmcia_disable __P((struct rl2_softc *));
#endif

struct cfattach rln_pcmcia_ca = {
	sizeof(struct rl2_pcmcia_softc), rl2_pcmcia_match, rl2_pcmcia_attach
};

/* Apparently this is the "7200 PC card"? */
#define PCMCIA_CIS_RANGELAN2  { "PROXIM", "LAN CARD", "RANGELAN2", NULL }

static struct rl2_pcmcia_product {
	u_int32_t	manufacturer;
	u_int32_t	product;
	const char	*name;
	u_int8_t	flags;
} rl2_pcmcia_products[] = {
	{ 0x0126,				/* Digital */
	  0x1058,				/* RoamAbout 2400 FH */
	  "Digital RoamAbout 2400 FH",
	  0 },
	{ 0x8a01,				/* AMP */
	  0x0066,				/* Wireless */
	  "AMP Wireless",
	  0 },
	{ 0,
	  0,
	  "unknown RangeLAN2 wireless network card",
	  0 },
};

/* Match the product and manufacturer codes with known card types */
static struct rl2_pcmcia_product *
rl2_pcmcia_product_lookup(pa)
	struct pcmcia_attach_args *pa;
{
	struct rl2_pcmcia_product *rpp;

	for (rpp = rl2_pcmcia_products; rpp->manufacturer && rpp->product; 
	    rpp++)
		if (pa->manufacturer == rpp->manufacturer &&
		    pa->product == rpp->product)
			break;
	return (rpp);
}

/* Match card CIS info string with RangeLAN2 cards */
static int
rl2_pcmcia_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pcmcia_attach_args *pa = aux;
	const char *cis1_info[4] = PCMCIA_CIS_RANGELAN2;
	int i;

	for (i = 0; i < 4; i++)
		if (cis1_info[i] && 
		    strcmp(cis1_info[i], pa->card->cis1_info[i]) != 0)
			return (0);

	return (1);
}

/* Attach and configure */
static void
rl2_pcmcia_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct rl2_pcmcia_softc *psc = (void *) self;
	struct rl2_softc *sc = &psc->sc_rl2;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	struct rl2_pcmcia_product *rpp;

#ifdef RL2DEBUG
	/* Allowed i/o base addresses from the RoamAbout owner's manual */
	int i;
	static bus_addr_t iobases[] = {
		0x270, 		/* useful in user-space debugging */
		0x100, 0x120, 0x140, 0x218, 0x270, 0x280, 0x290, 0x298, 
		0x2a0, 0x2a8, 0x2e0, 0x300, 0x310, 0x358, 0x360, 0x368,
		0
	};
#endif

	psc->sc_pf = pa->pf;
	cfe = psc->sc_pf->cfe_head.sqh_first;

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
	else if (cfe->iospace[0].length != RL2_NPORTS)
		printf(": unexpected size of i/o space (0x%x)\n",
		    cfe->iospace[0].length);
	if (sc->sc_width == 0)
		printf(": unknown bus width\n");
#endif /* DIAGNOSTIC */

	pcmcia_function_init(psc->sc_pf, cfe);

	/* Allocate i/o space */
#ifdef RL2DEBUG
	/* Try only those ports from the manual */
	for (i=0; iobases[i] != 0; i++)
		if (pcmcia_io_alloc(psc->sc_pf, iobases[i], RL2_NPORTS, 
		    RL2_NPORTS, &psc->sc_pcioh) == 0)
			break;
	if (iobases[i] == 0) {
#else
	if (pcmcia_io_alloc(psc->sc_pf, 0, RL2_NPORTS, 
	    RL2_NPORTS, &psc->sc_pcioh)) {
#endif
		printf(": can't alloc i/o space\n");
		return;
	}

	sc->sc_iot = psc->sc_pcioh.iot;
	sc->sc_ioh = psc->sc_pcioh.ioh;

	/* Map i/o space */
	if (pcmcia_io_map(psc->sc_pf, ((sc->sc_width == 8) ? PCMCIA_WIDTH_IO8 :
	    (sc->sc_width == 16) ? PCMCIA_WIDTH_IO16 : PCMCIA_WIDTH_AUTO),
	    0, RL2_NPORTS, &psc->sc_pcioh, &psc->sc_io_window)) {
		printf(": can't map i/o space\n");
		return;
	}

	/* Enable the card */
	if (pcmcia_function_enable(psc->sc_pf)) {
		printf(": function enable failed\n");
		return;
	}

#ifdef notyet
	sc->enable = rl2_pcmcia_enable;
	sc->disable = rl2_pcmcia_disable;
#endif

	rpp = rl2_pcmcia_product_lookup(pa);

	/* Check if the device has a separate antenna module */
	sc->sc_cardtype = 0;
	switch (psc->sc_pf->ccr_base) {
	case 0x0100:
		sc->sc_cardtype |= RL2_CTYPE_ONE_PIECE;
		break;
	case 0x0800:
		sc->sc_cardtype &= ~RL2_CTYPE_ONE_PIECE;
		break;
#ifdef DIAGNOSTIC
	default:
		printf("\n%s: cannot tell if one or two piece (ccr addr %x)\n",
			sc->sc_dev.dv_xname, psc->sc_pf->ccr_base);
#endif
	}

	/* The PC-card needs to be told to use 'irq' 15 */
	sc->sc_irq = 15;

	/*
	 * We need to get an interrupt before configuring, since
	 * polling registers (the alternative) to reading card
	 * responses, causes hard lock-ups.
	 */
	printf("\n");
	sc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET,
		rl2intr_pcmcia, sc);
	if (sc->sc_ih == NULL)
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);

	printf("%s: %s", sc->sc_dev.dv_xname, rpp->name);
	rl2config(sc);
	printf("\n");
}

/* Interrupt handler */
static int
rl2intr_pcmcia(arg)
	void *arg;
{
	struct rl2_softc *sc = (struct rl2_softc *)arg;
	struct rl2_pcmcia_softc *psc = (struct rl2_pcmcia_softc *)sc;
	int opt;
	int ret;

	/* Need to immediately read/write the option register for PC-card */
	opt = pcmcia_ccr_read(psc->sc_pf, PCMCIA_CCR_OPTION);
	pcmcia_ccr_write(psc->sc_pf, PCMCIA_CCR_OPTION, opt);

	/* Call actual interrupt handler */
	ret = rl2intr(arg);

	return (ret);
}

#ifdef notyet
static int
rl2_pcmcia_enable(sc)
	struct rl2_softc *sc;
{
	struct rl2_pcmcia_softc *psc = (struct rl2_pcmcia_softc *) sc;
	struct pcmcia_function *pf = psc->sc_pf;

	/* Establish the interrupt */
	sc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET, 
		rl2intr_pcmcia, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}

	return (pcmcia_function_enable(pf));
}

static void
rl2_pcmcia_disable(sc)
	struct rl2_softc *sc;
{
	struct rl2_pcmcia_softc *psc = (struct rl2_pcmcia_softc *) sc;
	
	pcmcia_function_disable(psc->sc_pf);
	pcmcia_intr_disestablish(psc->sc_pf, sc->sc_ih);
}
#endif
