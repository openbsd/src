#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>
#include <sparc/dev/dmavar.h>

#include <sparc/dev/qecvar.h>
#include <sparc/dev/bereg.h>
#include <sparc/dev/bevar.h>

int	bematch __P((struct device *, void *, void *));
void	beattach __P((struct device *, struct device *, void *));

int	beintr __P((void *arg));
void	bestop __P((struct be_softc *));

struct cfdriver be_cd = {
	NULL, "be", DV_IFNET
};

struct cfattach be_ca = {
	sizeof(struct be_softc), bematch, beattach
};

int
bematch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);
	return (1);
}

void
beattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct be_softc *sc = (struct be_softc *)self;
	struct confargs *ca = aux;
	int pri;
	struct qec_softc *qec = (struct qec_softc *)parent;

	/* XXX the following declarations should be elsewhere */
	extern void myetheraddr __P((u_char *));

	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ca->ca_ra.ra_nintr);
		return;
	}
	pri = ca->ca_ra.ra_intr[0].int_pri;
	printf(" pri %d", pri);
	sc->sc_rev = getpropint(ca->ca_ra.ra_node, "board-version", -1);
	printf(": rev %x", sc->sc_rev);
	myetheraddr(sc->sc_arpcom.ac_enaddr);

	sc->sc_cr = mapiodev(ca->ca_ra.ra_reg, 0, sizeof(struct be_cregs));
	sc->sc_br = mapiodev(&ca->ca_ra.ra_reg[1], 0, sizeof(struct be_bregs));
	sc->sc_tr = mapiodev(&ca->ca_ra.ra_reg[2], 0, sizeof(struct be_tregs));
	bestop(sc);

	sc->sc_mem = qec->sc_buffer;
	sc->sc_memsize = qec->sc_bufsiz;
	sc->sc_conf3 = getpropint(ca->ca_ra.ra_node, "busmaster-regval", 0);

	sc->sc_ih.ih_fun = beintr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(pri, &sc->sc_ih);

	printf("\n");
}

void
bestop(sc)
	struct be_softc *sc;
{
	int tries;

	tries = 32;
	sc->sc_br->tx_cfg = 0;
	while (sc->sc_br->tx_cfg != 0 && --tries)
		DELAY(20);

	tries = 32;
	sc->sc_br->rx_cfg = 0;
	while (sc->sc_br->rx_cfg != 0 && --tries)
		DELAY(20);
}

int
beintr(arg)
	void *arg;
{
	return (0);
}
