/*	$OpenBSD: ahci.c,v 1.35 2007/01/04 12:02:49 dlg Exp $ */

/*
 * Copyright (c) 2006 David Gwynne <dlg@openbsd.org>
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
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define AHCI_DEBUG

#ifdef AHCI_DEBUG
#define DPRINTF(m, f...) do { if (ahcidebug & (m)) printf(f); } while (0)
#define AHCI_D_VERBOSE		0x01
int ahcidebug = AHCI_D_VERBOSE;
#else
#define DPRINTF(m, f...)
#endif

#define AHCI_PCI_BAR		0x24

#define AHCI_REG_CAP		0x000 /* HBA Capabilities */
#define  AHCI_REG_CAP_NP(_r)		(((_r) & 0x1f)+1) /* Number of Ports */
#define  AHCI_REG_CAP_SXS		(1<<5) /* External SATA */
#define  AHCI_REG_CAP_EMS		(1<<6) /* Enclosure Mgmt */
#define  AHCI_REG_CAP_CCCS		(1<<7) /* Cmd Coalescing */
#define  AHCI_REG_CAP_NCS(_r)		((((_r) & 0x1f00)>>8)+1) /* NCmds*/
#define  AHCI_REG_CAP_PSC		(1<<13) /* Partial State Capable */
#define  AHCI_REG_CAP_SSC		(1<<14) /* Slumber State Capable */
#define  AHCI_REG_CAP_PMD		(1<<15) /* PIO Multiple DRQ Block */
#define  AHCI_REG_CAP_FBSS		(1<<16) /* FIS-Based Switching */
#define  AHCI_REG_CAP_SPM		(1<<17) /* Port Multiplier */
#define  AHCI_REG_CAP_SAM		(1<<18) /* AHCI Only mode */
#define  AHCI_REG_CAP_SNZO		(1<<19) /* Non Zero DMA Offsets */
#define  AHCI_REG_CAP_ISS		(0xf<<20) /* Interface Speed Support */
#define  AHCI_REG_CAP_ISS_G1		(0x1<<20) /* Gen 1 (1.5 Gbps) */
#define  AHCI_REG_CAP_ISS_G1_2		(0x2<<20) /* Gen 1 and 2 (3 Gbps) */
#define  AHCI_REG_CAP_SCLO		(1<<24) /* Cmd List Override */
#define  AHCI_REG_CAP_SAL		(1<<25) /* Activity LED */
#define  AHCI_REG_CAP_SALP		(1<<26) /* Aggresive Link Pwr Mgmt */
#define  AHCI_REG_CAP_SSS		(1<<27) /* Staggered Spinup */
#define  AHCI_REG_CAP_SMPS		(1<<28) /* Mech Presence Switch */
#define  AHCI_REG_CAP_SSNTF		(1<<29) /* SNotification Register */
#define  AHCI_REG_CAP_SNCQ		(1<<30) /* Native Cmd Queuing */
#define  AHCI_REG_CAP_S64A		(1<<31) /* 64bit Addressing */

#define  AHCI_FMT_CAP		"\020" "\006SXS" "\007EMS" "\010CCCS" \
				    "\016PSC" "\017SSC" "\020PMD" "\021FBSS" \
				    "\022SPM" "\023SAM" "\024SNZO" "\031SCLO" \
				    "\032SAL" "\033SALP" "\034SSS" "\035SMPS" \
				    "\036SSNTF" "\037NCQ" "\040S64A"

#define AHCI_REG_GHC		0x004 /* Global HBA Control */
#define  AHCI_REG_GHC_HR		(1<<0) /* HBA Reset */
#define  AHCI_REG_GHC_IE		(1<<1) /* Interrupt Enable */
#define  AHCI_REG_GHC_MRSM		(1<<2) /* MSI Revert to Single Msg */
#define  AHCI_REG_GHC_AE		(1<<31) /* AHCI Enable */
#define AHCI_REG_IS		0x008 /* Interrupt Status */
#define AHCI_REG_PI		0x00c /* Ports Implemented */
#define AHCI_REG_VS		0x010 /* AHCI Version */
#define  AHCI_REG_VS_0_95		0x00000905 /* 0.95 */
#define  AHCI_REG_VS_1_0		0x00010000 /* 1.0 */
#define  AHCI_REG_VS_1_1		0x00010100 /* 1.1 */
#define AHCI_REG_CCC_CTL	0x014 /* Coalescing Control */
#define AHCI_REG_CCC_PORTS	0x018 /* Coalescing Ports */
#define AHCI_REG_EM_LOC		0x01c /* Enclosure Mgmt Location */
#define AHCI_REG_EM_CTL		0x020 /* Enclosure Mgmt Control */

#define AHCI_PORT_REGION(_p)	(0x100 + ((_p) * 0x80))
#define AHCI_PORT_SIZE		0x80

#define AHCI_PREG_CLB		0x00 /* Cmd List Base Addr */
#define AHCI_PREG_CLBU		0x04 /* Cmd List Base Hi Addr */
#define AHCI_PREG_FB		0x08 /* FIS Base Addr */
#define AHCI_PREG_FBU		0x0c /* FIS Base Hi Addr */
#define AHCI_PREG_IS		0x10 /* Interrupt Status */
#define AHCI_PREG_IE		0x14 /* Interrupt Enable */
#define AHCI_PREG_CMD		0x18 /* Command and Status */
#define AHCI_PREG_TFD		0x20 /* Task File Data*/
#define AHCI_PREG_SIG		0x24 /* Signature */
#define AHCI_PREG_SSTS		0x28 /* SATA Status */
#define AHCI_PREG_SCTL		0x2c /* SATA Control */
#define AHCI_PREG_SERR		0x30 /* SATA Error */
#define AHCI_PREG_ACT		0x34 /* SATA Active */
#define AHCI_PREG_CI		0x38 /* Command Issue */
#define AHCI_PREG_SNTF		0x3c /* SNotification */

struct ahci_cmd_list {
	u_int16_t		prdtl; /* sgl len */
	u_int16_t		flags;

	u_int32_t		prdbc; /* datalen */

	u_int32_t		ctba_lo;
	u_int32_t		ctba_hi;

	u_int32_t		reserved[4];
} __packed;

struct ahci_rfis {
	u_int8_t		dsfis[28];
	u_int8_t		reserved1[4];
	u_int8_t		psfis[24];
	u_int8_t		reserved2[8];
	u_int8_t		rfis[24];
	u_int8_t		reserved3[4];
	u_int8_t		sdbfis[4];
	u_int8_t		ufis[64];
	u_int8_t		reserved4[96];
} __packed;

struct ahci_cmd_table {
	u_int8_t		cfis[64];
	u_int8_t		acmd[16];
	u_int8_t		reserved[48];
} __packed;

struct ahci_prdt {
	u_int32_t		dba_lo;
	u_int32_t		dba_hi;
	u_int32_t		reserved;
	u_int32_t		flags;
} __packed;

/* this makes ahci_cmd 512 bytes, which is good for alignment */
#define AHCI_MAX_PRDT		24

struct ahci_cmd {
	struct ahci_cmd_table	table;
	struct ahci_prdt		prdt[AHCI_MAX_PRDT];
} __packed;

#define AHCI_MAX_PORTS		32

static const struct pci_matchid ahci_devices[] = {
	{ PCI_VENDOR_JMICRON,	PCI_PRODUCT_JMICRON_JMB361 }
};

int			ahci_match(struct device *, void *, void *);
void			ahci_attach(struct device *, struct device *, void *);

struct ahci_dmamem {
	bus_dmamap_t		adm_map;
	bus_dma_segment_t	adm_seg;
	size_t			adm_size;
	caddr_t			adm_kva;
};
#define AHCI_DMA_MAP(_adm)	((_adm)->adm_map)
#define AHCI_DMA_DVA(_adm)	((_adm)->adm_map->dm_segs[0].ds_addr)
#define AHCI_DMA_KVA(_adm)	((void *)(_adm)->adm_kva)

struct ahci_softc;

struct ahci_ccb {
	int			ccb_id;

	struct ahci_cmd		*ccb_cmd;
	u_int64_t		ccb_cmd_dva;

	bus_dmamap_t		ccb_dmamap;

	TAILQ_ENTRY(ahci_ccb)	ccb_entry;
};

struct ahci_port {
	struct ahci_softc	*ap_sc;
	bus_space_handle_t	ap_ioh;

	struct ahci_dmamem	*ap_dmamem;
	struct ahci_rfis	*ap_rfis;
	struct ahci_cmd_list	*ap_cmd_list;

	struct ahci_ccb		*ap_ccbs;
	TAILQ_HEAD(, ahci_ccb)	ap_ccb_free;
};

struct ahci_softc {
	struct device		sc_dev;

	void			*sc_ih;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	bus_dma_tag_t		sc_dmat;

	u_int			sc_ncmds;
	struct ahci_port	*sc_ports[AHCI_MAX_PORTS];
};
#define DEVNAME(_s)	((_s)->sc_dev.dv_xname)

struct cfattach ahci_ca = {
	sizeof(struct ahci_softc), ahci_match, ahci_attach
};

struct cfdriver ahci_cd = {
	NULL, "ahci", DV_DULL
};

int			ahci_intr(void *);

int			ahci_map_regs(struct ahci_softc *,
			    struct pci_attach_args *);
void			ahci_unmap_regs(struct ahci_softc *,
			    struct pci_attach_args *);
int			ahci_init(struct ahci_softc *);
int			ahci_map_intr(struct ahci_softc *,
			    struct pci_attach_args *);
void			ahci_unmap_intr(struct ahci_softc *,
			    struct pci_attach_args *);
int			ahci_port_alloc(struct ahci_softc *, u_int);
void			ahci_port_free(struct ahci_softc *, u_int);

struct ahci_ccb		*ahci_get_ccb(struct ahci_port *);
void			ahci_put_ccb(struct ahci_port *, struct ahci_ccb *);

struct ahci_dmamem	*ahci_dmamem_alloc(struct ahci_softc *, size_t);
void			ahci_dmamem_free(struct ahci_softc *,
			    struct ahci_dmamem *);

u_int32_t		ahci_read(struct ahci_softc *, bus_size_t);
void			ahci_write(struct ahci_softc *, bus_size_t, u_int32_t);
u_int32_t		ahci_pread(struct ahci_port *, bus_size_t);
void			ahci_pwrite(struct ahci_port *, bus_size_t, u_int32_t);
int			ahci_wait_eq(struct ahci_softc *, bus_size_t,
			    u_int32_t, u_int32_t);
int			ahci_wait_ne(struct ahci_softc *, bus_size_t,
			    u_int32_t, u_int32_t);

int
ahci_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, ahci_devices,
	    sizeof(ahci_devices) / sizeof(ahci_devices[0])));
}

void
ahci_attach(struct device *parent, struct device *self, void *aux)
{
	struct ahci_softc		*sc = (struct ahci_softc *)self;
	struct pci_attach_args		*pa = aux;
	u_int32_t			reg;
	int				i;

	if (ahci_map_regs(sc, pa) != 0) {
		/* error already printed by ahci_map_regs */
		return;
	}

	if (ahci_init(sc) != 0) {
		/* error already printed by ahci_init */
		goto unmap;
	}

	if (ahci_map_intr(sc, pa) != 0) {
		/* error already printed by ahci_map_intr */
		goto unmap;
	}

	printf("\n");

	sc->sc_dmat = pa->pa_dmat;

	reg = ahci_read(sc, AHCI_REG_CAP);
	sc->sc_ncmds = AHCI_REG_CAP_NCS(reg);
#ifdef AHCI_DEBUG
	if (ahcidebug & AHCI_D_VERBOSE) {
		const char *gen;

		switch (reg & AHCI_REG_CAP_ISS) {
		case AHCI_REG_CAP_ISS_G1:
			gen = "1 (1.5Gbps)";
			break;
		case AHCI_REG_CAP_ISS_G1_2:
			gen = "1 (1.5Gbps) and 2 (3Gbps)";
			break;
		default:
			gen = "unknown";
			break;
		}

		printf("%s: capabilities: 0x%b ports: %d ncmds: %d gen: %s\n",
		    DEVNAME(sc), reg, AHCI_FMT_CAP,
		    AHCI_REG_CAP_NP(reg), sc->sc_ncmds, gen);
	}
#endif

	reg = ahci_read(sc, AHCI_REG_PI);
	DPRINTF(AHCI_D_VERBOSE, "%s: ports implemented: 0x%08x\n",
	    DEVNAME(sc), reg);
	for (i = 0; i < AHCI_MAX_PORTS; i++) {
		if (!ISSET(reg, 1 << i)) {
			/* dont allocate stuff if the port isnt implemented */
			continue;
		}

		if (ahci_port_alloc(sc, i) != 0)
			goto freeports;
	}

        return;

freeports:
	for (i = 0; i < AHCI_MAX_PORTS; i++) {
		if (sc->sc_ports[i] != NULL)
			ahci_port_free(sc, i);
	}
unmap:
	ahci_unmap_regs(sc, pa);
}

int
ahci_map_regs(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	pcireg_t			memtype;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, AHCI_PCI_BAR);
	if (pci_mapreg_map(pa, AHCI_PCI_BAR, memtype, 0, &sc->sc_iot,
	    &sc->sc_ioh, NULL, &sc->sc_ios, 0) != 0) {
		printf(": unable to map registers\n");
		return (1);
	}

	return (0);
}

void
ahci_unmap_regs(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	sc->sc_ios = 0;
}

int
ahci_map_intr(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	pci_intr_handle_t		ih;
	const char			*intrstr;

	if (pci_intr_map(pa, &ih) != 0) {
		printf(": unable to map interrupt\n");
		return (1);
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO,
	    ahci_intr, sc, DEVNAME(sc));
	if (sc->sc_ih == NULL) {
		printf(": unable to map interrupt%s%s\n",
		    intrstr == NULL ? "" : " at ",
		    intrstr == NULL ? "" : intrstr);
		return (1);
	}
	printf(": %s", intrstr);

	return (0);
}

void
ahci_unmap_intr(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	pci_intr_disestablish(pa->pa_pc, sc->sc_ih);
}

int
ahci_init(struct ahci_softc *sc)
{
	u_int32_t			reg;
	const char			*revision;

	/* reset the controller */
	ahci_write(sc, AHCI_REG_GHC, AHCI_REG_GHC_HR);
	if (ahci_wait_ne(sc, AHCI_REG_GHC, AHCI_REG_GHC_HR,
	    AHCI_REG_GHC_HR) != 0) {
		printf(": unable to reset controller\n");
		return (1);
	}

	/* enable ahci */
	ahci_write(sc, AHCI_REG_GHC, AHCI_REG_GHC_AE);

	/* check the revision */
	reg = ahci_read(sc, AHCI_REG_VS);
	switch (reg) {
	case AHCI_REG_VS_0_95:
		revision = "0.95";
		break;
	case AHCI_REG_VS_1_0:
		revision = "1.0";
		break;
	case AHCI_REG_VS_1_1:
		revision = "1.1";
		break;

	default:
		printf(": unsupported AHCI revision 0x%08x\n", reg);
		return (1);
	}

	/* clean interrupts */
	reg = ahci_read(sc, AHCI_REG_IS);
	ahci_write(sc, AHCI_REG_IS, reg);

	printf(": AHCI %s", revision);

	return (0);
}

int
ahci_port_alloc(struct ahci_softc *sc, u_int port)
{
	struct ahci_port		*ap;
	struct ahci_ccb			*ccb;
	u_int8_t			*kva;
	u_int64_t			dva;
	int				offset = 0;
	int				i;

	ap = malloc(sizeof(struct ahci_port), M_DEVBUF, M_NOWAIT);
	if (ap == NULL) {
		printf("%s: unable to allocate memory for port %d\n",
		    DEVNAME(sc), port);
		return (1);
	}
	bzero(ap, sizeof(struct ahci_port));

	ap->ap_sc = sc;
	TAILQ_INIT(&ap->ap_ccb_free);

	ap->ap_ccbs = malloc(sizeof(struct ahci_ccb) * sc->sc_ncmds, M_DEVBUF,
	    M_NOWAIT);
	if (ap->ap_ccbs == NULL) {
		printf("%s: unable to allocate command list for port %d\n",
		    DEVNAME(sc), port);
		goto freeport;
	}
	bzero(ap->ap_ccbs, sizeof(struct ahci_ccb) * sc->sc_ncmds);

	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh,
	    AHCI_PORT_REGION(i), AHCI_PORT_SIZE, &ap->ap_ioh) != 0) {
		printf("%s: unable to create register window for port %d\n",
		    DEVNAME(sc), port);
		goto freeccbs;
	}

	/* calculate the size of the dmaable memory this port will need.  */
	i = sizeof(struct ahci_cmd) * sc->sc_ncmds +
	    sizeof(struct ahci_rfis) +
	    sizeof(struct ahci_cmd_list) * sc->sc_ncmds;
	ap->ap_dmamem = ahci_dmamem_alloc(sc, i);
	if (ap->ap_dmamem == NULL) {
		printf("unable to allocate dmamem for port %d\n", DEVNAME(sc),
		    port);
		goto freeccbs;
	}
	kva = AHCI_DMA_KVA(ap->ap_dmamem);
	dva = AHCI_DMA_DVA(ap->ap_dmamem);

	for (i = 0; i < sc->sc_ncmds; i++) {
		ccb = &ap->ap_ccbs[i];

		if (bus_dmamap_create(sc->sc_dmat, MAXPHYS, AHCI_MAX_PRDT,
		    MAXPHYS, 0, 0, &ccb->ccb_dmamap) != 0) {
			printf("unable to create dmamap for port %d ccb %d\n",
			    DEVNAME(sc), port, i);
			goto freemaps;
		}

		ccb->ccb_id = i;
		ccb->ccb_cmd = (struct ahci_cmd *)(kva + offset);
		ccb->ccb_cmd_dva = dva + offset;
		offset += sizeof(struct ahci_cmd);

		ahci_put_ccb(ap, ccb);
	}

	ap->ap_rfis = (struct ahci_rfis *)(kva + offset);
	ahci_pwrite(ap, AHCI_PREG_FB, (u_int32_t)(dva + offset));
	ahci_pwrite(ap, AHCI_PREG_FBU, (u_int32_t)((dva + offset) >> 32));
	offset += sizeof(struct ahci_rfis);

	ap->ap_cmd_list = (struct ahci_cmd_list *)(kva + offset);
	ahci_pwrite(ap, AHCI_PREG_CLB, (u_int32_t)(dva + offset));
	ahci_pwrite(ap, AHCI_PREG_CLBU, (u_int32_t)((dva + offset) >> 32));

	sc->sc_ports[port] = ap;

	return (0);

freemaps:
	while ((ccb = ahci_get_ccb(ap)) != NULL)
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);
	ahci_dmamem_free(sc, ap->ap_dmamem);
freeccbs:
	free(ap->ap_ccbs, M_DEVBUF);
freeport:
	free(ap, M_DEVBUF);
	return (1);
}

void
ahci_port_free(struct ahci_softc *sc, u_int port)
{
	struct ahci_port		*ap;
	struct ahci_ccb			*ccb;
	int				i;

	ap = sc->sc_ports[i];

	for (i = 0; i < sc->sc_ncmds; i++) {
		ccb = &ap->ap_ccbs[i];

		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);
	}

	ahci_dmamem_free(sc, ap->ap_dmamem);
	/* bus_space(9) says we dont free the subregions handle */
	free(ap->ap_ccbs, M_DEVBUF);
	free(ap, M_DEVBUF);
	sc->sc_ports[i] = NULL;
}

int
ahci_intr(void *arg)
{
	return (0);
}

struct ahci_ccb *
ahci_get_ccb(struct ahci_port *ap)
{
	struct ahci_ccb			*ccb;

	ccb = TAILQ_FIRST(&ap->ap_ccb_free);
	if (ccb != NULL)
		TAILQ_REMOVE(&ap->ap_ccb_free, ccb, ccb_entry);

	return (ccb);
}

void
ahci_put_ccb(struct ahci_port *ap, struct ahci_ccb *ccb)
{
	/* scrub bits */
	TAILQ_INSERT_TAIL(&ap->ap_ccb_free, ccb, ccb_entry);
}

struct ahci_dmamem *
ahci_dmamem_alloc(struct ahci_softc *sc, size_t size)
{
	struct ahci_dmamem		*adm;
	int				nsegs;

	adm = malloc(sizeof(struct ahci_dmamem), M_DEVBUF, M_NOWAIT);
	if (adm == NULL)
		return (NULL);

	bzero(adm, sizeof(struct ahci_dmamem));
	adm->adm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &adm->adm_map) != 0)
		goto admfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &adm->adm_seg,
	    1, &nsegs, BUS_DMA_NOWAIT) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &adm->adm_seg, nsegs, size,
	    &adm->adm_kva, BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, adm->adm_map, adm->adm_kva, size,
	    NULL, BUS_DMA_NOWAIT) != 0)
		goto unmap;

	bzero(adm->adm_kva, size);

	return (adm);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, adm->adm_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &adm->adm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, adm->adm_map);
admfree:
	free(adm, M_DEVBUF);

	return (NULL);
}

void
ahci_dmamem_free(struct ahci_softc *sc, struct ahci_dmamem *adm)
{
	bus_dmamap_unload(sc->sc_dmat, adm->adm_map);
	bus_dmamem_unmap(sc->sc_dmat, adm->adm_kva, adm->adm_size);
	bus_dmamem_free(sc->sc_dmat, &adm->adm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, adm->adm_map);
	free(adm, M_DEVBUF);
}

u_int32_t
ahci_read(struct ahci_softc *sc, bus_size_t r)
{
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->sc_iot, sc->sc_ioh, r));
}

void
ahci_write(struct ahci_softc *sc, bus_size_t r, u_int32_t v)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, r, v);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

u_int32_t
ahci_pread(struct ahci_port *ap, bus_size_t r)
{
	bus_space_barrier(ap->ap_sc->sc_iot, ap->ap_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(ap->ap_sc->sc_iot, ap->ap_ioh, r));
}

void
ahci_pwrite(struct ahci_port *ap, bus_size_t r, u_int32_t v)
{
	bus_space_write_4(ap->ap_sc->sc_iot, ap->ap_ioh, r, v);
	bus_space_barrier(ap->ap_sc->sc_iot, ap->ap_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

int
ahci_wait_eq(struct ahci_softc *sc, bus_size_t r, u_int32_t mask,
    u_int32_t target)
{
	int				i;

	for (i = 0; i < 1000; i++) {
		if ((ahci_read(sc, r) & mask) == target)
			return (0);
		delay(1000);
	}

	return (1);
}

int
ahci_wait_ne(struct ahci_softc *sc, bus_size_t r, u_int32_t mask,
    u_int32_t target)
{
	int				i;

	for (i = 0; i < 1000; i++) {
		if ((ahci_read(sc, r) & mask) != target)
			return (0);
		delay(1000);
	}

	return (1);
}
