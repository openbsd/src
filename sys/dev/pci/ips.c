/*	$OpenBSD: ips.c,v 1.9 2006/11/28 19:59:14 grange Exp $	*/

/*
 * Copyright (c) 2006 Alexander Yurchenko <grange@openbsd.org>
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
 * IBM ServeRAID controller driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <sys/queue.h>

#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#define IPS_DEBUG	/* XXX: remove when the driver becomes stable */

/* Debug levels */
#define IPS_D_ERR	0x0001
#define IPS_D_INFO	0x0002
#define IPS_D_XFER	0x0004
#define IPS_D_INTR	0x0008

#ifdef IPS_DEBUG
#define DPRINTF(a, b)	if (ips_debug & (a)) printf b
int ips_debug = IPS_D_ERR;
#else
#define DPRINTF(a, b)
#endif

/*
 * Register definitions.
 */
#define IPS_BAR0		0x10	/* I/O space base address */
#define IPS_BAR1		0x14	/* I/O space base address */

#define IPS_MORPHEUS_OISR	0x0030	/* outbound IRQ status */
#define	IPS_MORPHEUS_OISR_CMD		(1 << 3)
#define IPS_MORPHEUS_OIMR	0x0034	/* outbound IRQ mask */
#define IPS_MORPHEUS_IQPR	0x0040	/* inbound queue port */
#define IPS_MORPHEUS_OQPR	0x0044	/* outbound queue port */

/* Commands */
#define IPS_CMD_READ		0x02
#define IPS_CMD_WRITE		0x03
#define IPS_CMD_ADAPTERINFO	0x05
#define IPS_CMD_FLUSHCACHE	0x0a
#define IPS_CMD_READ_SG		0x82
#define IPS_CMD_WRITE_SG	0x83
#define IPS_CMD_DRIVEINFO	0x19

#define IPS_MAXCMDSZ		256	/* XXX: for now */
#define IPS_MAXDATASZ		64 * 1024
#define IPS_MAXSEGS		32

#define IPS_MAXDRIVES		8
#define IPS_MAXCHANS		4
#define IPS_MAXTARGETS		15
#define IPS_MAXCMDS		32

#define IPS_MAXFER		(64 * 1024)
#define IPS_MAXSGS		32

/* Command frames */
struct ips_cmd_adapterinfo {
	u_int8_t	command;
	u_int8_t	id;
	u_int8_t	reserve1;
	u_int8_t	commandtype;
	u_int32_t	reserve2;
	u_int32_t	buffaddr;
	u_int32_t	reserve3;
} __packed;

struct ips_cmd_driveinfo {
	u_int8_t	command;
	u_int8_t	id;
	u_int8_t	drivenum;
	u_int8_t	reserve1;
	u_int32_t	reserve2;
	u_int32_t	buffaddr;
	u_int32_t	reserve3;
} __packed;

struct ips_cmd_generic {
	u_int8_t	command;
	u_int8_t	id;
	u_int8_t	drivenum;
	u_int8_t	reserve2;
	u_int32_t	lba;
	u_int32_t	buffaddr;
	u_int32_t	reserve3;
} __packed;

struct ips_cmd_io {
	u_int8_t	command;
	u_int8_t	id;
	u_int8_t	drivenum;
	u_int8_t	segnum;
	u_int32_t	lba;
	u_int32_t	buffaddr;
	u_int16_t	length;
	u_int16_t	reserve1;
} __packed;

/* Data frames */
struct ips_adapterinfo {
	u_int8_t	drivecount;
	u_int8_t	miscflags;
	u_int8_t	SLTflags;
	u_int8_t	BSTflags;
	u_int8_t	pwr_chg_count;
	u_int8_t	wrong_addr_count;
	u_int8_t	unident_count;
	u_int8_t	nvram_dev_chg_count;
	u_int8_t	codeblock_version[8];
	u_int8_t	bootblock_version[8];
	u_int32_t	drive_sector_count[IPS_MAXDRIVES];
	u_int8_t	max_concurrent_cmds;
	u_int8_t	max_phys_devices;
	u_int16_t	flash_prog_count;
	u_int8_t	defunct_disks;
	u_int8_t	rebuildflags;
	u_int8_t	offline_drivecount;
	u_int8_t	critical_drivecount;
	u_int16_t	config_update_count;
	u_int8_t	blockedflags;
	u_int8_t	psdn_error;
	u_int16_t	addr_dead_disk[IPS_MAXCHANS][IPS_MAXTARGETS];
} __packed;

struct ips_drive {
	u_int8_t	drivenum;
	u_int8_t	merge_id;
	u_int8_t	raid_lvl;
	u_int8_t	state;
	u_int32_t	sector_count;
} __packed;

struct ips_driveinfo {
	u_int8_t	drivecount;
	u_int8_t	reserve1;
	u_int16_t	reserve2;
	struct ips_drive drives[IPS_MAXDRIVES];
} __packed;

/* I/O access helper macros */
#define IPS_READ_4(s, r) \
	bus_space_read_4((s)->sc_iot, (s)->sc_ioh, (r))
#define IPS_WRITE_4(s, r, v) \
	bus_space_write_4((s)->sc_iot, (s)->sc_ioh, (r), (v))

struct ccb {
	int			c_id;
	int			c_flags;
#define CCB_F_RUN	0x0001

	bus_dmamap_t		c_dmam;
	struct scsi_xfer *	c_xfer;
	struct timeout		c_timo;

	TAILQ_ENTRY(ccb)	c_link;
};

TAILQ_HEAD(ccbq, ccb);

struct dmamem {
	bus_dma_tag_t		dm_tag;
	bus_dmamap_t		dm_map;
	bus_dma_segment_t	dm_seg;
	bus_size_t		dm_size;
	void *			dm_kva;
};

struct ips_softc {
	struct device		sc_dev;

	struct scsi_link	sc_scsi_link;
	struct scsibus_softc *	sc_scsi_bus;

	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_dma_tag_t		sc_dmat;

	struct dmamem *		sc_cmdm;

	struct ccb *		sc_ccb;
	struct ccbq		sc_ccbq;

	void *			sc_ih;

	void			(*sc_exec)(struct ips_softc *);
	void			(*sc_inten)(struct ips_softc *);
	int			(*sc_intr)(void *);

	struct ips_adapterinfo	sc_ai;
	struct ips_driveinfo	sc_di;
};

int	ips_match(struct device *, void *, void *);
void	ips_attach(struct device *, struct device *, void *);

int	ips_scsi_cmd(struct scsi_xfer *);
int	ips_scsi_io(struct scsi_xfer *);
int	ips_scsi_ioctl(struct scsi_link *, u_long, caddr_t, int,
	    struct proc *);
void	ips_scsi_minphys(struct buf *);

void	ips_xfer_timeout(void *);

void	ips_flushcache(struct ips_softc *);
int	ips_getadapterinfo(struct ips_softc *, struct ips_adapterinfo *);
int	ips_getdriveinfo(struct ips_softc *, struct ips_driveinfo *);

void	ips_copperhead_exec(struct ips_softc *);
void	ips_copperhead_inten(struct ips_softc *);
int	ips_copperhead_intr(void *);

void	ips_morpheus_exec(struct ips_softc *);
void	ips_morpheus_inten(struct ips_softc *);
int	ips_morpheus_intr(void *);

struct ccb *	ips_ccb_alloc(bus_dma_tag_t, int);
void		ips_ccb_free(struct ccb *, bus_dma_tag_t, int);

struct dmamem *	ips_dmamem_alloc(bus_dma_tag_t, bus_size_t);
void		ips_dmamem_free(struct dmamem *);

struct cfattach ips_ca = {
	sizeof(struct ips_softc),
	ips_match,
	ips_attach
};

struct cfdriver ips_cd = {
	NULL, "ips", DV_DULL
};

static const struct pci_matchid ips_ids[] = {
	{ PCI_VENDOR_IBM,	PCI_PRODUCT_IBM_SERVERAID },
	{ PCI_VENDOR_IBM,	PCI_PRODUCT_IBM_SERVERAID2 },
	{ PCI_VENDOR_ADP2,	PCI_PRODUCT_ADP2_SERVERAID }
};

static struct scsi_adapter ips_scsi_adapter = {
	ips_scsi_cmd,
	ips_scsi_minphys,
	NULL,
	NULL,
	ips_scsi_ioctl
};

static struct scsi_device ips_scsi_device = {
	NULL,
	NULL,
	NULL,
	NULL
};

int
ips_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, ips_ids,
	    sizeof(ips_ids) / sizeof(ips_ids[0])));
}

void
ips_attach(struct device *parent, struct device *self, void *aux)
{
	struct ips_softc *sc = (struct ips_softc *)self;
	struct pci_attach_args *pa = aux;
	int bar;
	pcireg_t maptype;
	bus_size_t iosize;
	pci_intr_handle_t ih;
	const char *intrstr;
	int i, maxcmds;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	/* Identify the chipset */
	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_IBM_SERVERAID:
		printf(": Copperhead");
		sc->sc_exec = ips_copperhead_exec;
		sc->sc_inten = ips_copperhead_inten;
		sc->sc_intr = ips_copperhead_intr;
		break;
	case PCI_PRODUCT_IBM_SERVERAID2:
	case PCI_PRODUCT_ADP2_SERVERAID:
		printf(": Morpheus");
		sc->sc_exec = ips_morpheus_exec;
		sc->sc_inten = ips_morpheus_inten;
		sc->sc_intr = ips_morpheus_intr;
		break;
	}

	/* Map I/O space */
	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_IBM_SERVERAID)
		bar = IPS_BAR1;
	else
		bar = IPS_BAR0;
	maptype = pci_mapreg_type(sc->sc_pc, sc->sc_tag, bar);
	if (pci_mapreg_map(pa, bar, maptype, 0, &sc->sc_iot, &sc->sc_ioh,
	    NULL, &iosize, 0)) {
		printf(": can't map I/O space\n");
		return;
	}

	/* Allocate command DMA buffer */
	if ((sc->sc_cmdm = ips_dmamem_alloc(sc->sc_dmat,
	    IPS_MAXCMDSZ)) == NULL) {
		printf(": can't alloc command DMA buffer\n");
		goto fail1;
	}

	/* Get adapter info */
	if (ips_getadapterinfo(sc, &sc->sc_ai)) {
		printf(": can't get adapter info\n");
		goto fail2;
	}

	/* Get logical drives info */
	if (ips_getdriveinfo(sc, &sc->sc_di)) {
		printf(": can't get drives info\n");
		goto fail2;
	}

	/* Allocate command queue */
	maxcmds = sc->sc_ai.max_concurrent_cmds;
	if ((sc->sc_ccb = ips_ccb_alloc(sc->sc_dmat, maxcmds)) == NULL) {
		printf(": can't alloc command queue\n");
		goto fail2;
	}

	TAILQ_INIT(&sc->sc_ccbq);
	for (i = 0; i < maxcmds; i++)
		TAILQ_INSERT_TAIL(&sc->sc_ccbq, &sc->sc_ccb[i], c_link);

	/* Install interrupt handler */
	if (pci_intr_map(pa, &ih)) {
		printf(": can't map interrupt\n");
		goto fail3;
	}
	intrstr = pci_intr_string(sc->sc_pc, ih);
	if ((sc->sc_ih = pci_intr_establish(sc->sc_pc, ih, IPL_BIO,
	    sc->sc_intr, sc, sc->sc_dev.dv_xname)) == NULL) {
		printf(": can't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail3;
	}
	printf(", %s\n", intrstr);

	/* Enable interrupts */
	(*sc->sc_inten)(sc);

	/* Attach SCSI bus */
	sc->sc_scsi_link.openings = IPS_MAXCMDS;	/* XXX: for now */
	sc->sc_scsi_link.adapter_target = IPS_MAXTARGETS;
	sc->sc_scsi_link.adapter_buswidth = IPS_MAXTARGETS;
	sc->sc_scsi_link.device = &ips_scsi_device;
	sc->sc_scsi_link.adapter = &ips_scsi_adapter;
	sc->sc_scsi_link.adapter_softc = sc;

	sc->sc_scsi_bus = (struct scsibus_softc *)config_found(self,
	    &sc->sc_scsi_link, scsiprint);

	return;
fail3:
	ips_ccb_free(sc->sc_ccb, sc->sc_dmat, maxcmds);
fail2:
	ips_dmamem_free(sc->sc_cmdm);
fail1:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, iosize);
}

int
ips_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct ips_softc *sc = link->adapter_softc;
	struct scsi_inquiry_data *inq;
	struct scsi_read_cap_data *cap;
	struct scsi_sense_data *sns;
	int target = link->target;
	int s;

	if (target >= sc->sc_di.drivecount || link->lun != 0)
		goto error;

	switch (xs->cmd->opcode) {
	case READ_BIG:
	case READ_COMMAND:
	case WRITE_BIG:
	case WRITE_COMMAND:
		return (ips_scsi_io(xs));
	case INQUIRY:
		inq = (void *)xs->data;
		bzero(inq, sizeof(*inq));
		inq->device = T_DIRECT;
		inq->version = 2;
		inq->response_format = 2;
		inq->additional_length = 32;
		strlcpy(inq->vendor, "IBM", sizeof(inq->vendor));
		snprintf(inq->product, sizeof(inq->product),
		    "ServeRAID LD %02d", target);
		goto done;
	case READ_CAPACITY:
		cap = (void *)xs->data;
		bzero(cap, sizeof(*cap));
		_lto4b(sc->sc_di.drives[target].sector_count - 1, cap->addr);
		_lto4b(512, cap->length);
		goto done;
	case REQUEST_SENSE:
		sns = (void *)xs->data;
		bzero(sns, sizeof(*sns));
		sns->error_code = 0x70;
		sns->flags = SKEY_NO_SENSE;
		goto done;
	case SYNCHRONIZE_CACHE:
		ips_flushcache(sc);
		goto done;
	case PREVENT_ALLOW:
	case START_STOP:
	case TEST_UNIT_READY:
		return (COMPLETE);
	}

error:
	xs->error = XS_DRIVER_STUFFUP;
done:
	s = splbio();
	scsi_done(xs);
	splx(s);
	return (COMPLETE);
}

int
ips_scsi_io(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct ips_softc *sc = link->adapter_softc;
	struct scsi_rw *rw;
	struct scsi_rw_big *rwb;
	struct ccb *ccb;
	struct ips_cmd_io *cmd;
	u_int32_t blkno, blkcnt;
	int i, s;

	/* Pick up the first free ccb */
	s = splbio();
	ccb = TAILQ_FIRST(&sc->sc_ccbq);
	if (ccb != NULL)
		TAILQ_REMOVE(&sc->sc_ccbq, ccb, c_link);
	splx(s);
	if (ccb == NULL) {
		DPRINTF(IPS_D_ERR, ("%s: scsi_io, no free ccb\n",
		    sc->sc_dev.dv_xname));
		return (TRY_AGAIN_LATER);
	}
	DPRINTF(IPS_D_XFER, ("%s: scsi_io, ccb id %d\n", sc->sc_dev.dv_xname,
	    ccb->c_id));

	bus_dmamap_load(sc->sc_dmat, ccb->c_dmam, xs->data, xs->datalen, NULL,
	    BUS_DMA_NOWAIT);
	ccb->c_xfer = xs;

	if (xs->cmd->opcode == READ_COMMAND ||
	    xs->cmd->opcode == WRITE_COMMAND) {
		rw = (void *)xs->cmd;
		blkno = _3btol(rw->addr) & (SRW_TOPADDR << 16 | 0xffff);
		blkcnt = rw->length > 0 ? rw->length : 0x100;
	} else {
		rwb = (void *)xs->cmd;
		blkno = _4btol(rwb->addr);
		blkcnt = _2btol(rwb->length);
	}

	cmd = sc->sc_cmdm->dm_kva;
	bzero(cmd, sizeof(*cmd));
	cmd->command = (xs->flags & SCSI_DATA_IN) ? IPS_CMD_READ :
	    IPS_CMD_WRITE;
	cmd->id = ccb->c_id;
	cmd->drivenum = link->target;
	cmd->lba = blkno;
	cmd->length = blkcnt;
	if (ccb->c_dmam->dm_nsegs > 1) {
		cmd->command = (xs->flags & SCSI_DATA_IN) ? IPS_CMD_READ_SG :
		    IPS_CMD_WRITE_SG;
		cmd->segnum = ccb->c_dmam->dm_nsegs;

		for (i = 0; i < ccb->c_dmam->dm_nsegs; i++) {
			*(u_int32_t *)((u_int8_t *)sc->sc_cmdm->dm_kva + 24 +
			    i * 8) = ccb->c_dmam->dm_segs[i].ds_addr;
			*(u_int32_t *)((u_int8_t *)sc->sc_cmdm->dm_kva + 24 +
			    i * 8 + 4) = ccb->c_dmam->dm_segs[i].ds_len;
		}
		cmd->buffaddr = sc->sc_cmdm->dm_seg.ds_addr + 24;
		cmd->length = 512;
	} else {
		cmd->buffaddr = ccb->c_dmam->dm_segs[0].ds_addr;
	}

	timeout_add(&ccb->c_timo, hz);

	s = splbio();
	(*sc->sc_exec)(sc);
	ccb->c_flags |= CCB_F_RUN;
	splx(s);

	return (SUCCESSFULLY_QUEUED);
}

int
ips_scsi_ioctl(struct scsi_link *link, u_long cmd, caddr_t addr, int flags,
    struct proc *p)
{
	return (ENOTTY);
}

void
ips_scsi_minphys(struct buf *bp)
{
	minphys(bp);
}

void
ips_xfer_timeout(void *arg)
{
	struct ccb *ccb = arg;
	struct scsi_xfer *xs = ccb->c_xfer;
	struct ips_softc *sc = xs->sc_link->adapter_softc;
	int s;

	DPRINTF(IPS_D_ERR, ("%s: xfer timeout, ccb id %d\n",
	    sc->sc_dev.dv_xname, ccb->c_id));

	bus_dmamap_unload(sc->sc_dmat, ccb->c_dmam);
	xs->error = XS_TIMEOUT;
	s = splbio();
	scsi_done(xs);
	ccb->c_flags &= ~CCB_F_RUN;
	TAILQ_INSERT_TAIL(&sc->sc_ccbq, ccb, c_link);
	splx(s);
}

void
ips_flushcache(struct ips_softc *sc)
{
	struct ips_cmd_generic *cmd;

	cmd = sc->sc_cmdm->dm_kva;
	cmd->command = IPS_CMD_FLUSHCACHE;

	(*sc->sc_exec)(sc);
	DELAY(1000);
}

int
ips_getadapterinfo(struct ips_softc *sc, struct ips_adapterinfo *ai)
{
	struct dmamem *dm;
	struct ips_cmd_adapterinfo *cmd;

	if ((dm = ips_dmamem_alloc(sc->sc_dmat, sizeof(*ai))) == NULL)
		return (1);

	cmd = sc->sc_cmdm->dm_kva;
	bzero(cmd, sizeof(*cmd));
	cmd->command = IPS_CMD_ADAPTERINFO;
	cmd->buffaddr = dm->dm_seg.ds_addr;

	(*sc->sc_exec)(sc);
	DELAY(1000);
	bcopy(dm->dm_kva, ai, sizeof(*ai));
	ips_dmamem_free(dm);

	return (0);
}

int
ips_getdriveinfo(struct ips_softc *sc, struct ips_driveinfo *di)
{
	struct dmamem *dm;
	struct ips_cmd_driveinfo *cmd;

	if ((dm = ips_dmamem_alloc(sc->sc_dmat, sizeof(*di))) == NULL)
		return (1);

	cmd = sc->sc_cmdm->dm_kva;
	bzero(cmd, sizeof(*cmd));
	cmd->command = IPS_CMD_DRIVEINFO;
	cmd->buffaddr = dm->dm_seg.ds_addr;

	(*sc->sc_exec)(sc);
	DELAY(1000);
	bcopy(dm->dm_kva, di, sizeof(*di));
	ips_dmamem_free(dm);

	return (0);
}

void
ips_copperhead_exec(struct ips_softc *sc)
{
}

void
ips_copperhead_inten(struct ips_softc *sc)
{
}

int
ips_copperhead_intr(void *arg)
{
	return (0);
}

void
ips_morpheus_exec(struct ips_softc *sc)
{
	IPS_WRITE_4(sc, IPS_MORPHEUS_IQPR, sc->sc_cmdm->dm_seg.ds_addr);
}

void
ips_morpheus_inten(struct ips_softc *sc)
{
	u_int32_t reg;

	reg = IPS_READ_4(sc, IPS_MORPHEUS_OIMR);
	reg &= ~0x08;
	IPS_WRITE_4(sc, IPS_MORPHEUS_OIMR, reg);
}

int
ips_morpheus_intr(void *arg)
{
	struct ips_softc *sc = arg;
	struct ccb *ccb;
	struct scsi_xfer *xs;
	u_int32_t oisr, oqpr;
	int id, s, rv = 0;

	oisr = IPS_READ_4(sc, IPS_MORPHEUS_OISR);
	DPRINTF(IPS_D_INTR, ("%s: intr, OISR 0x%08x\n",
	    sc->sc_dev.dv_xname, oisr));

	if (!(oisr & IPS_MORPHEUS_OISR_CMD))
		return (0);

	while ((oqpr = IPS_READ_4(sc, IPS_MORPHEUS_OQPR)) != 0xffffffff) {
		DPRINTF(IPS_D_INTR, ("OQPR 0x%08x\n", oqpr));

		id = (oqpr >> 8) & 0xff;
		if (id >= sc->sc_ai.max_concurrent_cmds) {
			DPRINTF(IPS_D_ERR, ("%s: intr, bogus id %d",
			    sc->sc_dev.dv_xname, id));
			DPRINTF(IPS_D_ERR, (", OISR 0x%08x, OQPR 0x%08x\n",
			    oisr, oqpr));
			continue;
		}

		ccb = &sc->sc_ccb[id];
		if (!(ccb->c_flags & CCB_F_RUN)) {
			DPRINTF(IPS_D_ERR, ("%s: intr, ccb id %d not run",
			    sc->sc_dev.dv_xname, id));
			DPRINTF(IPS_D_ERR, (", OISR 0x%08x, OQPR 0x%08x\n",
			    oisr, oqpr));
			continue;
		}

		rv = 1;
		timeout_del(&ccb->c_timo);
		bus_dmamap_unload(sc->sc_dmat, ccb->c_dmam);
		xs = ccb->c_xfer;
		xs->resid = 0;
		xs->flags |= ITSDONE;
		s = splbio();
		scsi_done(xs);
		ccb->c_flags &= ~CCB_F_RUN;
		TAILQ_INSERT_TAIL(&sc->sc_ccbq, ccb, c_link);
		splx(s);
	}

	return (rv);
}

struct ccb *
ips_ccb_alloc(bus_dma_tag_t dmat, int n)
{
	struct ccb *ccb;
	int i;

	if ((ccb = malloc(n * sizeof(*ccb), M_DEVBUF, M_NOWAIT)) == NULL)
		return (NULL);
	bzero(ccb, n * sizeof(*ccb));

	for (i = 0; i < n; i++) {
		ccb[i].c_id = i;
		if (bus_dmamap_create(dmat, IPS_MAXFER, IPS_MAXSGS,
		    IPS_MAXFER, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &ccb[i].c_dmam))
			goto fail;
		timeout_set(&ccb[i].c_timo, ips_xfer_timeout, &ccb[i]);
	}

	return (ccb);
fail:
	for (; i > 0; i--)
		bus_dmamap_destroy(dmat, ccb[i - 1].c_dmam);
	free(ccb, M_DEVBUF);
	return (NULL);
}

void
ips_ccb_free(struct ccb *ccb, bus_dma_tag_t dmat, int n)
{
	int i;

	for (i = 0; i < n; i++)
		bus_dmamap_destroy(dmat, ccb[i - 1].c_dmam);
	free(ccb, M_DEVBUF);
}

struct dmamem *
ips_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size)
{
	struct dmamem *dm;
	int nsegs;

	if ((dm = malloc(sizeof(*dm), M_DEVBUF, M_NOWAIT)) == NULL)
		return (NULL);

	dm->dm_tag = tag;
	dm->dm_size = size;

	if (bus_dmamap_create(tag, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &dm->dm_map))
		goto fail1;
	if (bus_dmamem_alloc(tag, size, 0, 0, &dm->dm_seg, 1, &nsegs,
	    BUS_DMA_NOWAIT))
		goto fail2;
	if (bus_dmamem_map(tag, &dm->dm_seg, 1, size, (caddr_t *)&dm->dm_kva,
	    BUS_DMA_NOWAIT))
		goto fail3;
	bzero(dm->dm_kva, size);
	if (bus_dmamap_load(tag, dm->dm_map, dm->dm_kva, size, NULL,
	    BUS_DMA_NOWAIT))
		goto fail4;

	return (dm);

fail4:
	bus_dmamem_unmap(tag, dm->dm_kva, size);
fail3:
	bus_dmamem_free(tag, &dm->dm_seg, 1);
fail2:
	bus_dmamap_destroy(tag, dm->dm_map);
fail1:
	free(dm, M_DEVBUF);
	return (NULL);
}

void
ips_dmamem_free(struct dmamem *dm)
{
	bus_dmamap_unload(dm->dm_tag, dm->dm_map);
	bus_dmamem_unmap(dm->dm_tag, dm->dm_kva, dm->dm_size);
	bus_dmamem_free(dm->dm_tag, &dm->dm_seg, 1);
	bus_dmamap_destroy(dm->dm_tag, dm->dm_map);
	free(dm, M_DEVBUF);
}
