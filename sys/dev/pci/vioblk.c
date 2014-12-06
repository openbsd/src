/*	$OpenBSD: vioblk.c,v 1.6 2014/12/06 10:11:46 sf Exp $	*/

/*
 * Copyright (c) 2012 Stefan Fritsch.
 * Copyright (c) 2010 Minoura Makoto.
 * Copyright (c) 1998, 2001 Manuel Bouyer.
 * All rights reserved.
 *
 * This code is based in part on the NetBSD ld_virtio driver and the
 * OpenBSD vdsk driver.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2009, 2011 Mark Kettenis
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
#include <sys/kernel.h>
#include <machine/bus.h>

#include <sys/device.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/virtioreg.h>
#include <dev/pci/virtiovar.h>
#include <dev/pci/vioblkreg.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#define VIOBLK_DONE	-1

#define MAX_XFER	MAX(MAXPHYS,MAXBSIZE)
/* Number of DMA segments for buffers that the device must support */
#define SEG_MAX		(MAX_XFER/PAGE_SIZE + 1)
/* In the virtqueue, we need space for header and footer, too */
#define ALLOC_SEGS	(SEG_MAX + 2)

struct virtio_feature_name vioblk_feature_names[] = {
	{ VIRTIO_BLK_F_BARRIER,		"Barrier" },
	{ VIRTIO_BLK_F_SIZE_MAX,	"SizeMax" },
	{ VIRTIO_BLK_F_SEG_MAX,		"SegMax" },
	{ VIRTIO_BLK_F_GEOMETRY,	"Geometry" },
	{ VIRTIO_BLK_F_RO,		"RO" },
	{ VIRTIO_BLK_F_BLK_SIZE,	"BlkSize" },
	{ VIRTIO_BLK_F_SCSI,		"SCSI" },
	{ VIRTIO_BLK_F_FLUSH,		"Flush" },
	{ VIRTIO_BLK_F_TOPOLOGY,	"Topology" },
	{ 0,				NULL }
};

struct virtio_blk_req {
	struct virtio_blk_req_hdr	 vr_hdr;
	uint8_t				 vr_status;
	struct scsi_xfer		*vr_xs;
	int				 vr_len;
	bus_dmamap_t			 vr_cmdsts;
	bus_dmamap_t			 vr_payload;
};

struct vioblk_softc {
	struct device		 sc_dev;
	struct virtio_softc	*sc_virtio;

	struct virtqueue         sc_vq[1];
	struct virtio_blk_req   *sc_reqs;
	bus_dma_segment_t        sc_reqs_segs[1];

	struct scsi_adapter	 sc_switch;
	struct scsi_link	 sc_link;

	int			 sc_notify_on_empty;

	uint32_t		 sc_queued;

	uint64_t		 sc_capacity;
};

int	vioblk_match(struct device *, void *, void *);
void	vioblk_attach(struct device *, struct device *, void *);
int	vioblk_alloc_reqs(struct vioblk_softc *, int);
int	vioblk_vq_done(struct virtqueue *);
void	vioblk_vq_done1(struct vioblk_softc *, struct virtio_softc *,
			struct virtqueue *, int);

void	vioblk_scsi_cmd(struct scsi_xfer *);
int	vioblk_dev_probe(struct scsi_link *);
void	vioblk_dev_free(struct scsi_link *);

void	vioblk_scsi_inq(struct scsi_xfer *);
void	vioblk_scsi_capacity(struct scsi_xfer *);
void	vioblk_scsi_capacity16(struct scsi_xfer *);
void	vioblk_scsi_done(struct scsi_xfer *, int);

struct cfattach vioblk_ca = {
	sizeof(struct vioblk_softc),
	vioblk_match,
	vioblk_attach,
	NULL
};

struct cfdriver vioblk_cd = {
	NULL, "vioblk", DV_DULL
};


int vioblk_match(struct device *parent, void *match, void *aux)
{
	struct virtio_softc *va = aux;
	if (va->sc_childdevid == PCI_PRODUCT_VIRTIO_BLOCK)
		return 1;
	return 0;
}

#if VIRTIO_DEBUG > 0
#define DBGPRINT(fmt, args...) printf("%s: " fmt "\n", __func__, ## args)
#else
#define DBGPRINT(fmt, args...)		do {} while (0)
#endif

void
vioblk_attach(struct device *parent, struct device *self, void *aux)
{
	struct vioblk_softc *sc = (struct vioblk_softc *)self;
	struct virtio_softc *vsc = (struct virtio_softc *)parent;
	struct scsibus_attach_args saa;
	uint32_t features;
	int qsize;

	vsc->sc_vqs = &sc->sc_vq[0];
	vsc->sc_nvqs = 1;
	vsc->sc_config_change = 0;
	if (vsc->sc_child)
		panic("already attached to something else");
	vsc->sc_child = self;
	vsc->sc_ipl = IPL_BIO;
	vsc->sc_intrhand = virtio_vq_intr;
	sc->sc_virtio = vsc;

        features = virtio_negotiate_features(vsc,
	    (VIRTIO_BLK_F_RO       | VIRTIO_F_NOTIFY_ON_EMPTY |
	     VIRTIO_BLK_F_SIZE_MAX | VIRTIO_BLK_F_SEG_MAX |
	     VIRTIO_BLK_F_FLUSH),
	    vioblk_feature_names);


	if (features & VIRTIO_BLK_F_SIZE_MAX) {
		uint32_t size_max = virtio_read_device_config_4(vsc,
		    VIRTIO_BLK_CONFIG_SIZE_MAX);
		if (size_max < PAGE_SIZE) {
			printf("\nMax segment size %u too low\n", size_max);
			goto err;
		}
	}

	if (features & VIRTIO_BLK_F_SEG_MAX) {
		uint32_t seg_max = virtio_read_device_config_4(vsc,
		    VIRTIO_BLK_CONFIG_SEG_MAX);
		if (seg_max < SEG_MAX) {
			printf("\nMax number of segments %d too small\n",
			    seg_max);
			goto err;
		}
	}

	sc->sc_capacity = virtio_read_device_config_8(vsc,
	    VIRTIO_BLK_CONFIG_CAPACITY);

	if (virtio_alloc_vq(vsc, &sc->sc_vq[0], 0, MAX_XFER, ALLOC_SEGS,
	    "I/O request") != 0) {
		printf("\nCan't alloc virtqueue\n");
		goto err;
	}
	qsize = sc->sc_vq[0].vq_num;
	sc->sc_vq[0].vq_done = vioblk_vq_done;
	if (vioblk_alloc_reqs(sc, qsize) < 0) {
		printf("\nCan't alloc reqs\n");
		goto err;
	}

	if (features & VIRTIO_F_NOTIFY_ON_EMPTY) {
		virtio_stop_vq_intr(vsc, &sc->sc_vq[0]);
		sc->sc_notify_on_empty = 1;
	}
	else {
		sc->sc_notify_on_empty = 0;
	}

	sc->sc_queued = 0;

	sc->sc_switch.scsi_cmd = vioblk_scsi_cmd;
	sc->sc_switch.scsi_minphys = scsi_minphys;
	sc->sc_switch.dev_probe = vioblk_dev_probe;
	sc->sc_switch.dev_free = vioblk_dev_free;

	sc->sc_link.adapter = &sc->sc_switch;
	sc->sc_link.adapter_softc = self;
	sc->sc_link.adapter_buswidth = 2;
	sc->sc_link.luns = 1;
	sc->sc_link.adapter_target = 2;
	sc->sc_link.openings = qsize;
	DBGPRINT("; qsize: %d", qsize);
	if (features & VIRTIO_BLK_F_RO)
		sc->sc_link.flags |= SDEV_READONLY;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;
	printf("\n");
	config_found(self, &saa, scsiprint);

	return;
err:
	vsc->sc_child = VIRTIO_CHILD_ERROR;
	return;
}

int
vioblk_vq_done(struct virtqueue *vq)
{
	struct virtio_softc *vsc = vq->vq_owner;
	struct vioblk_softc *sc = (struct vioblk_softc *)vsc->sc_child;
	int slot;
	int ret = 0;

	if (!sc->sc_notify_on_empty)
		virtio_stop_vq_intr(vsc, vq);
	for (;;) {
		if (virtio_dequeue(vsc, vq, &slot, NULL) != 0) {
			if (sc->sc_notify_on_empty)
				break;
			virtio_start_vq_intr(vsc, vq);
			if (virtio_dequeue(vsc, vq, &slot, NULL) != 0)
				break;
		}
		vioblk_vq_done1(sc, vsc, vq, slot);
		ret = 1;
	}
	return ret;
}

void
vioblk_vq_done1(struct vioblk_softc *sc, struct virtio_softc *vsc,
    struct virtqueue *vq, int slot)
{
	struct virtio_blk_req *vr = &sc->sc_reqs[slot];
	struct scsi_xfer *xs = vr->vr_xs;
	KASSERT(vr->vr_len != VIOBLK_DONE);
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_cmdsts, 0,
	    sizeof(struct virtio_blk_req_hdr), BUS_DMASYNC_POSTWRITE);
	if (vr->vr_hdr.type != VIRTIO_BLK_T_FLUSH) {
		bus_dmamap_sync(vsc->sc_dmat, vr->vr_payload, 0, vr->vr_len,
		    (vr->vr_hdr.type == VIRTIO_BLK_T_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	}
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_cmdsts,
	    sizeof(struct virtio_blk_req_hdr), sizeof(uint8_t),
	    BUS_DMASYNC_POSTREAD);


	if (vr->vr_status != VIRTIO_BLK_S_OK) {
		DBGPRINT("EIO");
		xs->error = XS_DRIVER_STUFFUP;
		xs->resid = xs->datalen;
	} else {
		xs->error = XS_NOERROR;
		xs->resid = xs->datalen - vr->vr_len;
	}
	scsi_done(xs);
	vr->vr_len = VIOBLK_DONE;

	virtio_dequeue_commit(vq, slot);
}

void
vioblk_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_rw *rw;
	struct scsi_rw_big *rwb;
	struct scsi_rw_12 *rw12;
	struct scsi_rw_16 *rw16;
	u_int64_t lba = 0;
	u_int32_t sector_count;
	uint8_t operation;
	int isread;

	switch (xs->cmd->opcode) {
	case READ_BIG:
	case READ_COMMAND:
	case READ_12:
	case READ_16:
		operation = VIRTIO_BLK_T_IN;
		isread = 1;
		break;
	case WRITE_BIG:
	case WRITE_COMMAND:
	case WRITE_12:
	case WRITE_16:
		operation = VIRTIO_BLK_T_OUT;
		isread = 0;
		break;

	case SYNCHRONIZE_CACHE:
		operation = VIRTIO_BLK_T_FLUSH;
		break;

	case INQUIRY:
		vioblk_scsi_inq(xs);
		return;
	case READ_CAPACITY:
		vioblk_scsi_capacity(xs);
		return;
	case READ_CAPACITY_16:
		vioblk_scsi_capacity16(xs);
		return;

	case TEST_UNIT_READY:
	case START_STOP:
	case PREVENT_ALLOW:
		vioblk_scsi_done(xs, XS_NOERROR);
		return;

	default:
		printf("%s cmd 0x%02x\n", __func__, xs->cmd->opcode);
	case MODE_SENSE:
	case MODE_SENSE_BIG:
	case REPORT_LUNS:
		vioblk_scsi_done(xs, XS_DRIVER_STUFFUP);
		return;
	}

	/*
	 * READ/WRITE/SYNCHRONIZE commands. SYNCHRONIZE CACHE has same
	 * layout as 10-byte READ/WRITE commands.
	 */
	if (xs->cmdlen == 6) {
		rw = (struct scsi_rw *)xs->cmd;
		lba = _3btol(rw->addr) & (SRW_TOPADDR << 16 | 0xffff);
		sector_count = rw->length ? rw->length : 0x100;
	} else if (xs->cmdlen == 10) {
		rwb = (struct scsi_rw_big *)xs->cmd;
		lba = _4btol(rwb->addr);
		sector_count = _2btol(rwb->length);
	} else if (xs->cmdlen == 12) {
		rw12 = (struct scsi_rw_12 *)xs->cmd;
		lba = _4btol(rw12->addr);
		sector_count = _4btol(rw12->length);
	} else if (xs->cmdlen == 16) {
		rw16 = (struct scsi_rw_16 *)xs->cmd;
		lba = _8btol(rw16->addr);
		sector_count = _4btol(rw16->length);
	}

{
	struct vioblk_softc *sc = xs->sc_link->adapter_softc;
	struct virtqueue *vq = &sc->sc_vq[0];
	struct virtio_blk_req *vr;
	struct virtio_softc *vsc = sc->sc_virtio;
	int len, s;
	int timeout;
	int slot, ret, nsegs;
	int error = XS_NO_CCB;

	s = splbio();
	ret = virtio_enqueue_prep(vq, &slot);
	if (ret) {
		DBGPRINT("virtio_enqueue_prep: %d, vq_num: %d, sc_queued: %d",
		    ret, vq->vq_num, sc->sc_queued);
		vioblk_scsi_done(xs, XS_NO_CCB);
		splx(s);
		return;
	}
	vr = &sc->sc_reqs[slot];
	if (operation != VIRTIO_BLK_T_FLUSH) {
		len = MIN(xs->datalen, sector_count * VIRTIO_BLK_SECTOR_SIZE);
		ret = bus_dmamap_load(vsc->sc_dmat, vr->vr_payload,
		    xs->data, len, NULL,
		    ((isread ? BUS_DMA_READ : BUS_DMA_WRITE) |
		     BUS_DMA_NOWAIT));
		if (ret) {
			printf("%s: bus_dmamap_load: %d", __func__, ret);
			error = XS_DRIVER_STUFFUP;
			goto out_enq_abort;
		}
		nsegs = vr->vr_payload->dm_nsegs + 2;
	} else {
		len = 0;
		nsegs = 2;
	}
	ret = virtio_enqueue_reserve(vq, slot, nsegs);
	if (ret) {
		DBGPRINT("virtio_enqueue_reserve: %d", ret);
		bus_dmamap_unload(vsc->sc_dmat, vr->vr_payload);
		goto out_done;
	}
	vr->vr_xs = xs;
	vr->vr_hdr.type = operation;
	vr->vr_hdr.ioprio = 0;
	vr->vr_hdr.sector = lba;
	vr->vr_len = len;

	bus_dmamap_sync(vsc->sc_dmat, vr->vr_cmdsts,
			0, sizeof(struct virtio_blk_req_hdr),
			BUS_DMASYNC_PREWRITE);
	if (operation != VIRTIO_BLK_T_FLUSH) {
		bus_dmamap_sync(vsc->sc_dmat, vr->vr_payload, 0, len,
		    isread ? BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);
	}
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_cmdsts,
	    offsetof(struct virtio_blk_req, vr_status), sizeof(uint8_t),
	    BUS_DMASYNC_PREREAD);

	virtio_enqueue_p(vq, slot, vr->vr_cmdsts, 0,
	    sizeof(struct virtio_blk_req_hdr), 1);
	if (operation != VIRTIO_BLK_T_FLUSH)
		virtio_enqueue(vq, slot, vr->vr_payload, !isread);
	virtio_enqueue_p(vq, slot, vr->vr_cmdsts,
	    offsetof(struct virtio_blk_req, vr_status), sizeof(uint8_t), 0);
	virtio_enqueue_commit(vsc, vq, slot, 1);
	sc->sc_queued++;

	if (!ISSET(xs->flags, SCSI_POLL)) {
		/* check if some xfers are done: */
		if (sc->sc_queued > 1)
			vioblk_vq_done(vq);
		splx(s);
		return;
	}

	timeout = 1000;
	do {
		if (vsc->sc_ops->intr(vsc) && vr->vr_len == VIOBLK_DONE)
			break;

		delay(1000);
	} while(--timeout > 0);
	splx(s);
	return;

out_enq_abort:
	virtio_enqueue_abort(vq, slot);
out_done:
	vioblk_scsi_done(xs, error);
	vr->vr_len = VIOBLK_DONE;
	splx(s);
}
}

void
vioblk_scsi_inq(struct scsi_xfer *xs)
{
	struct scsi_inquiry *inq = (struct scsi_inquiry *)xs->cmd;
	struct scsi_inquiry_data inqd;

	if (ISSET(inq->flags, SI_EVPD)) {
		vioblk_scsi_done(xs, XS_DRIVER_STUFFUP);
		return;
	}

	bzero(&inqd, sizeof(inqd));

	inqd.device = T_DIRECT;
	inqd.version = 0x05; /* SPC-3 */
	inqd.response_format = 2;
	inqd.additional_length = 32;
	inqd.flags |= SID_CmdQue;
	bcopy("VirtIO  ", inqd.vendor, sizeof(inqd.vendor));
	bcopy("Block Device    ", inqd.product, sizeof(inqd.product));

	bcopy(&inqd, xs->data, MIN(sizeof(inqd), xs->datalen));
	vioblk_scsi_done(xs, XS_NOERROR);
}

void
vioblk_scsi_capacity(struct scsi_xfer *xs)
{
	struct vioblk_softc *sc = xs->sc_link->adapter_softc;
	struct scsi_read_cap_data rcd;
	uint64_t capacity;

	bzero(&rcd, sizeof(rcd));

	capacity = sc->sc_capacity - 1;
	if (capacity > 0xffffffff)
		capacity = 0xffffffff;

	_lto4b(capacity, rcd.addr);
	_lto4b(VIRTIO_BLK_SECTOR_SIZE, rcd.length);

	bcopy(&rcd, xs->data, MIN(sizeof(rcd), xs->datalen));
	vioblk_scsi_done(xs, XS_NOERROR);
}

void
vioblk_scsi_capacity16(struct scsi_xfer *xs)
{
	struct vioblk_softc *sc = xs->sc_link->adapter_softc;
	struct scsi_read_cap_data_16 rcd;

	bzero(&rcd, sizeof(rcd));

	_lto8b(sc->sc_capacity - 1, rcd.addr);
	_lto4b(VIRTIO_BLK_SECTOR_SIZE, rcd.length);

	bcopy(&rcd, xs->data, MIN(sizeof(rcd), xs->datalen));
	vioblk_scsi_done(xs, XS_NOERROR);
}

void
vioblk_scsi_done(struct scsi_xfer *xs, int error)
{
	xs->error = error;
	scsi_done(xs);
}

int
vioblk_dev_probe(struct scsi_link *link)
{
	KASSERT(link->lun == 0);
	if (link->target == 0)
		return (0);
	return (ENODEV);
}

void
vioblk_dev_free(struct scsi_link *link)
{
	printf("%s\n", __func__);
}

int
vioblk_alloc_reqs(struct vioblk_softc *sc, int qsize)
{
	int allocsize, r, rsegs, i;
	void *vaddr;

	allocsize = sizeof(struct virtio_blk_req) * qsize;
	r = bus_dmamem_alloc(sc->sc_virtio->sc_dmat, allocsize, 0, 0,
	    &sc->sc_reqs_segs[0], 1, &rsegs, BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("DMA memory allocation failed, size %d, error %d\n",
		    allocsize, r);
		goto err_none;
	}
	r = bus_dmamem_map(sc->sc_virtio->sc_dmat, &sc->sc_reqs_segs[0], 1,
	    allocsize, (caddr_t *)&vaddr, BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("DMA memory map failed, error %d\n", r);
		goto err_dmamem_alloc;
	}
	sc->sc_reqs = vaddr;
	memset(vaddr, 0, allocsize);
	for (i = 0; i < qsize; i++) {
		struct virtio_blk_req *vr = &sc->sc_reqs[i];
		vr->vr_len = VIOBLK_DONE;
		r = bus_dmamap_create(sc->sc_virtio->sc_dmat,
		    offsetof(struct virtio_blk_req, vr_xs), 1,
		    offsetof(struct virtio_blk_req, vr_xs), 0,
		    BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW, &vr->vr_cmdsts);
		if (r != 0) {
			printf("cmd dmamap creation failed, err %d\n", r);
			goto err_reqs;
		}
		r = bus_dmamap_load(sc->sc_virtio->sc_dmat, vr->vr_cmdsts,
		    &vr->vr_hdr, offsetof(struct virtio_blk_req, vr_xs), NULL,
		    BUS_DMA_NOWAIT);
		if (r != 0) {
			printf("command dmamap load failed, err %d\n", r);
			goto err_reqs;
		}
		r = bus_dmamap_create(sc->sc_virtio->sc_dmat, MAX_XFER,
		    SEG_MAX, MAX_XFER, 0, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW,
		    &vr->vr_payload);
		if (r != 0) {
			printf("payload dmamap creation failed, err %d\n", r);
			goto err_reqs;
		}
	}
	return 0;

err_reqs:
	for (i = 0; i < qsize; i++) {
		struct virtio_blk_req *vr = &sc->sc_reqs[i];
		if (vr->vr_cmdsts) {
			bus_dmamap_destroy(sc->sc_virtio->sc_dmat,
			    vr->vr_cmdsts);
			vr->vr_cmdsts = 0;
		}
		if (vr->vr_payload) {
			bus_dmamap_destroy(sc->sc_virtio->sc_dmat,
			    vr->vr_payload);
			vr->vr_payload = 0;
		}
	}
	bus_dmamem_unmap(sc->sc_virtio->sc_dmat, (caddr_t)sc->sc_reqs,
	    allocsize);
err_dmamem_alloc:
	bus_dmamem_free(sc->sc_virtio->sc_dmat, &sc->sc_reqs_segs[0], 1);
err_none:
	return -1;
}
