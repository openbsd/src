/*	$OpenBSD: if_nx.c,v 1.2 2007/04/16 16:28:39 reyk Exp $	*/

/*
 * Copyright (c) 2007 Reyk Floeter <reyk@openbsd.org>
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

/*
 * Driver for the NetXen NX2031/NX2035 10Gb and Gigabit Ethernet chipsets,
 * see http://www.netxen.com/.
 *
 * This driver was made possible because NetXen Inc. donated NX203x
 * hardware and provided documentation. Thanks!
 *
 * (And Puffy Baba spoke the magic words OPEN-SOURCE-AMI...)
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_nxreg.h>

struct nx_softc;

struct nxb_port {
	u_int8_t		 nxp_id;
	u_int8_t		 nxp_mode;
	u_int32_t		 nxp_lladdrid;

	struct nx_softc		*nxp_nx;
};

struct nxb_softc {
	struct device		 sc_dev;

	pci_chipset_tag_t	 sc_pc;
	pcitag_t		 sc_tag;

	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;
	bus_size_t		 sc_ios;
	bus_dma_tag_t		 sc_dmat;

	void			*sc_ih;

	u_int32_t		 sc_nrxbuf;
	u_int32_t		 sc_ntxbuf;
	volatile u_int		 sc_txpending;

	struct nxb_port		 sc_nxp[NX_MAX_PORTS];	/* The nx ports */
};

struct nx_softc {
	struct device		 nx_dev;
	struct arpcom		 nx_ac;
	struct mii_data		 nx_mii;
	struct nxb_softc	*nx_sc;			/* The nxb board */
	struct nxb_port		*nx_port;		/* Port information */
	struct timeout		 nx_tick;
	u_int8_t		 nx_lladdr[ETHER_ADDR_LEN];
};

int	 nxb_match(struct device *, void *, void *);
void	 nxb_attach(struct device *, struct device *, void *);
int	 nxb_query(struct nxb_softc *sc);
int	 nxb_map_pci(struct nxb_softc *, struct pci_attach_args *);
int	 nxb_intr(void *);

int	 nx_match(struct device *, void *, void *);
void	 nx_attach(struct device *, struct device *, void *);
int	 nx_print(void *, const char *);
void	 nx_getlladdr(struct nx_softc *);
int	 nx_media_change(struct ifnet *);
void	 nx_media_status(struct ifnet *, struct ifmediareq *);
void	 nx_link_state(struct nx_softc *);
void	 nx_init(struct ifnet *);
void	 nx_start(struct ifnet *);
void	 nx_stop(struct ifnet *);
void	 nx_watchdog(struct ifnet *);
int	 nx_ioctl(struct ifnet *, u_long, caddr_t);
void	 nx_iff(struct nx_softc *);
void	 nx_tick(void *);

struct cfdriver nxb_cd = {
	0, "nxb", DV_DULL
};
struct cfattach nxb_ca = {
	sizeof(struct nxb_softc), nxb_match, nxb_attach
};

struct cfdriver nx_cd = {
	0, "nx", DV_IFNET
};
struct cfattach nx_ca = {
	sizeof(struct nx_softc), nx_match, nx_attach
};

const struct pci_matchid nxb_devices[] = {
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_10GXxR },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_10GCX4 },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_4GCU },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_10GBCH },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_0005 },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_0024 },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_0025 }
};

extern int ifqmaxlen;

/*
 * Routines handling the physical ''nxb'' board
 */

int
nxb_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux,
	    nxb_devices, sizeof(nxb_devices) / sizeof(nxb_devices[0])));
}

void
nxb_attach(struct device *parent, struct device *self, void *aux)
{
	struct nxb_softc	*sc = (struct nxb_softc *)self;
	struct pci_attach_args	*pa = aux;
	int			 i;

	if (nxb_map_pci(sc, pa) != 0)
		return;
	if (nxb_query(sc) != 0)
		return;
#if 0
	if (nxb_alloc_data(sc) != 0)
		return;
#endif

	for (i = 0; i < NX_MAX_PORTS; i++)
		config_found(&sc->sc_dev, &sc->sc_nxp[i], nx_print);
}

int
nxb_map_pci(struct nxb_softc *sc, struct pci_attach_args *pa)
{
	pcireg_t		 memtype;
	pci_intr_handle_t	 ih;
	const char		*intrstr;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	memtype = pci_mapreg_type(sc->sc_pc, sc->sc_tag, PCI_MAPREG_START);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		break;
	default:
		printf(": invalid memory type\n");
		return (1);
	}
	if (pci_mapreg_map(pa, PCI_MAPREG_START, memtype, 0, &sc->sc_iot,
	    &sc->sc_ioh, NULL, &sc->sc_ios, 0) != 0) {
		printf(": unable to map system interface register\n");
		return (1);
	}

	if (pci_intr_map(pa, &ih) != 0) {
		printf(": unable to map interrupt\n");
		goto unmap;
	}

	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_NET,
	    nxb_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": unable to map interrupt%s%s\n",
		    intrstr == NULL ? "" : " at ",
		    intrstr == NULL ? "" : intrstr);
		goto unmap;
	}
	printf(": %s\n", intrstr);

	return (0);

 unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	sc->sc_ios = 0;
	return (1);
}

int
nxb_query(struct nxb_softc *sc)
{
	return (0);
}

int
nxb_intr(void *arg)
{
	return (0);
}

/*
 * Routines handling the virtual ''nx'' ports
 */

int
nx_match(struct device *parent, void *match, void *aux)
{
	struct nxb_port		*nxp = (struct nxb_port *)aux;

	if (nxp->nxp_id >= NX_MAX_PORTS)
		return (0);

	switch (nxp->nxp_mode) {
	case NXNIU_MODE_XGE:
	case NXNIU_MODE_GBE:
		return (1);
	case NXNIU_MODE_FC:
		/* FibreChannel mode is not documented and not supported */
		return (0);
	}

	return (0);
}

void
nx_attach(struct device *parent, struct device *self, void *aux)
{
	struct nxb_softc	*sc = (struct nxb_softc *)parent;
	struct nx_softc		*nx = (struct nx_softc *)self;
	struct nxb_port		*nxp = (struct nxb_port *)aux;
	struct ifnet		*ifp;

	nx->nx_sc = sc;
	nx->nx_port = nxp;
	nxp->nxp_nx = nx;

	nx_getlladdr(nx);

	ifp = &nx->nx_ac.ac_if;
	ifp->if_softc = nx;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = nx_ioctl;
	ifp->if_start = nx_start;
	ifp->if_watchdog = nx_watchdog;
	ifp->if_hardmtu = NX_JUMBO_MTU;
	strlcpy(ifp->if_xname, nx->nx_dev.dv_xname, IFNAMSIZ);
	IFQ_SET_MAXLEN(&ifp->if_snd, sc->sc_ntxbuf - 1);
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_capabilities = IFCAP_VLAN_MTU;
#if 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
	ifp->if_capabilities |= IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 |
		    IFCAP_CSUM_UDPv4;
#endif

	ifmedia_init(&nx->nx_mii.mii_media, 0,
	    nx_media_change, nx_media_status);
	ifmedia_add(&nx->nx_mii.mii_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&nx->nx_mii.mii_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	timeout_set(&nx->nx_tick, nx_tick, sc);

	return;
}

int
nx_print(void *aux, const char *parentname)
{
	struct nxb_port		*nxp = (struct nxb_port *)aux;

	if (parentname)
		printf("nx port %u at %s",
		   nxp->nxp_id, parentname);
	else
		printf(" port %u", nxp->nxp_id);
	return (UNCONF);
}

void
nx_getlladdr(struct nx_softc *nx)
{
	/* XXX */
	return;
}

int
nx_media_change(struct ifnet *ifp)
{
	struct nx_softc		*nx = (struct nx_softc *)ifp->if_softc;
	struct nxb_port		*nxp = nx->nx_port;

	switch (nxp->nxp_mode) {
	case NXNIU_MODE_XGE:
		/* XXX */
		break;
	case NXNIU_MODE_GBE:
		mii_mediachg(&nx->nx_mii);
		break;
	}

	return (0);
}

void
nx_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct nx_softc		*nx = (struct nx_softc *)ifp->if_softc;
	struct nxb_port		*nxp = nx->nx_port;

	switch (nxp->nxp_mode) {
	case NXNIU_MODE_XGE:
		imr->ifm_active = IFM_ETHER | IFM_AUTO;
		imr->ifm_status = IFM_AVALID;
		nx_link_state(nx);
		if (LINK_STATE_IS_UP(ifp->if_link_state) &&
		    ifp->if_flags & IFF_UP)
			imr->ifm_status |= IFM_ACTIVE;
		break;
	case NXNIU_MODE_GBE:
		mii_pollstat(&nx->nx_mii);
		imr->ifm_active = nx->nx_mii.mii_media_active;
		imr->ifm_status = nx->nx_mii.mii_media_status;
		mii_mediachg(&nx->nx_mii);
		break;
	}
}

void
nx_link_state(struct nx_softc *nx)
{
	struct nxb_port		*nxp = nx->nx_port;
	struct ifnet		*ifp = &nx->nx_ac.ac_if;
	u_int32_t		 status = 0;
	int			 link_state = LINK_STATE_DOWN;

	switch (nxp->nxp_mode) {
	case NXNIU_MODE_XGE:
		/* XXX */
//		status = nx_read(sc, NX_XG_STATE);
		if (status & NXSW_XG_LINK_UP)
			link_state = LINK_STATE_FULL_DUPLEX;
		if (ifp->if_link_state != link_state) {
			ifp->if_link_state = link_state;
			if_link_state_change(ifp);
		}
		break;
	case NXNIU_MODE_GBE:
		mii_tick(&nx->nx_mii);
		break;
	}
}

void
nx_watchdog(struct ifnet *ifp)
{
	return;
}

int
nx_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct nx_softc		*nx = (struct nx_softc *)ifp->if_softc;
	struct ifaddr		*ifa;
	struct ifreq		*ifr;
	int			 s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, &nx->nx_ac, cmd, data)) > 0) {
		splx(s);
		return (error);
	}

	switch (cmd) {
	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		ifp->if_flags |= IFF_UP;
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&nx->nx_ac, ifa);
#endif
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				nx_iff(nx);
			else
				nx_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				nx_stop(ifp);
		}
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > ifp->if_hardmtu)
			error = EINVAL;
		else if (ifp->if_mtu != ifr->ifr_mtu)
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *)data;
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &nx->nx_ac) :
		    ether_delmulti(ifr, &nx->nx_ac);

		if (error == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				nx_iff(nx);
			error = 0;
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &nx->nx_mii.mii_media, cmd);
		break;

	default:
		error = ENOTTY;
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			nx_init(ifp);
		error = 0;
	}

	splx(s);

	return (error);
}

void
nx_init(struct ifnet *ifp)
{
	return;
}

void
nx_start(struct ifnet *ifp)
{
	return;
}

void
nx_stop(struct ifnet *ifp)
{
	return;
}

void
nx_iff(struct nx_softc *nx)
{
	return;
}

void
nx_tick(void *arg)
{
	struct nx_softc		*nx = (struct nx_softc *)arg;

	nx_link_state(nx);

	timeout_add(&nx->nx_tick, hz);
}

