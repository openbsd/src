/*	$OpenBSD: nhi.c,v 1.4 2026/01/19 21:12:49 kettenis Exp $ */

/*
 * Copyright (c) 2025 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/pool.h>
#include <sys/reboot.h>

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <lib/libkern/crc32c.h>

#define NHI_BAR		PCI_MAPREG_START

#define NHI_TX_RING_BASE_LO(_n)		(0x00000 + (_n) * 0x10)
#define NHI_TX_RING_BASE_HI(_n)		(0x00004 + (_n) * 0x10)
#define NHI_TX_RING_PROD_CONS(_n)	(0x00008 + (_n) * 0x10)
#define NHI_TX_RING_PROD_SHIFT		16
#define NHI_TX_RING_SIZE(_n)		(0x0000c + (_n) * 0x10)
#define NHI_RX_RING_BASE_LO(_n)		(0x08000 + (_n) * 0x10)
#define NHI_RX_RING_BASE_HI(_n)		(0x08004 + (_n) * 0x10)
#define NHI_RX_RING_PROD_CONS(_n)	(0x08008 + (_n) * 0x10)
#define NHI_RX_RING_CONS_SHIFT		0
#define NHI_RX_RING_PROD_SHIFT		16
#define NHI_RX_RING_SIZE(_n)		(0x0800c + (_n) * 0x10)
#define NHI_TX_RING_CTRL(_n)		(0x19800 + (_n) * 0x20)
#define NHI_RX_RING_CTRL(_n)		(0x29800 + (_n) * 0x20)
#define  NHI_RING_NS			(1U << 29)
#define  NHI_RING_RAW			(1U << 30)
#define  NHI_RING_VALID			(1U << 31)
#define NHI_ISR(_n)			(0x37800 + (_n) * 0x04)
#define NHI_ISC(_n)			(0x37808 + (_n) * 0x04)
#define NHI_ISS(_n)			(0x37810 + (_n) * 0x04)
#define NHI_IMR(_n)			(0x38200 + (_n) * 0x04)
#define NHI_IMC(_n)			(0x38208 + (_n) * 0x04)
#define NHI_IMS(_n)			(0x37810 + (_n) * 0x04)
#define NHI_ITR(_n)			(0x37c00 + (_n) * 0x04)
#define NHI_IVAR(_n)			(0x37c40 + (_n) * 0x04)
#define NHI_CAPS			0x39640
#define  NHI_CAPS_VER_MAJ(_x)		(((_x) >> 21) & 0x7)
#define  NHI_CAPS_VER_MIN(_x)		(((_x) >> 16) & 0x1f)
#define NHI_RESET			0x39858
#define  NHI_RESET_RST			(1U << 0)

/* Protocol Defined Field */
#define PDF_READ		0x1
#define PDF_WRITE		0x2
#define PDF_NOTIF		0x3
#define PDF_HOTPLUG		0x5

/* Adapter Configuration Space */
#define ADP_CS			0x1
#define  ADP_CS_0		0
#define  ADP_CS_1		1
#define  ADP_CS_2		2

/* Router Configuration Space */
#define ROUTER_CS		0x2
#define  ROUTER_CS_0		0
#define  ROUTER_CS_1		1
#define  ROUTER_CS_2		2
#define  ROUTER_CS_3		3
#define  ROUTER_CS_4		4
#define  ROUTER_CS_5		5
#define   ROUTER_CS_5_SLP	(1U << 0)
#define  ROUTER_CS_6		6
#define   ROUTER_CS_6_SLPR	(1U << 0)

/* Notification Events */
#define HP_ACK			7

struct nhi_dmamem {
	bus_dmamap_t		ndm_map;
	bus_dma_segment_t	ndm_seg;
	size_t			ndm_size;
	caddr_t			ndm_kva;
};

#define NHI_DMA_MAP(_ndm)	((_ndm)->ndm_map)
#define NHI_DMA_LEN(_ndm)	((_ndm)->ndm_size)
#define NHI_DMA_DVA(_ndm)	((_ndm)->ndm_map->dm_segs[0].ds_addr)
#define NHI_DMA_KVA(_ndm)	((void *)(_ndm)->ndm_kva)

struct nhi_desc {
	uint32_t	nd_addr_lo;
	uint32_t	nd_addr_hi;
	uint32_t	nd_flags;
	uint32_t	nd_reserved;
};

#define NHI_DESC_LEN_MASK	(0xffffU << 0)
#define NHI_DESC_LEN_SHIFT	0
#define NHI_DESC_EOF_PDF_MASK	(0xfU << 12)
#define NHI_DESC_EOF_PDF_SHIFT	12
#define NHI_DESC_SOF_PDF_MASK	(0xfU << 16)
#define NHI_DESC_SOF_PDF_SHIFT	16
#define NHI_DESC_CRC		(1U << 20)
#define NHI_DESC_DD		(1U << 21)
#define NHI_DESC_RS		(1U << 22)
#define NHI_DESC_BO		(1U << 22)
#define NHI_DESC_IE		(1U << 23)
#define NHI_DESC_OFF_MASK	(0xff << 24)
#define NHI_DESC_OFF_SHIFT	24

#define NHI_TX_NDESC		64
#define NHI_TX_SIZE		(256 * sizeof(uint32_t))
#define NHI_RX_NDESC		64

struct nhi_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	bus_dma_tag_t		sc_dmat;
	void			*sc_ih;

	struct nhi_dmamem	*sc_tx_ring;
	struct nhi_desc		*sc_tx_desc;
	bus_dmamap_t		sc_tx_map[NHI_TX_NDESC];
	uint32_t		*sc_tx_buf[NHI_TX_NDESC];
	u_int			sc_tx_prod;
	struct nhi_dmamem	*sc_rx_ring;
	struct nhi_desc		*sc_rx_desc;
	struct nhi_dmamem	*sc_rx_buf[NHI_RX_NDESC];
	u_int			sc_rx_prod;

	uint32_t		sc_pdf;
	uint32_t		sc_data;
};

int	nhi_conf_read(struct nhi_softc *, uint32_t, uint32_t, uint32_t,
	    uint32_t, uint32_t *);
int	nhi_conf_write(struct nhi_softc *, uint32_t, uint32_t, uint32_t,
	    uint32_t, uint32_t);

void	nhi_init(struct nhi_softc *);
int	nhi_intr(void *);

struct nhi_dmamem *nhi_dmamem_alloc(bus_dma_tag_t, bus_size_t,
	    bus_size_t);
void	nhi_dmamem_free(bus_dma_tag_t, struct nhi_dmamem *);

int	nhi_match(struct device *, void *, void *);
void	nhi_attach(struct device *, struct device *, void *);
int	nhi_activate(struct device *, int);

const struct cfattach nhi_ca = {
	sizeof(struct nhi_softc), nhi_match, nhi_attach,
	NULL, nhi_activate
};

struct cfdriver nhi_cd = {
	NULL, "nhi", DV_DULL
};

static const struct pci_matchid nhi_devices[] = {
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_19_4X_USB4_1 },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_19_4X_USB4_2 },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_19_7X_USB4_1 },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_19_7X_USB4_2 },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_1A_2X_USB4_1 },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_1A_2X_USB4_2 },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_1A_6X_USB4_1 },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_1A_6X_USB4_2 },
};

int
nhi_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, nhi_devices, nitems(nhi_devices)));
}

void
nhi_attach(struct device *parent, struct device *self, void *aux)
{
	struct nhi_softc *sc = (struct nhi_softc *)self;
	struct pci_attach_args *pa = aux;
	const char *intrstr = NULL;
	pci_intr_handle_t ih;
	pcireg_t maptype;
	uint32_t caps;
	u_int major, minor;
	int error, i;

	maptype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, NHI_BAR);
	if (pci_mapreg_map(pa, NHI_BAR, maptype, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, &sc->sc_ios, 0) != 0) {
		printf(": can't map registers\n");
		return;
	}
	sc->sc_dmat = pa->pa_dmat;

	sc->sc_tx_ring = nhi_dmamem_alloc(sc->sc_dmat,
	    NHI_TX_NDESC * sizeof(struct nhi_desc), sizeof(struct nhi_desc));
	if (sc->sc_tx_ring == NULL) {
		printf(": can't allocate tx ring\n");
		goto unmap;
	}
	sc->sc_tx_desc = NHI_DMA_KVA(sc->sc_tx_ring);

	for (i = 0; i < NHI_TX_NDESC; i++) {
		error = bus_dmamap_create(sc->sc_dmat, NHI_TX_SIZE, 1,
		    NHI_TX_SIZE, 0, BUS_DMA_WAITOK, &sc->sc_tx_map[i]);
		if (error)
			goto free_tx_ring;
		sc->sc_tx_buf[i] = dma_alloc(NHI_TX_SIZE, PR_WAITOK);
	}

	sc->sc_rx_ring = nhi_dmamem_alloc(sc->sc_dmat,
	   NHI_RX_NDESC * sizeof(struct nhi_desc), sizeof(struct nhi_desc));
	if (sc->sc_rx_ring == NULL) {
		printf(": can't allocate rx ring\n");
		goto free_tx_ring;
	}
	sc->sc_rx_desc = NHI_DMA_KVA(sc->sc_rx_ring);

	for (i = 0; i < NHI_RX_NDESC; i++) {
		struct nhi_dmamem *rxb;

		rxb = nhi_dmamem_alloc(sc->sc_dmat, 4096, sizeof(uint32_t));
		if (rxb == NULL)
			goto free_rx_ring;
		sc->sc_rx_buf[i] = rxb;
	}

	if (pci_intr_map_msix(pa, 0, &ih)) {
		printf(": can't map interrupt\n");
		goto free_rx_ring;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);

	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO,
	    nhi_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt");
		if (intrstr)
			printf(" at %s", intrstr);
		printf("\n");
		goto free_rx_ring;
	}
	printf(": %s", intrstr);

	caps = bus_space_read_4(sc->sc_iot, sc->sc_ioh, NHI_CAPS);
	major = NHI_CAPS_VER_MAJ(caps);
	minor = NHI_CAPS_VER_MIN(caps);
	if (major == 0)
		major = 1;

	printf(", USB4 %u.%u\n", major, minor);

	nhi_init(sc);

	return;

free_rx_ring:
	nhi_dmamem_free(sc->sc_dmat, sc->sc_rx_ring);
	for (i = 0; i < NHI_RX_NDESC; i++) {
		if (sc->sc_rx_buf[i])
			nhi_dmamem_free(sc->sc_dmat, sc->sc_rx_buf[i]);
	}
free_tx_ring:
	nhi_dmamem_free(sc->sc_dmat, sc->sc_tx_ring);
	for (i = 0; i < NHI_TX_NDESC; i++) {
		if (sc->sc_tx_map[i])
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_tx_map[i]);
		if (sc->sc_tx_buf[i])
			dma_free(sc->sc_tx_buf[i], NHI_TX_SIZE);
	}
unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
}

int
nhi_activate(struct device *self, int act)
{
	struct nhi_softc *sc = (struct nhi_softc *)self;
	uint32_t data;
	int error, timo;

	switch (act) {
	case DVACT_POWERDOWN:
		if (boothowto & RB_POWERDOWN) {
			error = nhi_conf_read(sc, ROUTER_CS_5, 0,
			    ROUTER_CS, 1, &data);
			if (error) {
				printf("%s: can't read ROUTER_CS_5 (%d)\n",
				    sc->sc_dev.dv_xname, error);
				break;
			}
			error = nhi_conf_write(sc, ROUTER_CS_5, 0,
			    ROUTER_CS, 2, data | ROUTER_CS_5_SLP);
			if (error) {
				printf("%s: can't write ROUTER_CS_5 (%d)\n",
				    sc->sc_dev.dv_xname, error);
				break;
			}

			/*
			 * The USB4 Connection Manager Guide says we
			 * should poll for up to 80ms but continue
			 * even if the Sleep Ready bit doesn't get set.
			 */
			for (timo = 80; timo > 0; timo--) {
				error = nhi_conf_read(sc, ROUTER_CS_6, 0,
				    ROUTER_CS, 0, &data);
				if (error)
					break;
				if (data & ROUTER_CS_6_SLPR)
					break;
				delay(1000);
			}
			if (error) {
				printf("%s: can't read ROUTER_CS_6 (%d)\n",
				    sc->sc_dev.dv_xname, error);
				break;
			}
		}
		break;
	case DVACT_RESUME:
		nhi_init(sc);
		break;
	}

	return 0;
}

void
nhi_init(struct nhi_softc *sc)
{
	uint32_t rst;
	int i, timo;

	for (i = 0; i < NHI_RX_NDESC; i++) {
		struct nhi_desc *rxd;
		struct nhi_dmamem *rxb;

		rxb = sc->sc_rx_buf[i];

		rxd = &sc->sc_rx_desc[i];
		rxd->nd_addr_lo = NHI_DMA_DVA(rxb);
		rxd->nd_addr_hi = NHI_DMA_DVA(rxb) >> 32;
		rxd->nd_flags = NHI_DESC_RS | NHI_DESC_IE;
	}

	sc->sc_tx_prod = 0;
	sc->sc_rx_prod = 0;

	/* Reset; should complete within 10 ms (tHIReset). */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, NHI_RESET, NHI_RESET_RST);
	for (timo = 10000; timo > 0; timo--) {
		rst = bus_space_read_4(sc->sc_iot, sc->sc_ioh, NHI_RESET);
		if ((rst & NHI_RESET) == 0)
			break;
		delay(1);
	}
	if (timo == 0)
		printf("%s: reset failed\n", sc->sc_dev.dv_xname);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, NHI_TX_RING_BASE_LO(0),
	    NHI_DMA_DVA(sc->sc_tx_ring));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, NHI_TX_RING_BASE_HI(0),
	    NHI_DMA_DVA(sc->sc_tx_ring) >> 32);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, NHI_TX_RING_SIZE(0),
	    NHI_TX_NDESC);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, NHI_TX_RING_CTRL(0),
	    NHI_RING_VALID | NHI_RING_RAW);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, NHI_RX_RING_BASE_LO(0),
	    NHI_DMA_DVA(sc->sc_rx_ring));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, NHI_RX_RING_BASE_HI(0),
	    NHI_DMA_DVA(sc->sc_rx_ring) >> 32);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, NHI_RX_RING_SIZE(0),
	    NHI_RX_NDESC);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, NHI_RX_RING_CTRL(0),
	    NHI_RING_VALID | NHI_RING_RAW);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, NHI_RX_RING_PROD_CONS(0),
	    (NHI_RX_NDESC - 1) << NHI_RX_RING_CONS_SHIFT);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, NHI_IMR(0), 0x9);
}

int
nhi_tx_enqueue(struct nhi_softc *sc, uint32_t pdf, void *buf, size_t len)
{
	struct nhi_desc *txd;
	bus_dmamap_t txm;
	uint32_t *txbuf;
	int error, i;

	txbuf = sc->sc_tx_buf[sc->sc_tx_prod];
	for (i = 0; i < len / sizeof(uint32_t); i++)
		txbuf[i] = htobe32(((uint32_t *)buf)[i]);
	txbuf[i] = htobe32(crc32c(0, (uint8_t *)txbuf, len));

	txm = sc->sc_tx_map[sc->sc_tx_prod];
	error = bus_dmamap_load(sc->sc_dmat, txm,
	    txbuf, len + sizeof(uint32_t), NULL, BUS_DMA_WAITOK);
	if (error)
		return error;

	txd = &sc->sc_tx_desc[sc->sc_tx_prod];
	txd->nd_addr_lo = txm->dm_segs[0].ds_addr;
	txd->nd_addr_hi = txm->dm_segs[0].ds_addr >> 32;
	txd->nd_flags = txm->dm_segs[0].ds_len << NHI_DESC_LEN_SHIFT;
	txd->nd_flags |= pdf << NHI_DESC_EOF_PDF_SHIFT;
	txd->nd_flags |= NHI_DESC_RS;

	sc->sc_tx_prod = (sc->sc_tx_prod + 1) % NHI_TX_NDESC;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, NHI_TX_RING_PROD_CONS(0),
	    sc->sc_tx_prod << NHI_TX_RING_PROD_SHIFT);

	return 0;
}

void
nhi_rx_dequeue(struct nhi_softc *sc)
{
	struct nhi_desc *rxd;
	struct nhi_dmamem *rxb;

	rxb = sc->sc_rx_buf[sc->sc_rx_prod];

	rxd = &sc->sc_rx_desc[sc->sc_rx_prod];
	rxd->nd_addr_lo = NHI_DMA_DVA(rxb);
	rxd->nd_addr_hi = NHI_DMA_DVA(rxb) >> 32;
	rxd->nd_flags = NHI_DESC_RS | NHI_DESC_IE;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, NHI_RX_RING_PROD_CONS(0),
	    sc->sc_rx_prod << NHI_RX_RING_CONS_SHIFT);
	sc->sc_rx_prod = (sc->sc_rx_prod + 1) % NHI_RX_NDESC;
}

int
nhi_conf_read(struct nhi_softc *sc, uint32_t addr, uint32_t adapter,
    uint32_t space, uint32_t seqno, uint32_t *data)
{
	uint32_t cmd[3];
	int error;
	int timo;

	cmd[0] = 0; 			/* Route String High */
	cmd[1] = 0; 			/* Route String Low */
	cmd[2] = addr << 0;		/* Address */
	cmd[2] |= 1 << 13;		/* Read Size */
	cmd[2] |= adapter << 19;	/* Adapter Num */
	cmd[2] |= space << 25;		/* Configuration Space (CS) */
	cmd[2] |= seqno << 27;		/* Sequence Number (SN) */

	sc->sc_pdf = 0;
	error = nhi_tx_enqueue(sc, PDF_READ, cmd, sizeof(cmd));
	if (error)
		return error;

	if (cold) {
		for (timo = 100; timo > 0; timo--) {
			nhi_intr(sc);
			if (sc->sc_pdf == PDF_READ)
				break;
			delay(10);
		}
		if (timo == 0)
			return EWOULDBLOCK;
	} else {
		while (sc->sc_pdf != PDF_READ) {
			error = tsleep_nsec(&sc->sc_data, PWAIT, "nhird",
			    USEC_TO_NSEC(1000));
			if (error)
				break;
		}
		if (error)
			return error;
	}

	*data = sc->sc_data;
	return 0;
}

int
nhi_conf_write(struct nhi_softc *sc, uint32_t addr, uint32_t adapter,
    uint32_t space, uint32_t seqno, uint32_t data)
{
	uint32_t cmd[4];
	int error;
	int timo;

	cmd[0] = 0; 			/* Route String High */
	cmd[1] = 0; 			/* Route String Low */
	cmd[2] = addr << 0;		/* Address */
	cmd[2] |= 1 << 13;		/* Write Size */
	cmd[2] |= adapter << 19;	/* Adapter Num */
	cmd[2] |= space << 25;		/* Configuration Space (CS) */
	cmd[2] |= seqno << 27;		/* Sequence Number (SN) */
	cmd[3] = data;

	sc->sc_pdf = 0;
	error = nhi_tx_enqueue(sc, PDF_WRITE, cmd, sizeof(cmd));
	if (error)
		return error;

	if (cold) {
		for (timo = 100; timo > 0; timo--) {
			nhi_intr(sc);
			if (sc->sc_pdf == PDF_WRITE)
				break;
			delay(10);
		}
		if (timo == 0)
			return EWOULDBLOCK;
	} else {
		while (sc->sc_pdf != PDF_WRITE) {
			error = tsleep_nsec(&sc->sc_data, PWAIT, "nhiwr",
			    USEC_TO_NSEC(1000));
			if (error)
				break;
		}
		if (error)
			return error;
	}
	
	return 0;
}

int
nhi_intr(void *arg)
{
	struct nhi_softc *sc = arg;
	uint32_t prod, stat;

	stat = bus_space_read_4(sc->sc_iot, sc->sc_ioh, NHI_ISR(0));
	if (stat == 0)
		return 0;

	prod = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	    NHI_RX_RING_PROD_CONS(0)) >> NHI_RX_RING_PROD_SHIFT;

	while (prod != sc->sc_rx_prod) {
		struct nhi_desc *rxd;
		uint32_t cmd[3];
		uint32_t adapter;
		uint32_t upg;
		uint32_t *resp;
		uint32_t pdf;

		rxd = &sc->sc_rx_desc[sc->sc_rx_prod];
		resp = NHI_DMA_KVA(sc->sc_rx_buf[sc->sc_rx_prod]);
		pdf = (rxd->nd_flags & NHI_DESC_EOF_PDF_MASK) >>
		    NHI_DESC_EOF_PDF_SHIFT;

		switch (pdf) {
		case PDF_READ:
			sc->sc_pdf = PDF_READ;
			sc->sc_data = be32toh(resp[3]);
			wakeup(&sc->sc_data);
			break;
		case PDF_WRITE:
			sc->sc_pdf = PDF_WRITE;
			wakeup(&sc->sc_pdf);
			break;
		case PDF_HOTPLUG:
			adapter = (be32toh(resp[2]) & 0x1f) >> 0;
			upg = (be32toh(resp[2]) & 0x80000000) >> 31;

			/*
			 * We need to acknowledge hotplug events even
			 * though we don't do anything with them.
			 */
			cmd[0] = 0; 		/* Route String High */
			cmd[1] = 0; 		/* Route String Low */
			cmd[2] = HP_ACK << 0;	/* Event Code */
			cmd[2] |= adapter << 8;	/* Adapter Num */
			cmd[2] |= (upg | 0x2U) << 30;	/* PG */

			nhi_tx_enqueue(sc, PDF_NOTIF, cmd, sizeof(cmd));
			break;
		}

		nhi_rx_dequeue(sc);
	}

	return 1;
}

struct nhi_dmamem *
nhi_dmamem_alloc(bus_dma_tag_t dmat, bus_size_t size, bus_size_t align)
{
	struct nhi_dmamem *ndm;
	int nsegs;

	ndm = malloc(sizeof(*ndm), M_DEVBUF, M_WAITOK | M_ZERO);
	ndm->ndm_size = size;

	if (bus_dmamap_create(dmat, size, 1, size, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW, &ndm->ndm_map) != 0)
		goto ndmfree;

	if (bus_dmamem_alloc(dmat, size, align, 0, &ndm->ndm_seg, 1,
	    &nsegs, BUS_DMA_WAITOK | BUS_DMA_ZERO) != 0)
		goto destroy;

	if (bus_dmamem_map(dmat, &ndm->ndm_seg, nsegs, size,
	    &ndm->ndm_kva, BUS_DMA_WAITOK | BUS_DMA_NOCACHE) != 0)
		goto free;

	if (bus_dmamap_load_raw(dmat, ndm->ndm_map, &ndm->ndm_seg,
	    nsegs, size, BUS_DMA_WAITOK) != 0)
		goto unmap;

	return ndm;

unmap:
	bus_dmamem_unmap(dmat, ndm->ndm_kva, size);
free:
	bus_dmamem_free(dmat, &ndm->ndm_seg, 1);
destroy:
	bus_dmamap_destroy(dmat, ndm->ndm_map);
ndmfree:
	free(ndm, M_DEVBUF, sizeof(*ndm));

	return NULL;
}

void
nhi_dmamem_free(bus_dma_tag_t dmat, struct nhi_dmamem *ndm)
{
	bus_dmamem_unmap(dmat, ndm->ndm_kva, ndm->ndm_size);
	bus_dmamem_free(dmat, &ndm->ndm_seg, 1);
	bus_dmamap_destroy(dmat, ndm->ndm_map);
	free(ndm, M_DEVBUF, sizeof(*ndm));
}
