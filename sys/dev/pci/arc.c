/*	$OpenBSD: arc.c,v 1.8 2006/08/05 00:55:35 dlg Exp $ */

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

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#define ARC_DEBUG

#ifdef ARC_DEBUG
#define ARC_D_INIT	(1<<0)
#define ARC_D_RW	(1<<1)

int arcdebug = 0;

#define DPRINTF(p...)		do { if (arcdebug) printf(p); } while (0)
#define DNPRINTF(n, p...)	do { if ((n) & arcdebug) printf(p); } while (0)

#else
#define DPRINTF(p...)		/* p */
#define DNPRINTF(n, p...)	/* n, p */
#endif

static const struct pci_matchid arc_devices[] = {
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1110 },
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1120 },
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1130 },
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1160 },
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1170 },
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1210 },
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1220 },
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1230 },
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1260 },
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1270 },
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1280 },
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1380 },
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1381 },
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1680 },
	{ PCI_VENDOR_ARECA,	PCI_PRODUCT_ARECA_ARC1681 }
};

#define ARC_PCI_BAR			PCI_MAPREG_START

#define ARC_REG_INB_MSG0		0x0010
#define  ARC_REG_INB_MSG0_NOP			(0x00000000)
#define  ARC_REG_INB_MSG0_GET_CONFIG		(0x00000001)
#define  ARC_REG_INB_MSG0_SET_CONFIG		(0x00000002)
#define  ARC_REG_INB_MSG0_ABORT_CMD		(0x00000003)
#define  ARC_REG_INB_MSG0_STOP_BGRB		(0x00000004)
#define  ARC_REG_INB_MSG0_FLUSH_CACHE		(0x00000005)
#define  ARC_REG_INB_MSG0_START_BGRB		(0x00000006)
#define  ARC_REG_INB_MSG0_CHK331PENDING		(0x00000007)
#define  ARC_REG_INB_MSG0_SYNC_TIMER		(0x00000008)
#define ARC_REG_INB_MSG1		0x0014
#define ARC_REG_OUTB_ADDR0		0x0018
#define ARC_REG_OUTB_ADDR1		0x001c
#define  ARC_REG_OUTB_ADDR1_FIRMWARE_OK		(1<<31)
#define ARC_REG_INB_DOORBELL		0x0020
#define ARC_REG_INB_INTRSTAT		0x0024
#define  ARC_REG_INB_INTRSTAT_MSG0		(1<<0)
#define  ARC_REG_INB_INTRSTAT_MSG1		(1<<1)
#define  ARC_REG_INB_INTRSTAT_DOORBELL		(1<<2)
#define  ARC_REG_INB_INTRSTAT_DOORBELL_ERR	(1<<3)
#define  ARC_REG_INB_INTRSTAT_POSTQUEUE		(1<<4)
#define  ARC_REG_INB_INTRSTAT_QUEUEFULL		(1<<5)
#define  ARC_REG_INB_INTRSTAT_INDEX		(1<<6)
#define ARC_REG_INB_INTRMASK	0x0028
#define  ARC_REG_INB_INTRMASK_MSG0		(1<<0)
#define  ARC_REG_INB_INTRMASK_MSG1		(1<<1)
#define  ARC_REG_INB_INTRMASK_DOORBELL		(1<<2)
#define  ARC_REG_INB_INTRMASK_DOORBELL_ERR	(1<<3)
#define  ARC_REG_INB_INTRMASK_POSTQUEUE		(1<<4)
#define  ARC_REG_INB_INTRMASK_QUEUEFULL		(1<<5)
#define  ARC_REG_INB_INTRMASK_INDEX		(1<<6)
#define ARC_REG_OUTB_DOORBELL	0x002c
#define ARC_REG_OUTB_INTRSTAT	0x0030
#define  ARC_REG_OUTB_INTRSTAT_MSG0		(1<<0)
#define  ARC_REG_OUTB_INTRSTAT_MSG1		(1<<1)
#define  ARC_REG_OUTB_INTRSTAT_DOORBELL		(1<<2)
#define  ARC_REG_OUTB_INTRSTAT_POSTQUEUE	(1<<3)
#define  ARC_REG_OUTB_INTRSTAT_PCI		(1<<4)
#define ARC_REG_OUTB_INTRMASK	0x0034
#define  ARC_REG_OUTB_INTRMASK_MSG0		(1<<0)
#define  ARC_REG_OUTB_INTRMASK_MSG1		(1<<1)
#define  ARC_REG_OUTB_INTRMASK_DOORBELL		(1<<2)
#define  ARC_REG_OUTB_INTRMASK_POSTQUEUE	(1<<3)
#define  ARC_REG_OUTB_INTRMASK_PCI		(1<<4)
#define ARC_REG_POST_QUEUE	0x0040
#define  ARC_REG_POST_QUEUE_ADDR_SHIFT		5
#define  ARC_REG_POST_QUEUE_ADDR(r)	\
    ((r) >> ARC_REG_REPLY_QUEUE_ADDR_SHIFT)
#define  ARC_REG_POST_QUEUE_IAMBIOS		(1<<30)
#define  ARC_REG_POST_QUEUE_BIGFRAME		(1<<31)
#define ARC_REG_REPLY_QUEUE	0x0044
#define  ARC_REG_REPLY_QUEUE_ADDR_MASK		(0x07ffffff)
#define  ARC_REG_REPLY_QUEUE_ADDR_SHIFT		5
#define  ARC_REG_REPLY_QUEUE_ADDR(r)	\
    (((r) & ARC_REG_REPLY_QUEUE_ADDR_MASK) << ARC_REG_REPLY_QUEUE_ADDR_SHIFT)
#define  ARC_REG_REPLY_QUEUE_ERR		(1<<28)
#define  ARC_REG_REPLY_QUEUE_IAMBIOS		(1<<30)
#define ARC_REG_MSGBUF		0x0a00
#define  ARC_REG_MSGBUF_LEN	256	/* dwords */
#define ARC_REG_IOC_WBUF	0x0e00
#define  ARC_REG_IOC_WBUF_LEN	32	/* dwords */
#define ARC_REG_IOC_RBUF	0x0f00
#define  ARC_REG_IOC_RBUF_LEN	32	/* dwords */

struct arc_msg_firmware_info {
	u_int32_t		signature;
#define ARC_FWINFO_SIGNATURE_GET_CONFIG		(0x87974060)
	u_int32_t		request_len;
	u_int32_t		queue_len;
	u_int32_t		sdram_size;
	u_int32_t		sata_ports;
	u_int8_t		vendor[40];
	u_int8_t		model[8];
	u_int8_t		fw_version[16];
	u_int8_t		device_map[16];
} __packed;

struct arc_msg_scsicmd {
	u_int8_t		bus;
	u_int8_t		target;
	u_int8_t		lun;
	u_int8_t		function;

	u_int8_t		cdb_len;
	u_int8_t		sgl_len;
	u_int8_t		flags;
#define ARC_MSG_SCSICMD_FLAG_SGL_BSIZE_512	(1<<0)
#define ARC_MSG_SCSICMD_FLAG_FROM_BIOS		(1<<1)
#define ARC_MSG_SCSICMD_FLAG_WRITE		(1<<2)
#define ARC_MSG_SCSICMD_FLAG_SIMPLEQ		(0x00)
#define ARC_MSG_SCSICMD_FLAG_HEADQ		(0x08)
#define ARC_MSG_SCSICMD_FLAG_ORDERQ		(0x10)
	u_int8_t		reserved;

	u_int32_t		context;
	u_int32_t		data_len;

#define ARC_MSG_CDBLEN				16
	u_int8_t		cdb[ARC_MSG_CDBLEN];

#define ARC_MSG_SENSELEN			16
	u_int8_t		sense_data[ARC_MSG_SENSELEN];
#define ARC_MSG_SENSE_TIMEOUT			0xf0
#define ARC_MSG_SENSE_ABORTED			0xf1
#define ARC_MSG_SENSE_INIT_FAIL			0xf2

	/* followed by an sgl */
} __packed;

struct arc_sge {
	u_int32_t		sg_hdr;
#define ARC_SGE_64BIT				(1<<24)
	u_int32_t		sg_lo_addr;
	u_int32_t		sg_hi_addr;
} __packed;

#define ARC_MAX_TARGET		16
#define ARC_MAX_LUN		8

/* the firmware deals with up to 256 or 512 byte command frames. */
/* sizeof(struct arc_msg_scsicmd) + (sizeof(struct arc_sge) * 38) == 512 */
#define ARC_SGL_MAXLEN		38
/* sizeof(struct arc_msg_scsicmd) + (sizeof(struct arc_sge) * 17) == 256 */
#define ARC_SGL_256LEN		17

int			arc_match(struct device *, void *, void *);
void			arc_attach(struct device *, struct device *, void *);
int			arc_detach(struct device *, int);
int			arc_intr(void *);

struct arc_ccb;
TAILQ_HEAD(arc_ccb_list, arc_ccb);

struct arc_softc {
	struct device		sc_dev;
	struct scsi_link	sc_link;

	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	bus_dma_tag_t		sc_dmat;

	void			*sc_ih;

	int			sc_req_size;
	int			sc_req_count;

	struct arc_dmamem	*sc_requests;
	struct arc_ccb		*sc_ccbs;
	struct arc_ccb_list	sc_ccb_free;
};
#define DEVNAME(_s)		((_s)->sc_dev.dv_xname)

struct cfattach arc_ca = {
	sizeof(struct arc_softc), arc_match, arc_attach, arc_detach
};

struct cfdriver arc_cd = {
	NULL, "arc", DV_DULL
};

/* interface for scsi midlayer to talk to */
int			arc_scsi_cmd(struct scsi_xfer *);
void			arc_minphys(struct buf *);

struct scsi_adapter arc_switch = {
	arc_scsi_cmd, arc_minphys, NULL, NULL, NULL
};

struct scsi_device arc_dev = {
	NULL, NULL, NULL, NULL
};

/* code to deal with getting bits in and out of the bus space */
u_int32_t		arc_read(struct arc_softc *, bus_size_t);
void			arc_read_region(struct arc_softc *, bus_size_t,
			    void *, size_t);
void			arc_write(struct arc_softc *, bus_size_t, u_int32_t);
void			arc_write_region(struct arc_softc *, bus_size_t,
			    void *, size_t);
int			arc_wait_eq(struct arc_softc *, bus_size_t,
			    u_int32_t, u_int32_t);
int			arc_wait_ne(struct arc_softc *, bus_size_t,
			    u_int32_t, u_int32_t);

#define arc_push(_s, _r)	arc_write((_s), ARC_REG_POST_QUEUE, (_r))
#define arc_pop(_s)		arc_read((_s), ARC_REG_REPLY_QUEUE)

/* wrap up the bus_dma api */
struct arc_dmamem {
	bus_dmamap_t		adm_map;
	bus_dma_segment_t	adm_seg;
	size_t			adm_size;
	caddr_t			adm_kva;
};
#define ARC_DMA_MAP(_adm)	((_adm)->adm_map)
#define ARC_DMA_DVA(_adm)	((_adm)->adm_map->dm_segs[0].ds_addr)
#define ARC_DMA_KVA(_adm)	((void *)(_adm)->adm_kva)

struct arc_dmamem	*arc_dmamem_alloc(struct arc_softc *, size_t);
void			arc_dmamem_free(struct arc_softc *,
			    struct arc_dmamem *);

/* stuff to manage a scsi command */
struct arc_ccb {
	struct arc_softc	*ccb_sc;
	int			ccb_id;

	struct scsi_xfer	*ccb_xs;

	bus_dmamap_t		ccb_dmamap;
	bus_addr_t		ccb_offset;
	void			*ccb_cmd;
	u_int32_t		ccb_cmd_post;

	TAILQ_ENTRY(arc_ccb)	ccb_link;
};

int			arc_alloc_ccbs(struct arc_softc *);
struct arc_ccb		*arc_get_ccb(struct arc_softc *);
void			arc_put_ccb(struct arc_softc *, struct arc_ccb *);
int			arc_load_xs(struct arc_ccb *);
int			arc_complete(struct arc_softc *, struct arc_ccb *,
			    int);
void			arc_scsi_cmd_done(struct arc_softc *, struct arc_ccb *,
			    u_int32_t);

/* real stuff for dealing with the hardware */
int			arc_map_pci_resources(struct arc_softc *,
			    struct pci_attach_args *);
int			arc_query_firmware(struct arc_softc *);


int
arc_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, arc_devices,
	    sizeof(arc_devices) / sizeof(arc_devices[0])));
}

void
arc_attach(struct device *parent, struct device *self, void *aux)
{
	struct arc_softc		*sc = (struct arc_softc *)self;
	struct pci_attach_args		*pa = aux;

	if (arc_map_pci_resources(sc, pa) != 0) {
		/* error message printed by arc_map_pci_resources */
		return;
	}

	if (arc_query_firmware(sc) != 0) {
		/* error message printed by arc_query_firmware */
		return;
	}

	if (arc_alloc_ccbs(sc) != 0) {
		/* error message printed by arc_alloc_ccbs */
		return;
	}

	sc->sc_link.device = &arc_dev;
	sc->sc_link.adapter = &arc_switch;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = ARC_MAX_TARGET;
	sc->sc_link.adapter_buswidth = ARC_MAX_TARGET;
	sc->sc_link.openings = sc->sc_req_count / ARC_MAX_TARGET;

	config_found(self, &sc->sc_link, scsiprint);

	return;
}

int
arc_detach(struct device *self, int flags)
{
	return (0);
}

int
arc_intr(void *arg)
{
#if 0
	struct arc_softc		*sc = arg;
#endif

	return (0);
}

int
arc_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link		*link = xs->sc_link;
	struct arc_softc		*sc = link->adapter_softc;
	struct arc_ccb			*ccb;
	struct arc_msg_scsicmd		*cmd;
	u_int32_t			reg;
	int				rv = SUCCESSFULLY_QUEUED;
	int				s;

	if (xs->cmdlen > ARC_MSG_CDBLEN) {
		bzero(&xs->sense, sizeof(xs->sense));
		xs->sense.error_code = SSD_ERRCODE_VALID | 0x70;
		xs->sense.flags = SKEY_ILLEGAL_REQUEST;
		xs->sense.add_sense_code = 0x20;
		xs->error = XS_SENSE;
		s = splbio();
		scsi_done(xs);
		splx(s);
		return (COMPLETE);
	}

	s = splbio();
	ccb = arc_get_ccb(sc);
	splx(s);
	if (ccb == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		s = splbio();
		scsi_done(xs);
		splx(s);
		return (COMPLETE);
	}

	ccb->ccb_xs = xs;

	if (arc_load_xs(ccb) != 0) {
		xs->error = XS_DRIVER_STUFFUP;
		s = splbio();
		arc_put_ccb(sc, ccb);
		scsi_done(xs);
		splx(s);
		return (COMPLETE);
	}

	cmd = ccb->ccb_cmd;
	reg = ccb->ccb_cmd_post;

	/* bus is always 0 */
	cmd->target = link->target;
	cmd->lun = link->lun;
	cmd->function = 1; /* XXX magic number */

	cmd->cdb_len = xs->cmdlen;
	cmd->sgl_len = ccb->ccb_dmamap->dm_nsegs;
	if (xs->flags & SCSI_DATA_OUT)
		cmd->flags = ARC_MSG_SCSICMD_FLAG_WRITE;
	if (ccb->ccb_dmamap->dm_nsegs > ARC_SGL_256LEN) {
		cmd->flags |= ARC_MSG_SCSICMD_FLAG_SGL_BSIZE_512;
		reg |= ARC_REG_POST_QUEUE_BIGFRAME;
	}

	cmd->context = htole32(ccb->ccb_id);
	cmd->data_len = htole32(xs->datalen);

	bcopy(xs->cmd, cmd->cdb, xs->cmdlen);

	/* we've built the command, lets put it on the hw */
	bus_dmamap_sync(sc->sc_dmat, ARC_DMA_MAP(sc->sc_requests),
	    ccb->ccb_offset, sc->sc_req_size,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	s = splbio();
	arc_push(sc, reg);
#if 0
	if (xs->flags & SCSI_POLL) {
#endif
		rv = COMPLETE;
		if (arc_complete(sc, ccb, xs->timeout) != 0) {
			xs->error = XS_DRIVER_STUFFUP;
			scsi_done(xs);
		}
#if 0
	}
#endif
	splx(s);

	return (rv);
}

int
arc_load_xs(struct arc_ccb *ccb)
{
	struct arc_softc		*sc = ccb->ccb_sc;
	struct scsi_xfer		*xs = ccb->ccb_xs;
	bus_dmamap_t			dmap = ccb->ccb_dmamap;
	struct {
		struct arc_msg_scsicmd		cmd;
		struct arc_sge			sgl[ARC_SGL_MAXLEN];
	} __packed			*bundle = ccb->ccb_cmd;
	struct arc_sge			*sge;
	u_int64_t			addr;
	int				i, error;

	if (xs->datalen == 0)
		return (0);

	error = bus_dmamap_load(sc->sc_dmat, dmap,
	    xs->data, xs->datalen, NULL,
	    (xs->flags & SCSI_NOSLEEP) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error) {
		printf("%s: error %d loading dmamap\n", DEVNAME(sc), error);
		return (1);
	}

	for (i = 0; i < dmap->dm_nsegs; i++) {
		sge = &bundle->sgl[i];

		sge->sg_hdr = htole32(ARC_SGE_64BIT | dmap->dm_segs[i].ds_len);
		addr = dmap->dm_segs[i].ds_addr;
		sge->sg_hi_addr = htole32((u_int32_t)(addr >> 32));
		sge->sg_lo_addr = htole32((u_int32_t)addr);
	}

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_PREREAD :
	    BUS_DMASYNC_PREWRITE);

	return (0);
}

void
arc_scsi_cmd_done(struct arc_softc *sc, struct arc_ccb *ccb, u_int32_t reg)
{
	struct scsi_xfer		*xs = ccb->ccb_xs;

	if (xs->datalen != 0) {
		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize, (xs->flags & SCSI_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ccb->ccb_dmamap);
	}

	/* timeout_del */
	xs->flags |= ITSDONE;

	if (reg & ARC_REG_REPLY_QUEUE_ERR) {
		// printf("%s: something went wrong\n", DEVNAME(sc));
		xs->error = XS_DRIVER_STUFFUP;
	} else {
		xs->status = SCSI_OK;
		xs->error = XS_NOERROR;
		xs->resid = 0;
	}

	arc_put_ccb(sc, ccb);
	scsi_done(xs);
}

int
arc_complete(struct arc_softc *sc, struct arc_ccb *nccb, int timeout)
{
	struct arc_ccb			*ccb = NULL;
	char				*kva = ARC_DMA_KVA(sc->sc_requests);
	struct arc_msg_scsicmd		*cmd;
	int				diff;
	u_int32_t			reg;

	do {

		reg = arc_pop(sc);
		if (reg == 0xffffffff) {
			if (timeout-- == 0)
				return (1);

			delay(1000);
			continue;
		}

		diff = (reg << 5) - ARC_DMA_DVA(sc->sc_requests);
		cmd = (struct arc_msg_scsicmd *)(kva + diff);

		ccb = &sc->sc_ccbs[cmd->context];

		bus_dmamap_sync(sc->sc_dmat, ARC_DMA_MAP(sc->sc_requests),
		    ccb->ccb_offset, sc->sc_req_size,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		arc_scsi_cmd_done(sc, ccb, reg);

	} while (nccb != ccb);

	return (0);
}

void
arc_minphys(struct buf *bp)
{
	if (bp->b_bcount > MAXPHYS)
		bp->b_bcount = MAXPHYS;
	minphys(bp);
}

int
arc_map_pci_resources(struct arc_softc *sc, struct pci_attach_args *pa)
{
	pcireg_t			memtype;
	pci_intr_handle_t		ih;
	const char			*intrstr;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	memtype = pci_mapreg_type(sc->sc_pc, sc->sc_tag, ARC_PCI_BAR);
	if (pci_mapreg_map(pa, ARC_PCI_BAR, memtype, 0, &sc->sc_iot,
	    &sc->sc_ioh, NULL, &sc->sc_ios, 0) != 0) {
		printf(": unable to map system interface register\n");
		return(1);
	}

	if (pci_intr_map(pa, &ih) != 0) {
		printf(": unable to map interrupt\n");
		goto unmap;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO,
	    arc_intr, sc, DEVNAME(sc));
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
arc_query_firmware(struct arc_softc *sc)
{
	struct arc_msg_firmware_info	fwinfo;
	char				string[81]; /* sizeof(vendor)*2+1 */

	if (arc_wait_eq(sc, ARC_REG_OUTB_ADDR1, ARC_REG_OUTB_ADDR1_FIRMWARE_OK,
	    ARC_REG_OUTB_ADDR1_FIRMWARE_OK) != 0) {
		printf("%s: timeout waiting for firmware ok\n");
		return (1);
	}

	arc_write(sc, ARC_REG_INB_MSG0, ARC_REG_INB_MSG0_GET_CONFIG);
	if (arc_wait_eq(sc, ARC_REG_OUTB_INTRSTAT, ARC_REG_OUTB_INTRSTAT_MSG0,
	    ARC_REG_OUTB_INTRSTAT_MSG0) != 0) {
		printf("%s: timeout waiting for get config\n");
		return (1);
	}
	arc_write(sc, ARC_REG_OUTB_INTRSTAT, ARC_REG_OUTB_INTRSTAT_MSG0);

	arc_read_region(sc, ARC_REG_MSGBUF, &fwinfo, sizeof(fwinfo));

	DNPRINTF(ARC_D_INIT, "%s: signature: 0x%08x\n", DEVNAME(sc),
	    letoh32(fwinfo.signature));

	if (letoh32(fwinfo.signature) != ARC_FWINFO_SIGNATURE_GET_CONFIG) {
		printf("%s: invalid firmware info from iop\n", DEVNAME(sc));
		return (1);
	}

	DNPRINTF(ARC_D_INIT, "%s: request_len: %d\n", DEVNAME(sc),
	    letoh32(fwinfo.request_len));
	DNPRINTF(ARC_D_INIT, "%s: queue_len: %d\n", DEVNAME(sc),
	    letoh32(fwinfo.queue_len));
	DNPRINTF(ARC_D_INIT, "%s: sdram_size: %d\n", DEVNAME(sc),
	    letoh32(fwinfo.sdram_size));
	DNPRINTF(ARC_D_INIT, "%s: sata_ports: %d\n", DEVNAME(sc),
	    letoh32(fwinfo.sata_ports), letoh32(fwinfo.sata_ports));

#ifdef ARC_DEBUG
	scsi_strvis(string, fwinfo.vendor, sizeof(fwinfo.vendor));
	DNPRINTF(ARC_D_INIT, "%s: vendor: \"%s\"\n", DEVNAME(sc), string);
	scsi_strvis(string, fwinfo.model, sizeof(fwinfo.model));
	DNPRINTF(ARC_D_INIT, "%s: model: \"%s\"\n", DEVNAME(sc), string);
#endif /* ARC_DEBUG */

	scsi_strvis(string, fwinfo.fw_version, sizeof(fwinfo.fw_version));
	DNPRINTF(ARC_D_INIT, "%s: model: \"%s\"\n", DEVNAME(sc), string);

	/* device map? */

	sc->sc_req_size = letoh32(fwinfo.request_len); /* always 512 */
	sc->sc_req_count = letoh32(fwinfo.queue_len);

	printf("%s: %d SATA Ports, %dMB SDRAM, FW Version: %s\n",
	    DEVNAME(sc), letoh32(fwinfo.sata_ports),
	    letoh32(fwinfo.sdram_size), string);

	return (0);
}

u_int32_t
arc_read(struct arc_softc *sc, bus_size_t r)
{
	u_int32_t			v;

	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	v = bus_space_read_4(sc->sc_iot, sc->sc_ioh, r);

	DNPRINTF(ARC_D_RW, "%s: arc_read 0x%x 0x%08x\n", DEVNAME(sc), r, v);

	return (v);
}

void
arc_read_region(struct arc_softc *sc, bus_size_t r, void *buf, size_t len)
{
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, len,
	    BUS_SPACE_BARRIER_READ);
	bus_space_read_raw_region_4(sc->sc_iot, sc->sc_ioh, r, buf, len);
}

void
arc_write(struct arc_softc *sc, bus_size_t r, u_int32_t v)
{
	DNPRINTF(ARC_D_RW, "%s: arc_write 0x%x 0x%08x\n", DEVNAME(sc), r, v);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, r, v);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

void
arc_write_region(struct arc_softc *sc, bus_size_t r, void *buf, size_t len)
{
	bus_space_write_raw_region_4(sc->sc_iot, sc->sc_ioh, r, buf, len);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, len,
	    BUS_SPACE_BARRIER_WRITE);
}

int
arc_wait_eq(struct arc_softc *sc, bus_size_t r, u_int32_t mask,
    u_int32_t target)
{
	int				i;

	DNPRINTF(ARC_D_RW, "%s: arc_wait_eq 0x%x 0x%08x 0x%08x\n",
	    DEVNAME(sc), r, mask, target);

	for (i = 0; i < 10000; i++) {
		if ((arc_read(sc, r) & mask) == target)
			return (0);
		delay(1000);
	}

	return (1);
}

int
arc_wait_ne(struct arc_softc *sc, bus_size_t r, u_int32_t mask,
    u_int32_t target)
{
	int				i;

	DNPRINTF(ARC_D_RW, "%s: arc_wait_ne 0x%x 0x%08x 0x%08x\n",
	    DEVNAME(sc), r, mask, target);

	for (i = 0; i < 10000; i++) {
		if ((arc_read(sc, r) & mask) != target)
			return (0);
		delay(1000);
	}

	return (1);
}

struct arc_dmamem *
arc_dmamem_alloc(struct arc_softc *sc, size_t size)
{
	struct arc_dmamem		*adm;
	int				nsegs;

	adm = malloc(sizeof(struct arc_dmamem), M_DEVBUF, M_NOWAIT);
	if (adm == NULL)
		return (NULL);

	bzero(adm, sizeof(struct arc_dmamem));
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
arc_dmamem_free(struct arc_softc *sc, struct arc_dmamem *adm)
{
	bus_dmamap_unload(sc->sc_dmat, adm->adm_map);
	bus_dmamem_unmap(sc->sc_dmat, adm->adm_kva, adm->adm_size);
	bus_dmamem_free(sc->sc_dmat, &adm->adm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, adm->adm_map);
	free(adm, M_DEVBUF);
}

int
arc_alloc_ccbs(struct arc_softc *sc)
{
	struct arc_ccb			*ccb;
	u_int8_t			*cmd;
	int				i;

	TAILQ_INIT(&sc->sc_ccb_free);

	sc->sc_ccbs = malloc(sizeof(struct arc_ccb) * sc->sc_req_count,
	    M_DEVBUF, M_WAITOK);
	if (sc->sc_ccbs == NULL) {
		printf("%s: unable to allocate ccbs\n", DEVNAME(sc));
		return (1);
	}
	bzero(sc->sc_ccbs, sizeof(struct arc_ccb) * sc->sc_req_count);

	sc->sc_requests = arc_dmamem_alloc(sc,
	    sc->sc_req_size * sc->sc_req_count);
	if (sc->sc_requests == NULL) {
		printf("%s: unable to allocate ccb dmamem\n", DEVNAME(sc));
		goto free_ccbs;
	}
	cmd = ARC_DMA_KVA(sc->sc_requests);

	for (i = 0; i < sc->sc_req_count; i++) {
		ccb = &sc->sc_ccbs[i];

		if (bus_dmamap_create(sc->sc_dmat, MAXPHYS, 1, MAXPHYS, 0, 0,
		    &ccb->ccb_dmamap) != 0) {
			printf("%s: unable to create dmamap for ccb %d\n",
			    DEVNAME(sc), i);
			goto free_maps;
		}

		ccb->ccb_sc = sc;
		ccb->ccb_id = i;
		ccb->ccb_offset = sc->sc_req_size * i;

		ccb->ccb_cmd = &cmd[ccb->ccb_offset];
		ccb->ccb_cmd_post = (ARC_DMA_DVA(sc->sc_requests) +
		    ccb->ccb_offset) >> 5; /* XXX magic number */

		arc_put_ccb(sc, ccb);
	}

	return (0);

free_maps:
	while ((ccb = arc_get_ccb(sc)) != NULL)
	    bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);
	arc_dmamem_free(sc, sc->sc_requests);

free_ccbs:
	free(sc->sc_ccbs, M_DEVBUF);

	return (1);
}

struct arc_ccb *
arc_get_ccb(struct arc_softc *sc)
{
	struct arc_ccb			*ccb;

	ccb = TAILQ_FIRST(&sc->sc_ccb_free);
	if (ccb != NULL)
		TAILQ_REMOVE(&sc->sc_ccb_free, ccb, ccb_link);

	return (ccb);
}

void
arc_put_ccb(struct arc_softc *sc, struct arc_ccb *ccb)
{
	ccb->ccb_xs = NULL;
	bzero(ccb->ccb_cmd, sc->sc_req_size);
	TAILQ_INSERT_TAIL(&sc->sc_ccb_free, ccb, ccb_link);
}
