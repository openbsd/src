/*	$OpenBSD: if_tht.c,v 1.17 2007/04/17 10:34:54 dlg Exp $ */

/*
 * Copyright (c) 2007 David Gwynne <dlg@openbsd.org>
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
 * Driver for the Tehuti TN30xx multi port 10Gb Ethernet chipsets,
 * see http://www.tehutinetworks.net/.
 *
 * This driver was made possible because Tehuti networks provided
 * hardware and documentation. Thanks!
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

/* registers */

#define THT_PCI_BAR		0x10

#define THT_REG_10G_REV		0x6000
#define THT_REG_10G_SCR		0x6004
#define THT_REG_10G_CTL		0x6008
#define THT_REG_10G_FMT_LNG	0x6014
#define THT_REG_10G_PAUSE	0x6018
#define THT_REG_10G_RX_SEC	0x601c
#define THT_REG_10G_TX_SEC	0x6020
#define THT_REG_10G_RFIFO_AEF	0x6024
#define THT_REG_10G_TFIFO_AEF	0x6028
#define THT_REG_10G_SM_STAT	0x6030
#define THT_REG_10G_SM_CMD	0x6034
#define THT_REG_10G_SM_DAT	0x6038
#define THT_REG_10G_SM_ADD	0x603c
#define THT_REG_10G_STAT	0x6040

#define THT_REG_RG_RX_UNC_MAC0	0x1250
#define THT_REG_RG_RX_UNC_MAC1	0x1260
#define THT_REG_RG_RX_UNC_MAC2	0x1270

#define THT_PORT_SIZE		0x8000
#define THT_PORT_REGION(_p)	((_p) * THT_PORT_SIZE)

/* hardware structures (we're using the 64 bit variants) */

/* physical buffer descriptor */
struct tht_pbd {
	u_int32_t		addr_lo;
	u_int32_t		addr_hi;
	u_int32_t		len;
} __packed;

/* rx free fifo */
struct tht_rxf {
	u_int32_t		flags;

	u_int32_t		uid_lo;
	u_int32_t		uid_hi;

	/* followed by a pdb list */
} __packed;

/* rx descriptor type 2 */
struct tht_rx_desc {
	u_int32_t		flags;
	u_int16_t		len;
	u_int16_t		vlan;

	u_int32_t		uid_lo;
	u_int32_t		uid_hi;
} __packed;

/* rx decriptor type 3: data chain instruction */
struct tht_rx_desc_dc {
	/* preceded by tht_rx_desc */

	u_int16_t		cd_offset;
	u_int16_t		flags;

	u_int8_t		data[4];
} __packed;

/* rx descriptor type 4: rss information */
struct tht_rx_desc_rss {
	/* preceded by tht_rx_desc */

	u_int8_t		rss_hft;
	u_int8_t		rss_type;
	u_int8_t		rss_tcpu;
	u_int8_t		reserved;

	u_int32_t		rss_hash;
} __packed;

/* TX_TASK FIFO */
struct tht_tx_task {
	u_int32_t		flags;
	u_int16_t		mss_mtu;
	u_int16_t		len;

	u_int32_t		uid_lo;
	u_int32_t		uid_hi;

	/* followed by a pbd list */
} __packed;

/* TX_FREE FIFO */
struct tht_tx_free {
	u_int32_t		status;

	u_int32_t		uid_lo;
	u_int32_t		uid_hi;

	u_int32_t		pad;
} __packed;

/* pci controller autoconf glue */

struct thtc_softc {
	struct device		sc_dev;

	bus_dma_tag_t		sc_dmat;

	bus_space_tag_t		sc_memt;
	bus_space_handle_t	sc_memh;
	bus_size_t		sc_mems;
};

int			thtc_match(struct device *, void *, void *);
void			thtc_attach(struct device *, struct device *, void *);
int			thtc_print(void *, const char *);

struct cfattach thtc_ca = {
	sizeof(struct thtc_softc), thtc_match, thtc_attach
};

struct cfdriver thtc_cd = {
	NULL, "thtc", DV_DULL
};

/* port autoconf glue */

struct tht_softc {
	struct device		sc_dev;
	struct thtc_softc	*sc_thtc;

	void			*sc_ih;

	bus_space_handle_t	sc_memh;

	struct arpcom		sc_ac;
	struct ifmedia		sc_media;

	u_int16_t		sc_lladdr[3];
};

int			tht_match(struct device *, void *, void *);
void			tht_attach(struct device *, struct device *, void *);
int			tht_intr(void *);

struct cfattach tht_ca = {
	sizeof(struct tht_softc), tht_match, tht_attach
};

struct cfdriver tht_cd = {
	NULL, "tht", DV_IFNET
};

/* glue between the controller and the port */

struct tht_attach_args {
	int			taa_port;

	struct pci_attach_args	*taa_pa;
	pci_intr_handle_t	taa_ih;
};

/* port operations */
int			tht_ioctl(struct ifnet *, u_long, caddr_t);
void			tht_start(struct ifnet *);
void			tht_watchdog(struct ifnet *);
int			tht_media_change(struct ifnet *);
void			tht_media_status(struct ifnet *, struct ifmediareq *);
void			tht_read_lladdr(struct tht_softc *);

/* bus space operations */
u_int32_t		tht_read(struct tht_softc *, bus_size_t);
void			tht_write(struct tht_softc *, bus_size_t, u_int32_t);

/* misc */
#define DEVNAME(_sc)	((_sc)->sc_dev.dv_xname)
#define sizeofa(_a)	(sizeof(_a) / sizeof((_a)[0]))

struct thtc_device {
	pci_vendor_id_t		td_vendor;
	pci_vendor_id_t		td_product;
	u_int			td_nports;
};

const struct thtc_device *thtc_lookup(struct pci_attach_args *);

static const struct thtc_device thtc_devices[] = {
	{ PCI_VENDOR_TEHUTI,	PCI_PRODUCT_TEHUTI_TN3009, 1 },
	{ PCI_VENDOR_TEHUTI,	PCI_PRODUCT_TEHUTI_TN3010, 1 },
	{ PCI_VENDOR_TEHUTI,	PCI_PRODUCT_TEHUTI_TN3014, 2 }
};

const struct thtc_device *
thtc_lookup(struct pci_attach_args *pa)
{
	int				i;
	const struct thtc_device	*td;

	for (i = 0; i < sizeofa(thtc_devices); i++) {
		td = &thtc_devices[i];
		if (td->td_vendor == PCI_VENDOR(pa->pa_id) &&
		    td->td_product == PCI_PRODUCT(pa->pa_id))
			return (td);
	}

	return (NULL);
}

int
thtc_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args		*pa = aux;

	if (thtc_lookup(pa) != NULL)
		return (1);

	return (0);
}

void
thtc_attach(struct device *parent, struct device *self, void *aux)
{
	struct thtc_softc		*sc = (struct thtc_softc *)self;
	struct pci_attach_args		*pa = aux;
	pcireg_t			memtype;
	const struct thtc_device	*td;
	struct tht_attach_args		taa;
	int				i;

	bzero(&taa, sizeof(taa));
	td = thtc_lookup(pa);

	sc->sc_dmat = pa->pa_dmat;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, THT_PCI_BAR);
	if (pci_mapreg_map(pa, THT_PCI_BAR, memtype, 0, &sc->sc_memt,
	    &sc->sc_memh, NULL, &sc->sc_mems, 0) != 0) {
		printf(": unable to map host registers\n");
		return;
	}

	if (pci_intr_map(pa, &taa.taa_ih) != 0) {
		printf(": unable to map interrupt\n");
		goto unmap;
	}
	printf(": %s\n", pci_intr_string(pa->pa_pc, taa.taa_ih));

	taa.taa_pa = pa;
	for (i = 0; i < td->td_nports; i++) {
		taa.taa_port = i;

		config_found(self, &taa, thtc_print);
	}

	return;

unmap:
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
	sc->sc_mems = 0;
}

int
thtc_print(void *aux, const char *pnp)
{
	struct tht_attach_args		*taa = aux;

	if (pnp != NULL)
		printf("\"%s\" at %s", tht_cd.cd_name, pnp);

	printf(" port %d", taa->taa_port);

	return (UNCONF);
}

int
tht_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
tht_attach(struct device *parent, struct device *self, void *aux)
{
	struct thtc_softc		*csc = (struct thtc_softc *)parent;
	struct tht_softc		*sc = (struct tht_softc *)self;
	struct tht_attach_args		*taa = aux;
	struct ifnet			*ifp;

	sc->sc_thtc = csc;

	if (bus_space_subregion(csc->sc_memt, csc->sc_memh,
	    THT_PORT_REGION(taa->taa_port), THT_PORT_SIZE,
	    &sc->sc_memh) != 0) {
		printf(": unable to map port registers\n");
		return;
	}

	sc->sc_ih = pci_intr_establish(taa->taa_pa->pa_pc, taa->taa_ih,
	    IPL_NET, tht_intr, sc, DEVNAME(sc));
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
		/* bus_space(9) says we dont have to free subregions */
		return;
	}

	tht_read_lladdr(sc);
	bcopy(sc->sc_lladdr, sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN);

	ifp = &sc->sc_ac.ac_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_capabilities = IFCAP_VLAN_MTU;
	ifp->if_ioctl = tht_ioctl;
	ifp->if_start = tht_start;
	ifp->if_watchdog = tht_watchdog;
	ifp->if_hardmtu = 1500; /* XXX */
	strlcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);
	IFQ_SET_MAXLEN(&ifp->if_snd, 400);
	IFQ_SET_READY(&ifp->if_snd);

	ifmedia_init(&sc->sc_media, 0, tht_media_change, tht_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	printf(" address %s\n", ether_sprintf(sc->sc_ac.ac_enaddr));
}

int
tht_intr(void *arg)
{
	return (0);
}

int
tht_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	struct tht_softc		*sc = ifp->if_softc;
	struct ifreq			*ifr = (struct ifreq *)addr;
	int				error;
	int				s;

	s = splnet();

	error = ether_ioctl(ifp, &sc->sc_ac, cmd, addr);
	if (error > 0) {
		splx(s);
		return (error);
	}

	switch (cmd) {
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	default:
		error = ENOTTY;
		break;
	}

	splx(s);

	return (error);
}

void
tht_start(struct ifnet *ifp)
{
	/* do nothing */
}

void
tht_watchdog(struct ifnet *ifp)
{
	/* do nothing */
}

int
tht_media_change(struct ifnet *ifp)
{
	/* ignore */
	return (0);
}

void
tht_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	imr->ifm_active = IFM_ETHER | IFM_AUTO;
	imr->ifm_status = IFM_AVALID;

	/* TODO: check link state */
}

void
tht_read_lladdr(struct tht_softc *sc)
{
	const static bus_size_t		r[3] = {
	    THT_REG_RG_RX_UNC_MAC2,
	    THT_REG_RG_RX_UNC_MAC1,
	    THT_REG_RG_RX_UNC_MAC0
	};
	int				i;

	for (i = 0; i < sizeofa(r); i++)
		sc->sc_lladdr[i] = swap16(tht_read(sc, r[i]));
}

u_int32_t
tht_read(struct tht_softc *sc, bus_size_t r)
{
	bus_space_barrier(sc->sc_thtc->sc_memt, sc->sc_memh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->sc_thtc->sc_memt, sc->sc_memh, r));
}

void
tht_write(struct tht_softc *sc, bus_size_t r, u_int32_t v)
{
	bus_space_write_4(sc->sc_thtc->sc_memt, sc->sc_memh, r, v);
	bus_space_barrier(sc->sc_thtc->sc_memt, sc->sc_memh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}
