/*	$OpenBSD: ips.c,v 1.17 2007/05/27 19:21:09 grange Exp $	*/

/*
 * Copyright (c) 2006, 2007 Alexander Yurchenko <grange@openbsd.org>
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
 * IBM (Adaptec) ServeRAID controller driver.
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

#define IPS_DEBUG	/* XXX: remove when driver becomes stable */

/* Debug levels */
#define IPS_D_ERR	0x0001	/* errors */
#define IPS_D_INFO	0x0002	/* information */
#define IPS_D_XFER	0x0004	/* transfers */
#define IPS_D_INTR	0x0008	/* interrupts */

#ifdef IPS_DEBUG
#define DPRINTF(a, b)	do { if (ips_debug & (a)) printf b; } while (0)
int ips_debug = IPS_D_ERR;
#else
#define DPRINTF(a, b)
#endif

#define IPS_MAXDRIVES		8
#define IPS_MAXCHANS		4
#define IPS_MAXTARGETS		15

#define IPS_MAXFER		(64 * 1024)
#define IPS_MAXSGS		16
#define IPS_MAXCMDSZ		(IPS_CMDSZ + IPS_MAXSGS * IPS_SGSZ)

#define IPS_CMDSZ		sizeof(struct ips_cmd)
#define IPS_SGSZ		sizeof(struct ips_sg)
#define IPS_SECSZ		512

/* Command codes */
#define IPS_CMD_READ		0x02
#define IPS_CMD_WRITE		0x03
#define IPS_CMD_DCDB		0x04
#define IPS_CMD_GETADAPTERINFO	0x05
#define IPS_CMD_FLUSH		0x0a
#define IPS_CMD_ERRORTABLE	0x17
#define IPS_CMD_GETDRIVEINFO	0x19
#define IPS_CMD_RESETCHAN	0x1a
#define IPS_CMD_DOWNLOAD	0x20
#define IPS_CMD_RWBIOSFW	0x22
#define IPS_CMD_READCONF	0x38
#define IPS_CMD_GETSUBSYS	0x40
#define IPS_CMD_CONFIGSYNC	0x58
#define IPS_CMD_READ_SG		0x82
#define IPS_CMD_WRITE_SG	0x83
#define IPS_CMD_DCDB_SG		0x84
#define IPS_CMD_EXT_DCDB	0x95
#define IPS_CMD_EXT_DCDB_SG	0x96
#define IPS_CMD_RWNVRAMPAGE	0xbc
#define IPS_CMD_GETVERINFO	0xc6
#define IPS_CMD_FFDC		0xd7
#define IPS_CMD_SG		0x80

/* Register definitions */
#define IPS_REG_CCSA		0x10	/* command channel system address */
#define IPS_REG_CCC		0x14	/* command channel control */
#define IPS_REG_CCC_SEM			0x0008	/* semaphore */
#define IPS_REG_CCC_START		0x101a	/* start command */
#define IPS_REG_OIS		0x30	/* outbound interrupt status */
#define IPS_REG_OIS_PEND		0x0008	/* interrupt is pending */
#define IPS_REG_OIM		0x34	/* outbound interrupt mask */
#define IPS_REG_OIM_DS			0x0008	/* disable interrupts */
#define IPS_REG_IQP		0x40	/* inbound queue port */
#define IPS_REG_OQP		0x44	/* outbound queue port */

#define IPS_REG_STAT_ID(x)	(((x) >> 8) & 0xff)

/* Command frame */
struct ips_cmd {
	u_int8_t	code;
	u_int8_t	id;
	u_int8_t	drive;
	u_int8_t	sgcnt;
	u_int32_t	lba;
	u_int32_t	sgaddr;
	u_int16_t	seccnt;
	u_int8_t	seg4g;
	u_int8_t	esg;
	u_int32_t	ccsar;
	u_int32_t	cccr;
};

/* Scatter-gather array element */
struct ips_sg {
	u_int32_t	addr;
	u_int32_t	size;
};

/* Data frames */
struct ips_adapterinfo {
	u_int8_t	drivecnt;
	u_int8_t	miscflag;
	u_int8_t	sltflag;
	u_int8_t	bstflag;
	u_int8_t	pwrchgcnt;
	u_int8_t	wrongaddrcnt;
	u_int8_t	unidentcnt;
	u_int8_t	nvramdevchgcnt;
	u_int8_t	codeblkver[8];
	u_int8_t	bootblkver[8];
	u_int32_t	drivesize[IPS_MAXDRIVES];
	u_int8_t	cmdcnt;
	u_int8_t	maxphysdevs;
	u_int16_t	flashrepgmcnt;
	u_int8_t	defunctdiskcnt;
	u_int8_t	rebuildflag;
	u_int8_t	offdrivecnt;
	u_int8_t	critdrivecnt;
	u_int16_t	confupdcnt;
	u_int8_t	blkflag;
	u_int8_t	__reserved;
	u_int16_t	deaddisk[IPS_MAXCHANS * (IPS_MAXTARGETS + 1)];
};

struct ips_driveinfo {
	u_int8_t	drivecnt;
	u_int8_t	__reserved[3];
	struct ips_drive {
		u_int8_t	id;
		u_int8_t	__reserved;
		u_int8_t	raid;
		u_int8_t	state;
		u_int32_t	seccnt;
	}		drive[IPS_MAXDRIVES];
};

/* Command control block */
struct ips_ccb {
	int			c_id;		/* command id */
	int			c_flags;	/* flags */
#define IPS_CCB_READ	0x0001
#define IPS_CCB_WRITE	0x0002
#define IPS_CCB_POLL	0x0004
#define IPS_CCB_RUN	0x0008

	bus_dmamap_t		c_dmam;		/* data buffer DMA map */
	struct scsi_xfer *	c_xfer;		/* corresponding SCSI xfer */

	TAILQ_ENTRY(ips_ccb)	c_link;		/* queue link */
};

/* CCB queue */
TAILQ_HEAD(ips_ccbq, ips_ccb);

/* DMA-able chunk of memory */
struct dmamem {
	bus_dma_tag_t		dm_tag;
	bus_dmamap_t		dm_map;
	bus_dma_segment_t	dm_seg;
	bus_size_t		dm_size;
	void *			dm_vaddr;
#define dm_paddr dm_seg.ds_addr
};

struct ips_softc {
	struct device		sc_dev;

	struct scsi_link	sc_scsi_link;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_dma_tag_t		sc_dmat;

	const struct ips_chipset *sc_chip;

	struct ips_driveinfo	sc_di;
	int			sc_nunits;

	struct dmamem		sc_cmdm;

	struct ips_ccb *	sc_ccb;
	int			sc_nccbs;
	struct ips_ccbq		sc_ccbq_free;
	struct ips_ccbq		sc_ccbq_run;
};

int	ips_match(struct device *, void *, void *);
void	ips_attach(struct device *, struct device *, void *);

int	ips_scsi_cmd(struct scsi_xfer *);

int	ips_cmd(struct ips_softc *, int, int, u_int32_t, void *, size_t, int,
	    struct scsi_xfer *);
int	ips_poll(struct ips_softc *, struct ips_ccb *);
void	ips_done(struct ips_softc *, struct ips_ccb *);
int	ips_intr(void *);

int	ips_getadapterinfo(struct ips_softc *, struct ips_adapterinfo *);
int	ips_getdriveinfo(struct ips_softc *, struct ips_driveinfo *);
int	ips_flush(struct ips_softc *);

void	ips_copperhead_exec(struct ips_softc *);
void	ips_copperhead_init(struct ips_softc *);
void	ips_copperhead_intren(struct ips_softc *);
int	ips_copperhead_isintr(struct ips_softc *);
int	ips_copperhead_reset(struct ips_softc *);
u_int32_t ips_copperhead_status(struct ips_softc *);

void	ips_morpheus_exec(struct ips_softc *);
void	ips_morpheus_init(struct ips_softc *);
void	ips_morpheus_intren(struct ips_softc *);
int	ips_morpheus_isintr(struct ips_softc *);
int	ips_morpheus_reset(struct ips_softc *);
u_int32_t ips_morpheus_status(struct ips_softc *);

struct ips_ccb *ips_ccb_alloc(bus_dma_tag_t, int);
void	ips_ccb_free(struct ips_ccb *, bus_dma_tag_t, int);
struct ips_ccb *ips_ccb_get(struct ips_softc *);
void	ips_ccb_put(struct ips_softc *, struct ips_ccb *);

int	ips_dmamem_alloc(struct dmamem *, bus_dma_tag_t, bus_size_t);
void	ips_dmamem_free(struct dmamem *);

struct cfattach ips_ca = {
	sizeof(struct ips_softc),
	ips_match,
	ips_attach
};

struct cfdriver ips_cd = {
	NULL, "ips", DV_DULL
};

static struct scsi_adapter ips_scsi_adapter = {
	ips_scsi_cmd,
	minphys,
	NULL,
	NULL,
	NULL
};

static struct scsi_device ips_scsi_device = {
	NULL,
	NULL,
	NULL,
	NULL
};

static const struct pci_matchid ips_ids[] = {
	{ PCI_VENDOR_IBM,	PCI_PRODUCT_IBM_SERVERAID },
	{ PCI_VENDOR_IBM,	PCI_PRODUCT_IBM_SERVERAID2 },
	{ PCI_VENDOR_ADP2,	PCI_PRODUCT_ADP2_SERVERAID }
};

static const struct ips_chipset {
	const char *	ic_name;
	int		ic_bar;

	void		(*ic_exec)(struct ips_softc *);
	void		(*ic_init)(struct ips_softc *);
	void		(*ic_intren)(struct ips_softc *);
	int		(*ic_isintr)(struct ips_softc *);
	int		(*ic_reset)(struct ips_softc *);
	u_int32_t	(*ic_status)(struct ips_softc *);
} ips_chips[] = {
	{
		"Copperhead",
		0x14,
		ips_copperhead_exec,
		ips_copperhead_init,
		ips_copperhead_intren,
		ips_copperhead_isintr,
		ips_copperhead_reset,
		ips_copperhead_status
	},
	{
		"Morpheus",
		0x10,
		ips_morpheus_exec,
		ips_morpheus_init,
		ips_morpheus_intren,
		ips_morpheus_isintr,
		ips_morpheus_reset,
		ips_morpheus_status
	}
};

enum {
	IPS_CHIP_COPPERHEAD = 0,
	IPS_CHIP_MORPHEUS
};

#define ips_exec(s)	(s)->sc_chip->ic_exec((s))
#define ips_init(s)	(s)->sc_chip->ic_init((s))
#define ips_intren(s)	(s)->sc_chip->ic_intren((s))
#define ips_isintr(s)	(s)->sc_chip->ic_isintr((s))
#define ips_reset(s)	(s)->sc_chip->ic_reset((s))
#define ips_status(s)	(s)->sc_chip->ic_status((s))

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
	struct ips_ccb ccb0;
	struct scsibus_attach_args saa;
	struct ips_adapterinfo ai;
	pcireg_t maptype;
	bus_size_t iosize;
	pci_intr_handle_t ih;
	const char *intrstr;
	int i;

	sc->sc_dmat = pa->pa_dmat;

	/* Identify chipset */
	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_IBM_SERVERAID:
		sc->sc_chip = &ips_chips[IPS_CHIP_COPPERHEAD];
		break;
	case PCI_PRODUCT_IBM_SERVERAID2:
	case PCI_PRODUCT_ADP2_SERVERAID:
		sc->sc_chip = &ips_chips[IPS_CHIP_MORPHEUS];
		break;
	default:
		printf(": unsupported chipset\n");
		return;
	}

	/* Map registers */
	maptype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, sc->sc_chip->ic_bar);
	if (pci_mapreg_map(pa, sc->sc_chip->ic_bar, maptype, 0, &sc->sc_iot,
	    &sc->sc_ioh, NULL, &iosize, 0)) {
		printf(": can't map registers\n");
		return;
	}

	/* Initialize hardware */
	ips_init(sc);

	/* Allocate command buffer */
	if (ips_dmamem_alloc(&sc->sc_cmdm, sc->sc_dmat, IPS_MAXCMDSZ)) {
		printf(": can't allocate command buffer\n");
		goto fail1;
	}

	/* Bootstrap CCB queue */
	sc->sc_nccbs = 1;
	sc->sc_ccb = &ccb0;
	bzero(&ccb0, sizeof(ccb0));
	if (bus_dmamap_create(sc->sc_dmat, IPS_MAXFER, IPS_MAXSGS,
	    IPS_MAXFER, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &ccb0.c_dmam)) {
		printf(": can't bootstrap CCB queue\n");
		goto fail2;
	}
	TAILQ_INIT(&sc->sc_ccbq_free);
	TAILQ_INIT(&sc->sc_ccbq_run);
	TAILQ_INSERT_TAIL(&sc->sc_ccbq_free, &ccb0, c_link);

	/* Get adapter info */
	if (ips_getadapterinfo(sc, &ai)) {
		printf(": can't get adapter info\n");
		bus_dmamap_destroy(sc->sc_dmat, ccb0.c_dmam);
		goto fail2;
	}

	/* Get logical drives info */
	if (ips_getdriveinfo(sc, &sc->sc_di)) {
		printf(": can't get logical drives info\n");
		bus_dmamap_destroy(sc->sc_dmat, ccb0.c_dmam);
		goto fail2;
	}
	sc->sc_nunits = sc->sc_di.drivecnt;

	bus_dmamap_destroy(sc->sc_dmat, ccb0.c_dmam);

	/* Initialize CCB queue */
	sc->sc_nccbs = ai.cmdcnt;
	if ((sc->sc_ccb = ips_ccb_alloc(sc->sc_dmat, sc->sc_nccbs)) == NULL) {
		printf(": can't allocate CCB queue\n");
		goto fail2;
	}
	TAILQ_INIT(&sc->sc_ccbq_free);
	TAILQ_INIT(&sc->sc_ccbq_run);
	for (i = 0; i < sc->sc_nccbs; i++)
		TAILQ_INSERT_TAIL(&sc->sc_ccbq_free,
		    &sc->sc_ccb[i], c_link);

	/* Install interrupt handler */
	if (pci_intr_map(pa, &ih)) {
		printf(": can't map interrupt\n");
		goto fail3;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	if (pci_intr_establish(pa->pa_pc, ih, IPL_BIO, ips_intr, sc,
	    sc->sc_dev.dv_xname) == NULL) {
		printf(": can't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail3;
	}
	printf(": %s\n", intrstr);

	/* Display adapter info */
	printf("%s", sc->sc_dev.dv_xname);
	printf(": %s", sc->sc_chip->ic_name);
	printf(", firmware %c%c%c%c%c%c%c",
	    ai.codeblkver[0], ai.codeblkver[1], ai.codeblkver[2],
	    ai.codeblkver[3], ai.codeblkver[4], ai.codeblkver[5],
	    ai.codeblkver[6]);
	printf(", bootblock %c%c%c%c%c%c%c",
	    ai.bootblkver[0], ai.bootblkver[1], ai.bootblkver[2],
	    ai.bootblkver[3], ai.bootblkver[4], ai.bootblkver[5],
	    ai.bootblkver[6]);
	printf(", %d CCBs, %d units", sc->sc_nccbs, sc->sc_nunits);
	printf("\n");

	/* Attach SCSI bus */
	if (sc->sc_nunits > 0)
		sc->sc_scsi_link.openings = sc->sc_nccbs / sc->sc_nunits;
	sc->sc_scsi_link.openings = 1; /* XXX */
	sc->sc_scsi_link.adapter_target = sc->sc_nunits;
	sc->sc_scsi_link.adapter_buswidth = sc->sc_nunits;
	sc->sc_scsi_link.device = &ips_scsi_device;
	sc->sc_scsi_link.adapter = &ips_scsi_adapter;
	sc->sc_scsi_link.adapter_softc = sc;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_scsi_link;
	config_found(self, &saa, scsiprint);

	/* Enable interrupts */
	ips_intren(sc);

	return;
fail3:
	ips_ccb_free(sc->sc_ccb, sc->sc_dmat, sc->sc_nccbs);
fail2:
	ips_dmamem_free(&sc->sc_cmdm);
fail1:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, iosize);
}

int
ips_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct ips_softc *sc = link->adapter_softc;
	struct ips_drive *drive;
	struct scsi_inquiry_data *id;
	struct scsi_read_cap_data *rcd;
	struct scsi_sense_data *sd;
	struct scsi_rw *rw;
	struct scsi_rw_big *rwb;
	int target = link->target;
	u_int32_t blkno, blkcnt;
	int cmd, error, flags, s;

	if (target >= sc->sc_nunits || link->lun != 0) {
		DPRINTF(IPS_D_INFO, ("%s: invalid scsi command, "
		    "target %d, lun %d\n", sc->sc_dev.dv_xname,
		    target, link->lun));
		xs->error = XS_DRIVER_STUFFUP;
		return (COMPLETE);
	}

	s = splbio();
	drive = &sc->sc_di.drive[target];
	xs->error = XS_NOERROR;

	/* Fake SCSI commands */
	switch (xs->cmd->opcode) {
	case READ_BIG:
	case READ_COMMAND:
	case WRITE_BIG:
	case WRITE_COMMAND:
		if (xs->cmdlen == sizeof(struct scsi_rw)) {
			rw = (void *)xs->cmd;
			blkno = _3btol(rw->addr) &
			    (SRW_TOPADDR << 16 | 0xffff);
			blkcnt = rw->length ? rw->length : 0x100;
		} else {
			rwb = (void *)xs->cmd;
			blkno = _4btol(rwb->addr);
			blkcnt = _2btol(rwb->length);
		}

		if (blkno >= letoh32(drive->seccnt) || blkno + blkcnt >
		    letoh32(drive->seccnt)) {
			DPRINTF(IPS_D_ERR, ("%s: invalid scsi command, "
			    "blkno %u, blkcnt %u\n", sc->sc_dev.dv_xname,
			    blkno, blkcnt));
			xs->error = XS_DRIVER_STUFFUP;
			scsi_done(xs);
			break;
		}

		if (xs->flags & SCSI_DATA_IN) {
			cmd = IPS_CMD_READ;
			flags = IPS_CCB_READ;
		} else {
			cmd = IPS_CMD_WRITE;
			flags = IPS_CCB_WRITE;
		}
		if (xs->flags & SCSI_POLL)
			flags |= IPS_CCB_POLL;

		if ((error = ips_cmd(sc, cmd, target, blkno, xs->data,
		    blkcnt * IPS_SECSZ, flags, xs))) {
			if (error == ENOMEM) {
				splx(s);
				return (NO_CCB);
			} else if (flags & IPS_CCB_POLL) {
				splx(s);
				return (TRY_AGAIN_LATER);
			} else {
				xs->error = XS_DRIVER_STUFFUP;
				scsi_done(xs);
				break;
			}
		}

		splx(s);
		if (flags & IPS_CCB_POLL)
			return (COMPLETE);
		else
			return (SUCCESSFULLY_QUEUED);
	case INQUIRY:
		id = (void *)xs->data;
		bzero(id, sizeof(*id));
		id->device = T_DIRECT;
		id->version = 2;
		id->response_format = 2;
		id->additional_length = 32;
		strlcpy(id->vendor, "IBM     ", sizeof(id->vendor));
		snprintf(id->product, sizeof(id->product),
		    "ServeRAID RAID%d #%02d", drive->raid, target);
		strlcpy(id->revision, "   ", sizeof(id->revision));
		break;
	case READ_CAPACITY:
		rcd = (void *)xs->data;
		bzero(rcd, sizeof(*rcd));
		_lto4b(letoh32(drive->seccnt) - 1, rcd->addr);
		_lto4b(IPS_SECSZ, rcd->length);
		break;
	case REQUEST_SENSE:
		sd = (void *)xs->data;
		bzero(sd, sizeof(*sd));
		sd->error_code = SSD_ERRCODE_CURRENT;
		sd->flags = SKEY_NO_SENSE;
		break;
	case SYNCHRONIZE_CACHE:
		if (ips_flush(sc))
			xs->error = XS_DRIVER_STUFFUP;
		break;
	case PREVENT_ALLOW:
	case START_STOP:
	case TEST_UNIT_READY:
		break;
	default:
		DPRINTF(IPS_D_INFO, ("%s: unsupported scsi command 0x%02x\n",
		    sc->sc_dev.dv_xname, xs->cmd->opcode));
		xs->error = XS_DRIVER_STUFFUP;
	}
	splx(s);

	return (COMPLETE);
}

int
ips_cmd(struct ips_softc *sc, int code, int drive, u_int32_t lba, void *data,
    size_t size, int flags, struct scsi_xfer *xs)
{
	struct ips_cmd *cmd = sc->sc_cmdm.dm_vaddr;
	struct ips_sg *sg = (void *)(cmd + 1);
	struct ips_ccb *ccb;
	int nsegs, i, error = 0;

	DPRINTF(IPS_D_XFER, ("%s: cmd code 0x%02x, drive %d, lba %u, "
	    "size %lu, flags 0x%02x\n", sc->sc_dev.dv_xname, code, drive, lba,
	    (u_long)size, flags));

	/* Grab free CCB */
	if ((ccb = ips_ccb_get(sc)) == NULL) {
		DPRINTF(IPS_D_ERR, ("%s: no free CCB\n", sc->sc_dev.dv_xname));
		return (ENOMEM);
	}

	ccb->c_flags = flags;
	ccb->c_xfer = xs;

	/* Fill in command frame */
	cmd->code = code;
	cmd->id = ccb->c_id;
	cmd->drive = drive;
	cmd->lba = htole32(lba);
	cmd->seccnt = htole16(howmany(size, IPS_SECSZ));

	if (size > 0) {
		/* Map data buffer into DMA segments */
		if (bus_dmamap_load(sc->sc_dmat, ccb->c_dmam, data, size,
		    NULL, BUS_DMA_NOWAIT)) {
			printf("%s: can't load DMA map\n",
			    sc->sc_dev.dv_xname);
			return (1);	/* XXX: return code */
		}
		bus_dmamap_sync(sc->sc_dmat, ccb->c_dmam, 0, size,
		    flags & IPS_CCB_READ ? BUS_DMASYNC_PREREAD :
		    BUS_DMASYNC_PREWRITE);

		if ((nsegs = ccb->c_dmam->dm_nsegs) > IPS_MAXSGS) {
			printf("%s: too many DMA segments\n",
			    sc->sc_dev.dv_xname);
			return (1);	/* XXX: return code */
		}

		if (nsegs > 1) {
			cmd->code |= IPS_CMD_SG;
			cmd->sgcnt = nsegs;
			cmd->sgaddr = htole32(sc->sc_cmdm.dm_paddr + IPS_CMDSZ);

			/* Fill in scatter-gather array */
			for (i = 0; i < nsegs; i++) {
				sg[i].addr =
				    htole32(ccb->c_dmam->dm_segs[i].ds_addr);
				sg[i].size =
				    htole32(ccb->c_dmam->dm_segs[i].ds_len);
			}
		} else {
			cmd->sgcnt = 0;
			cmd->sgaddr = htole32(ccb->c_dmam->dm_segs[0].ds_addr);
		}
	}

	/* Pass command to hardware */
	ccb->c_flags |= IPS_CCB_RUN;
	TAILQ_INSERT_TAIL(&sc->sc_ccbq_run, ccb, c_link);
	ips_exec(sc);

	if (flags & IPS_CCB_POLL)
		/* Wait for command to complete */
		error = ips_poll(sc, ccb);

	return (error);
}

int
ips_poll(struct ips_softc *sc, struct ips_ccb *c)
{
	struct ips_ccb *ccb = NULL;
	u_int32_t status;
	int id, timeout;

	while (ccb != c) {
		for (timeout = 10; timeout-- > 0; delay(100)) {
			if ((status = ips_status(sc)) == 0xffffffff)
				continue;
			id = IPS_REG_STAT_ID(status);
			if (id >= sc->sc_nccbs) {
				DPRINTF(IPS_D_ERR, ("%s: invalid command %d\n",
				    sc->sc_dev.dv_xname, id));
				continue;
			}
			break;
		}
		if (timeout == 0) {
			printf("%s: poll timeout\n", sc->sc_dev.dv_xname);
			return (EBUSY);
		}
		ccb = &sc->sc_ccb[id];
		ips_done(sc, ccb);
	}

	return (0);
}

void
ips_done(struct ips_softc *sc, struct ips_ccb *ccb)
{
	struct scsi_xfer *xs = ccb->c_xfer;
	int flags = ccb->c_flags;

	if ((flags & IPS_CCB_RUN) == 0) {
		printf("%s: command %d not run\n", sc->sc_dev.dv_xname,
		    ccb->c_id);
		if (xs != NULL) {
			xs->error = XS_DRIVER_STUFFUP;
			scsi_done(xs);
		}
		return;
	}

	if (flags & (IPS_CCB_READ | IPS_CCB_WRITE)) {
		bus_dmamap_sync(sc->sc_dmat, ccb->c_dmam, 0,
		    ccb->c_dmam->dm_mapsize, flags & IPS_CCB_READ ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ccb->c_dmam);
	}

	if (xs != NULL) {
		xs->resid = 0;
		xs->flags |= ITSDONE;
		scsi_done(xs);
	}

	/* Release CCB */
	TAILQ_REMOVE(&sc->sc_ccbq_run, ccb, c_link);
	ips_ccb_put(sc, ccb);
}

int
ips_intr(void *arg)
{
	struct ips_softc *sc = arg;
	struct ips_ccb *ccb;
	u_int32_t status;
	int id;

	if (!ips_isintr(sc))
		return (0);

	/* Process completed commands */
	while ((status = ips_status(sc)) != 0xffffffff) {
		DPRINTF(IPS_D_INTR, ("%s: intr status 0x%08x\n",
		    sc->sc_dev.dv_xname, status));

		id = IPS_REG_STAT_ID(status);
		if (id >= sc->sc_nccbs) {
			DPRINTF(IPS_D_ERR, ("%s: invalid command %d\n",
			    sc->sc_dev.dv_xname, id));
			continue;
		}
		ccb = &sc->sc_ccb[id];
		ips_done(sc, ccb);
	}

	return (1);
}

int
ips_getadapterinfo(struct ips_softc *sc, struct ips_adapterinfo *ai)
{
	return (ips_cmd(sc, IPS_CMD_GETADAPTERINFO, 0, 0, ai, sizeof(*ai),
	    IPS_CCB_READ | IPS_CCB_POLL, NULL));
}

int
ips_getdriveinfo(struct ips_softc *sc, struct ips_driveinfo *di)
{
	return (ips_cmd(sc, IPS_CMD_GETDRIVEINFO, 0, 0, di, sizeof(*di),
	    IPS_CCB_READ | IPS_CCB_POLL, NULL));
}

int
ips_flush(struct ips_softc *sc)
{
	return (ips_cmd(sc, IPS_CMD_FLUSH, 0, 0, NULL, 0, IPS_CCB_POLL, NULL));
}

void
ips_copperhead_exec(struct ips_softc *sc)
{
	/* XXX: not implemented */
}

void
ips_copperhead_init(struct ips_softc *sc)
{
	/* XXX: not implemented */
}

void
ips_copperhead_intren(struct ips_softc *sc)
{
	/* XXX: not implemented */
}

int
ips_copperhead_isintr(struct ips_softc *sc)
{
	/* XXX: not implemented */
	return (0);
}

int
ips_copperhead_reset(struct ips_softc *sc)
{
	/* XXX: not implemented */
	return (0);
}

u_int32_t
ips_copperhead_status(struct ips_softc *sc)
{
	/* XXX: not implemented */
	return (0);
}

void
ips_morpheus_exec(struct ips_softc *sc)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, IPS_REG_IQP,
	    sc->sc_cmdm.dm_paddr);
}

void
ips_morpheus_init(struct ips_softc *sc)
{
	/* XXX: not implemented */
}

void
ips_morpheus_intren(struct ips_softc *sc)
{
	u_int32_t reg;

	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, IPS_REG_OIM);
	reg &= ~IPS_REG_OIM_DS;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, IPS_REG_OIM, reg);
}

int
ips_morpheus_isintr(struct ips_softc *sc)
{
	u_int32_t reg;

	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, IPS_REG_OIS);
	DPRINTF(IPS_D_INTR, ("%s: isintr 0x%08x\n", sc->sc_dev.dv_xname, reg));

	return (reg & IPS_REG_OIS_PEND);
}

int
ips_morpheus_reset(struct ips_softc *sc)
{
	/* XXX: not implemented */
	return (0);
}

u_int32_t
ips_morpheus_status(struct ips_softc *sc)
{
	u_int32_t reg;

	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, IPS_REG_OQP);
	DPRINTF(IPS_D_INTR, ("%s: status 0x%08x\n", sc->sc_dev.dv_xname, reg));

	return (reg);
}

struct ips_ccb *
ips_ccb_alloc(bus_dma_tag_t dmat, int n)
{
	struct ips_ccb *ccb;
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
	}

	return (ccb);
fail:
	for (; i > 0; i--)
		bus_dmamap_destroy(dmat, ccb[i - 1].c_dmam);
	free(ccb, M_DEVBUF);
	return (NULL);
}

void
ips_ccb_free(struct ips_ccb *ccb, bus_dma_tag_t dmat, int n)
{
	int i;

	for (i = 0; i < n; i++)
		bus_dmamap_destroy(dmat, ccb[i - 1].c_dmam);
	free(ccb, M_DEVBUF);
}

struct ips_ccb *
ips_ccb_get(struct ips_softc *sc)
{
	struct ips_ccb *ccb;

	if ((ccb = TAILQ_FIRST(&sc->sc_ccbq_free)) != NULL)
		TAILQ_REMOVE(&sc->sc_ccbq_free, ccb, c_link);

	return (ccb);
}

void
ips_ccb_put(struct ips_softc *sc, struct ips_ccb *ccb)
{
	ccb->c_flags = 0;
	ccb->c_xfer = NULL;
	TAILQ_INSERT_TAIL(&sc->sc_ccbq_free, ccb, c_link);
}

int
ips_dmamem_alloc(struct dmamem *dm, bus_dma_tag_t tag, bus_size_t size)
{
	int nsegs;

	dm->dm_tag = tag;
	dm->dm_size = size;

	if (bus_dmamap_create(tag, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &dm->dm_map))
		return (1);
	if (bus_dmamem_alloc(tag, size, 0, 0, &dm->dm_seg, 1, &nsegs,
	    BUS_DMA_NOWAIT))
		goto fail1;
	if (bus_dmamem_map(tag, &dm->dm_seg, 1, size, (caddr_t *)&dm->dm_vaddr,
	    BUS_DMA_NOWAIT))
		goto fail2;
	if (bus_dmamap_load(tag, dm->dm_map, dm->dm_vaddr, size, NULL,
	    BUS_DMA_NOWAIT))
		goto fail3;

	return (0);

fail3:
	bus_dmamem_unmap(tag, dm->dm_vaddr, size);
fail2:
	bus_dmamem_free(tag, &dm->dm_seg, 1);
fail1:
	bus_dmamap_destroy(tag, dm->dm_map);
	return (1);
}

void
ips_dmamem_free(struct dmamem *dm)
{
	bus_dmamap_unload(dm->dm_tag, dm->dm_map);
	bus_dmamem_unmap(dm->dm_tag, dm->dm_vaddr, dm->dm_size);
	bus_dmamem_free(dm->dm_tag, &dm->dm_seg, 1);
	bus_dmamap_destroy(dm->dm_tag, dm->dm_map);
}
