/*	$OpenBSD: if_nep.c,v 1.2 2014/12/22 02:28:52 tedu Exp $	*/
/*
 * Copyright (c) 2014 Mark Kettenis
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
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#ifdef __sparc64__
#include <dev/ofw/openfirm.h>
extern void myetheraddr(u_char *);
#endif

#define FZC_MAC		0x180000

#define MIF_FRAME_OUTPUT	(FZC_MAC + 0x16018)
#define  MIF_FRAME_DATA			0xffff
#define  MIF_FRAME_TA0			(1ULL << 16)
#define  MIF_FRAME_TA1			(1ULL << 17)
#define  MIF_FRAME_REG_SHIFT		18
#define  MIF_FRAME_PHY_SHIFT		23
#define  MIF_FRAME_READ			0x60020000
#define  MIF_FRAME_WRITE		0x50020000
#define MIF_CONFIG		(FZC_MAC + 0x16020)
#define  MIF_CONFIG_INDIRECT_MODE	(1ULL << 15)

struct nep_softc {
	struct device		sc_dev;
	struct arpcom		sc_ac;
	struct mii_data		sc_mii;

	bus_dma_tag_t		sc_dmat;
	bus_space_tag_t		sc_memt;
	bus_space_handle_t 	sc_memh;
	bus_size_t		sc_mems;
	void			*sc_ih;

	int			sc_port;

	struct timeout		sc_tick_ch;
};

int	nep_match(struct device *, void *, void *);
void	nep_attach(struct device *, struct device *, void *);

struct cfattach nep_ca = {
	sizeof(struct nep_softc), nep_match, nep_attach
};

struct cfdriver nep_cd = {
	NULL, "nep", DV_DULL
};

uint64_t nep_read(struct nep_softc *, uint32_t);
void	nep_write(struct nep_softc *, uint32_t, uint64_t);
int	nep_mii_readreg(struct device *, int, int);
void	nep_mii_writereg(struct device *, int, int, int);
void	nep_mii_statchg(struct device *);
int	nep_mediachange(struct ifnet *);
void	nep_mediastatus(struct ifnet *, struct ifmediareq *);
int	nep_init(struct ifnet *ifp);
void	nep_stop(struct ifnet *);
void	nep_iff(struct nep_softc *);
void	nep_tick(void *);
int	nep_ioctl(struct ifnet *, u_long, caddr_t);

int
nep_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SUN &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SUN_NEPTUNE)
		return 1;
	return 0;
}

void
nep_attach(struct device *parent, struct device *self, void *aux)
{
	struct nep_softc *sc = (struct nep_softc *)self;
	struct pci_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mii_data *mii = &sc->sc_mii;
	pcireg_t memtype;
	uint64_t cfg;

	sc->sc_dmat = pa->pa_dmat;

	memtype = PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT;
	if (pci_mapreg_map(pa, PCI_MAPREG_START, memtype, 0,
	    &sc->sc_memt, &sc->sc_memh, NULL, &sc->sc_mems, 0)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_port = pa->pa_function;

#ifdef __sparc64__
	if (OF_getprop(PCITAG_NODE(pa->pa_tag), "local-mac-address",
	    sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN) <= 0)
		myetheraddr(sc->sc_ac.ac_enaddr);
#endif

	printf(", address %s\n", ether_sprintf(sc->sc_ac.ac_enaddr));

	cfg = nep_read(sc, MIF_CONFIG);
	cfg &= ~MIF_CONFIG_INDIRECT_MODE;
	nep_write(sc, MIF_CONFIG, cfg);

	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, sizeof(ifp->if_xname));
	ifp->if_softc = sc;
	ifp->if_ioctl = nep_ioctl;

	mii->mii_ifp = ifp;
	mii->mii_readreg = nep_mii_readreg;
	mii->mii_writereg = nep_mii_writereg;
	mii->mii_statchg = nep_mii_statchg;

	ifmedia_init(&mii->mii_media, 0, nep_mediachange, nep_mediastatus);

	mii_attach(&sc->sc_dev, mii, 0xffffffff, MII_PHY_ANY, sc->sc_port, 0);
	ifmedia_set(&mii->mii_media, IFM_ETHER|IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	timeout_set(&sc->sc_tick_ch, nep_tick, sc);
}

uint64_t
nep_read(struct nep_softc *sc, uint32_t reg)
{
	return bus_space_read_8(sc->sc_memt, sc->sc_memh, reg);
}

void
nep_write(struct nep_softc *sc, uint32_t reg, uint64_t value)
{
	bus_space_write_8(sc->sc_memt, sc->sc_memh, reg, value);
}

int
nep_mii_readreg(struct device *self, int phy, int reg)
{
	struct nep_softc *sc = (struct nep_softc *)self;
	uint64_t frame;
	int n;

	frame = MIF_FRAME_READ;
	frame |= (reg << MIF_FRAME_REG_SHIFT) | (phy << MIF_FRAME_PHY_SHIFT);
	nep_write(sc, MIF_FRAME_OUTPUT, frame);
	for (n = 0; n < 1000; n++) {
		delay(10);
		frame = nep_read(sc, MIF_FRAME_OUTPUT);
		if (frame & MIF_FRAME_TA0)
			return (frame & MIF_FRAME_DATA);
	}

	printf("%s: %s timeout\n", __func__);
	return (0);
}

void
nep_mii_writereg(struct device *self, int phy, int reg, int val)
{
	struct nep_softc *sc = (struct nep_softc *)self;
	uint64_t frame;
	int n;

	frame = MIF_FRAME_WRITE;
	frame |= (reg << MIF_FRAME_REG_SHIFT) | (phy << MIF_FRAME_PHY_SHIFT);
	frame |= (val & MIF_FRAME_DATA);
	nep_write(sc, MIF_FRAME_OUTPUT, frame);
	for (n = 0; n < 1000; n++) {
		delay(10);
		frame = nep_read(sc, MIF_FRAME_OUTPUT);
		if (frame & MIF_FRAME_TA0)
			return;
	}

	printf("%s: %s timeout\n", __func__);
	return;
}

void
nep_mii_statchg(struct device *dev)
{
	printf("%s\n", __func__);
}

int
nep_mediachange(struct ifnet *ifp)
{
	printf("%s\n", __func__);
	return 0;
}

void
nep_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct nep_softc *sc = (struct nep_softc *)ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
}

int
nep_init(struct ifnet *ifp)
{
	struct nep_softc *sc = (struct nep_softc *)ifp->if_softc;
	int s;

	s = splnet();

	timeout_add_sec(&sc->sc_tick_ch, 1);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;

	splx(s);

	return 0;
}

void
nep_stop(struct ifnet *ifp)
{
	struct nep_softc *sc = (struct nep_softc *)ifp->if_softc;

	timeout_del(&sc->sc_tick_ch);
}

void
nep_iff(struct nep_softc *sc)
{
	printf("%s\n", __func__);
}

void
nep_tick(void *arg)
{
	struct nep_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	timeout_add_sec(&sc->sc_tick_ch, 1);
}

int
nep_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct nep_softc *sc = (struct nep_softc *)ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
		/* FALLTHROUGH */

	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = ENETRESET;
			else
				nep_init(ifp);
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				nep_stop(ifp);
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			nep_iff(sc);
		error = 0;
	}

	splx(s);
	return (error);
}
