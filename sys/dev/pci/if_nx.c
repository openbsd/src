/*	$OpenBSD: if_nx.c,v 1.14 2007/04/27 19:46:47 reyk Exp $	*/

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
 * This driver was made possible because NetXen Inc. provided hardware
 * and documentation. Thanks!
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

#ifdef NX_DEBUG
int nx_debug = 0;
#define DPRINTF(_arg...)	do {					\
	if (nx_debug)							\
		printf(_arg);						\
} while (0)
#define DPRINTREG(_reg)		do {					\
	if (nx_debug)							\
		printf("%s: 0x%08x: %08x\n",				\
		    #_reg, _reg, nxb_read(sc, _reg));			\
} while (0)
#else
#define DPRINTREG(_reg)
#define DPRINTF(arg...)
#endif

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
	u_int			 sc_function;

	bus_dma_tag_t		 sc_dmat;
	bus_space_tag_t		 sc_memt;
	bus_space_handle_t	 sc_memh;
	bus_size_t		 sc_mems;
	bus_space_tag_t		 sc_dbmemt;
	bus_space_handle_t	 sc_dbmemh;
	bus_size_t		 sc_dbmems;

	pci_intr_handle_t	 sc_ih;

	int			 sc_window;

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
	void			*nx_ih;

	struct timeout		 nx_tick;
	u_int8_t		 nx_lladdr[ETHER_ADDR_LEN];
};

int	 nxb_match(struct device *, void *, void *);
void	 nxb_attach(struct device *, struct device *, void *);
int	 nxb_query(struct nxb_softc *sc);

u_int32_t nxb_read(struct nxb_softc *, bus_size_t);
void	 nxb_write(struct nxb_softc *, bus_size_t, u_int32_t);
void	 nxb_set_window(struct nxb_softc *, int);

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
int	 nx_intr(void *);

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
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_IMEZ },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_HMEZ },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_IMEZ_2 },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_HMEZ_2 }
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
	pcireg_t		 memtype;
	const char		*intrstr;
	bus_size_t		 pcisize;
	paddr_t			 pciaddr;
	int			 i;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_function = pa->pa_function;
	sc->sc_window = -1;

	/*
	 * The NetXen NICs can have different PCI memory layouts which
	 * need some special handling in the driver. Support is limited
	 * to 32bit 128MB memory for now (the chipset uses a configurable
	 * window to access the complete memory range).
	 */
	memtype = pci_mapreg_type(sc->sc_pc, sc->sc_tag, NXBAR0);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
		break;
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
	default:
		printf(": invalid memory type: 0x%x\n", memtype);
		return;
	}
	if (pci_mapreg_info(sc->sc_pc, sc->sc_tag, NXBAR0,
	    memtype, &pciaddr, &pcisize, NULL)) {
		printf(": failed to get pci info\n");
		return;
	}
	switch (pcisize) {
	case NXPCIMEM_SIZE_128MB:
		break;
	case NXPCIMEM_SIZE_32MB:
	default:
		printf(": invalid memory size: %ld\n", pcisize);
		return;
	}

	/* Finally map the PCI memory space */
	if (pci_mapreg_map(pa, NXBAR0, memtype, 0, &sc->sc_memt,
	    &sc->sc_memh, NULL, &sc->sc_mems, 0) != 0) {
		printf(": unable to map register memory\n");
		return;
	}
	if (pci_mapreg_map(pa, NXBAR4, memtype, 0, &sc->sc_dbmemt,
	    &sc->sc_dbmemh, NULL, &sc->sc_dbmems, 0) != 0) {
		printf(": unable to map doorbell memory\n");
		goto unmap1;
	}

	/* Map the interrupt, the handlers will be attached later */
	if (pci_intr_map(pa, &sc->sc_ih) != 0) {
		printf(": unable to map interrupt\n");
		goto unmap;
	}
	intrstr = pci_intr_string(pa->pa_pc, sc->sc_ih);
	printf(": %s\n", intrstr);

	if (nxb_query(sc) != 0)
		goto unmap;

	for (i = 0; i < NX_MAX_PORTS; i++)
		config_found(&sc->sc_dev, &sc->sc_nxp[i], nx_print);

	return;

 unmap:
	bus_space_unmap(sc->sc_dbmemt, sc->sc_dbmemh, sc->sc_dbmems);
	sc->sc_dbmems = 0;
 unmap1:
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
	sc->sc_mems = 0;
}

int
nxb_query(struct nxb_softc *sc)
{
	nxb_set_window(sc, 1);
	return (0);
}

u_int32_t
nxb_read(struct nxb_softc *sc, bus_size_t reg)
{
	bus_space_barrier(sc->sc_memt, sc->sc_memh, reg, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->sc_memt, sc->sc_memh, reg));
}

void
nxb_write(struct nxb_softc *sc, bus_size_t reg, u_int32_t val)
{
	bus_space_write_4(sc->sc_memt, sc->sc_memh, reg, val);
	bus_space_barrier(sc->sc_memt, sc->sc_memh, reg, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

void
nxb_set_window(struct nxb_softc *sc, int window)
{
	u_int32_t val;

	if (sc->sc_window == window)
		return;
	assert(window == 0 || window == 1);
	val = nxb_read(sc, NXCRB_WINDOW(sc->sc_function));
	if (window)
		val |= NXCRB_WINDOW_1;
	else
		val &= ~NXCRB_WINDOW_1;
	nxb_write(sc, NXCRB_WINDOW(sc->sc_function), val);
	sc->sc_window = window;
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

	nx->nx_ih = pci_intr_establish(sc->sc_pc, sc->sc_ih, IPL_NET,
	    nx_intr, nx, nx->nx_dev.dv_xname);
	if (nx->nx_ih == NULL) {
		printf(": unable to establish interrupt\n");
		return;
	}

	nx_getlladdr(nx);
	bcopy(nx->nx_lladdr, nx->nx_ac.ac_enaddr, ETHER_ADDR_LEN);
	printf(" address %s\n", ether_sprintf(nx->nx_ac.ac_enaddr));

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

int
nx_intr(void *arg)
{
	return (0);
}
