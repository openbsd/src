/*	$OpenBSD: vioscsi.c,v 1.3 2015/03/14 03:38:49 jsg Exp $	*/
/*
 * Copyright (c) 2013 Google Inc.
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
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/vioscsireg.h>
#include <dev/pci/virtiovar.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

enum { vioscsi_debug = 0 };
#define DPRINTF(f...) do { if (vioscsi_debug) printf(f); } while (0)

struct vioscsi_req {
	struct virtio_scsi_req_hdr	 vr_req;
	struct virtio_scsi_res_hdr	 vr_res;
	struct scsi_xfer		*vr_xs;
	bus_dmamap_t			 vr_control;
	bus_dmamap_t			 vr_data;
};

struct vioscsi_softc {
	struct device		 sc_dev;
	struct scsi_link	 sc_link;
	struct scsibus		*sc_scsibus;
	struct scsi_iopool	 sc_iopool;

	struct virtqueue	 sc_vqs[3];
	struct vioscsi_req	*sc_reqs;
	bus_dma_segment_t        sc_reqs_segs[1];

	u_int32_t		 sc_seg_max;
};

int		 vioscsi_match(struct device *, void *, void *);
void		 vioscsi_attach(struct device *, struct device *, void *);

int		 vioscsi_alloc_reqs(struct vioscsi_softc *,
		    struct virtio_softc *, int, uint32_t);
void		 vioscsi_scsi_cmd(struct scsi_xfer *);
int		 vioscsi_vq_done(struct virtqueue *);
void		 vioscsi_req_done(struct vioscsi_softc *, struct virtio_softc *,
		    struct vioscsi_req *);
void		*vioscsi_req_get(void *);
void		 vioscsi_req_put(void *, void *);

struct cfattach vioscsi_ca = {
	sizeof(struct vioscsi_softc),
	vioscsi_match,
	vioscsi_attach,
};

struct cfdriver vioscsi_cd = {
	NULL,
	"vioscsi",
	DV_DULL,
};

struct scsi_adapter vioscsi_switch = {
	vioscsi_scsi_cmd,
	scsi_minphys,
};

const char *const vioscsi_vq_names[] = {
	"control",
	"event",
	"request",
};

int
vioscsi_match(struct device *parent, void *self, void *aux)
{
	struct virtio_softc *va = (struct virtio_softc *)aux;

	if (va->sc_childdevid == PCI_PRODUCT_VIRTIO_SCSI)
		return (1);
	return (0);
}

void
vioscsi_attach(struct device *parent, struct device *self, void *aux)
{
	struct virtio_softc *vsc = (struct virtio_softc *)parent;
	struct vioscsi_softc *sc = (struct vioscsi_softc *)self;
	struct scsibus_attach_args saa;
	int i, rv;

	if (vsc->sc_child != NULL) {
		printf(": parent already has a child\n");
		return;
	}
	vsc->sc_child = &sc->sc_dev;
	vsc->sc_ipl = IPL_BIO;
	vsc->sc_intrhand = virtio_vq_intr;

	// TODO(matthew): Negotiate hotplug.

	vsc->sc_vqs = sc->sc_vqs;
	vsc->sc_nvqs = nitems(sc->sc_vqs);

	uint32_t cmd_per_lun = virtio_read_device_config_4(vsc,
	    VIRTIO_SCSI_CONFIG_CMD_PER_LUN);
	uint32_t seg_max = virtio_read_device_config_4(vsc,
	    VIRTIO_SCSI_CONFIG_SEG_MAX);
	uint16_t max_target = virtio_read_device_config_2(vsc,
	    VIRTIO_SCSI_CONFIG_MAX_TARGET);

	sc->sc_seg_max = seg_max;

	for (i = 0; i < nitems(sc->sc_vqs); i++) {
		rv = virtio_alloc_vq(vsc, &sc->sc_vqs[i], i, MAXPHYS,
		    1 + howmany(MAXPHYS, NBPG), vioscsi_vq_names[i]);
		if (rv) {
			printf(": failed to allocate virtqueue %d\n", i);
			return;
		}
		sc->sc_vqs[i].vq_done = vioscsi_vq_done;
	}

	int qsize = sc->sc_vqs[2].vq_num;
	printf(": qsize %d\n", qsize);
	if (vioscsi_alloc_reqs(sc, vsc, qsize, seg_max))
		return;

	scsi_iopool_init(&sc->sc_iopool, sc, vioscsi_req_get, vioscsi_req_put);

	sc->sc_link.adapter = &vioscsi_switch;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = max_target;
	sc->sc_link.adapter_buswidth = max_target;
	sc->sc_link.openings = cmd_per_lun;
	sc->sc_link.pool = &sc->sc_iopool;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	sc->sc_scsibus = (struct scsibus *)config_found(self, &saa, scsiprint);
}

void
vioscsi_scsi_cmd(struct scsi_xfer *xs)
{
	struct vioscsi_softc *sc = xs->sc_link->adapter_softc;
	struct virtio_softc *vsc = (struct virtio_softc *)sc->sc_dev.dv_parent;
	struct vioscsi_req *vr = xs->io;
	struct virtio_scsi_req_hdr *req = &vr->vr_req;
	struct virtqueue *vq = &sc->sc_vqs[2];
	int slot = vr - sc->sc_reqs;

	DPRINTF("vioscsi_scsi_cmd: enter\n");

	// TODO(matthew): Support bidirectional SCSI commands?
	if ((xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT))
	    == (SCSI_DATA_IN | SCSI_DATA_OUT)) {
	stuffup:
		xs->error = XS_DRIVER_STUFFUP;
		xs->resid = xs->datalen;
		DPRINTF("vioscsi_scsi_cmd: stuffup\n");
		scsi_done(xs);
		return;
	}

	vr->vr_xs = xs;

	/*
	 * "The only supported format for the LUN field is: first byte set to
	 * 1, second byte set to target, third and fourth byte representing a
	 * single level LUN structure, followed by four zero bytes."
	 */
	if (xs->sc_link->target >= 256 || xs->sc_link->lun >= 16384)
		goto stuffup;
	req->lun[0] = 1;
	req->lun[1] = xs->sc_link->target;
	req->lun[2] = 0x40 | (xs->sc_link->lun >> 8);
	req->lun[3] = xs->sc_link->lun;
	memset(req->lun + 4, 0, 4);

	if ((size_t)xs->cmdlen > sizeof(req->cdb))
		goto stuffup;
	memset(req->cdb, 0, sizeof(req->cdb));
	memcpy(req->cdb, xs->cmd, xs->cmdlen);

	int isread = !!(xs->flags & SCSI_DATA_IN);

	int nsegs = 2;
	if (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
		if (bus_dmamap_load(vsc->sc_dmat, vr->vr_data,
		    xs->data, xs->datalen, NULL,
		    ((isread ? BUS_DMA_READ : BUS_DMA_WRITE) |
		     BUS_DMA_NOWAIT)))
			goto stuffup;
		nsegs += vr->vr_data->dm_nsegs;
	}

	int s = splbio();
	int r = virtio_enqueue_reserve(vq, slot, nsegs);
	splx(s);
	if (r)
		goto stuffup;

	bus_dmamap_sync(vsc->sc_dmat, vr->vr_control,
	    offsetof(struct vioscsi_req, vr_req),
	    sizeof(struct virtio_scsi_req_hdr),
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_control,
	    offsetof(struct vioscsi_req, vr_res),
            sizeof(struct virtio_scsi_res_hdr),
	    BUS_DMASYNC_PREREAD);
	if (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT))
		bus_dmamap_sync(vsc->sc_dmat, vr->vr_data, 0, xs->datalen,
		    isread ? BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

	s = splbio();
	virtio_enqueue_p(vq, slot, vr->vr_control,
	    offsetof(struct vioscsi_req, vr_req),
            sizeof(struct virtio_scsi_req_hdr),
	    1);
	if (xs->flags & SCSI_DATA_OUT)
		virtio_enqueue(vq, slot, vr->vr_data, 1);
	virtio_enqueue_p(vq, slot, vr->vr_control,
	    offsetof(struct vioscsi_req, vr_res),
            sizeof(struct virtio_scsi_res_hdr),
	    0);
	if (xs->flags & SCSI_DATA_IN)
		virtio_enqueue(vq, slot, vr->vr_data, 0);

	virtio_enqueue_commit(vsc, vq, slot, 1);

	if (ISSET(xs->flags, SCSI_POLL)) {
		DPRINTF("vioscsi_scsi_cmd: polling...\n");
		int timeout = 1000;
		do {
			vsc->sc_ops->intr(vsc);
			if (vr->vr_xs != xs)
				break;
			delay(1000);
		} while (--timeout > 0);
		if (vr->vr_xs == xs) {
			// TODO(matthew): Abort the request.
			xs->error = XS_TIMEOUT;
			xs->resid = xs->datalen;
			DPRINTF("vioscsi_scsi_cmd: polling timeout\n");
			scsi_done(xs);
		}
		DPRINTF("vioscsi_scsi_cmd: done (timeout=%d)\n", timeout);
	}
	splx(s);
}

void
vioscsi_req_done(struct vioscsi_softc *sc, struct virtio_softc *vsc,
    struct vioscsi_req *vr)
{
	struct scsi_xfer *xs = vr->vr_xs;

	DPRINTF("vioscsi_req_done: enter\n");

	int isread = !!(xs->flags & SCSI_DATA_IN);
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_control,
	    offsetof(struct vioscsi_req, vr_req),
	    sizeof(struct virtio_scsi_req_hdr),
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_control,
	    offsetof(struct vioscsi_req, vr_res),
	    sizeof(struct virtio_scsi_res_hdr),
	    BUS_DMASYNC_POSTREAD);
	if (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT))
		bus_dmamap_sync(vsc->sc_dmat, vr->vr_data, 0, xs->datalen,
		    isread ? BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

	if (vr->vr_res.response != VIRTIO_SCSI_S_OK) {
		xs->error = XS_DRIVER_STUFFUP;
		xs->resid = xs->datalen;
		DPRINTF("vioscsi_req_done: stuffup: %d\n", vr->vr_res.response);
		goto done;
	}

	size_t sense_len = MIN(sizeof(xs->sense), vr->vr_res.sense_len);
	memcpy(&xs->sense, vr->vr_res.sense, sense_len);
	xs->error = (sense_len == 0) ? XS_NOERROR : XS_SENSE;

	xs->status = vr->vr_res.status;
	xs->resid = vr->vr_res.residual;

	DPRINTF("vioscsi_req_done: done %d, %d, %zd\n", 
	    xs->error, xs->status, xs->resid);

done:
	vr->vr_xs = NULL;
	scsi_done(xs);
}

int
vioscsi_vq_done(struct virtqueue *vq)
{
	struct virtio_softc *vsc = vq->vq_owner;
	struct vioscsi_softc *sc = (struct vioscsi_softc *)vsc->sc_child;
	int ret = 0;

	DPRINTF("vioscsi_vq_done: enter\n");

	for (;;) {
		int r, s, slot;
		s = splbio();
		r = virtio_dequeue(vsc, vq, &slot, NULL);
		splx(s);
		if (r != 0)
			break;

		DPRINTF("vioscsi_vq_done: slot=%d\n", slot);
		vioscsi_req_done(sc, vsc, &sc->sc_reqs[slot]);
		ret = 1;
	}

	DPRINTF("vioscsi_vq_done: exit %d\n", ret);

	return (ret);
}

void *
vioscsi_req_get(void *cookie)
{
	struct vioscsi_softc *sc = cookie;
	struct virtio_softc *vsc = (struct virtio_softc *)sc->sc_dev.dv_parent;
	struct virtqueue *vq = &sc->sc_vqs[2];
	struct vioscsi_req *vr = NULL;
	int r, s, slot;

	s = splbio();
	if (virtio_enqueue_prep(vq, &slot) == 0)
		vr = &sc->sc_reqs[slot];
	splx(s);

	if (vr == NULL)
		goto err1;

	vr->vr_req.id = slot;
	vr->vr_req.task_attr = VIRTIO_SCSI_S_SIMPLE;

	r = bus_dmamap_create(vsc->sc_dmat,
	    offsetof(struct vioscsi_req, vr_xs), 1,
	    offsetof(struct vioscsi_req, vr_xs), 0,
	    BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW, &vr->vr_control);
	if (r != 0)
		goto err2;
	r = bus_dmamap_create(vsc->sc_dmat, MAXPHYS, sc->sc_seg_max,
	    MAXPHYS, 0, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW, &vr->vr_data);
	if (r != 0)
		goto err3;
	r = bus_dmamap_load(vsc->sc_dmat, vr->vr_control,
	    vr, offsetof(struct vioscsi_req, vr_xs), NULL,
	    BUS_DMA_NOWAIT);
	if (r != 0)
		goto err4;

	DPRINTF("vioscsi_req_get: %p, %d\n", vr, slot);

	return (vr);

err4:
	bus_dmamap_destroy(vsc->sc_dmat, vr->vr_data);
err3:
	bus_dmamap_destroy(vsc->sc_dmat, vr->vr_control);
err2:
	s = splbio();
	virtio_enqueue_abort(vq, slot);
	splx(s);
err1:
	return (NULL);
}

void
vioscsi_req_put(void *cookie, void *io)
{
	struct vioscsi_softc *sc = cookie;
	struct virtio_softc *vsc = (struct virtio_softc *)sc->sc_dev.dv_parent;
	struct virtqueue *vq = &sc->sc_vqs[2];
	struct vioscsi_req *vr = io;
	int slot = vr - sc->sc_reqs;

	DPRINTF("vioscsi_req_put: %p, %d\n", vr, slot);

	bus_dmamap_destroy(vsc->sc_dmat, vr->vr_control);
	bus_dmamap_destroy(vsc->sc_dmat, vr->vr_data);

	int s = splbio();
	virtio_dequeue_commit(vq, slot);
	splx(s);
}

int
vioscsi_alloc_reqs(struct vioscsi_softc *sc, struct virtio_softc *vsc,
    int qsize, uint32_t seg_max)
{
	size_t allocsize;
	int r, rsegs;
	void *vaddr;

	allocsize = qsize * sizeof(struct vioscsi_req);
	r = bus_dmamem_alloc(vsc->sc_dmat, allocsize, 0, 0,
	    &sc->sc_reqs_segs[0], 1, &rsegs, BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("bus_dmamem_alloc, size %zd, error %d\n",
		    allocsize, r);
		return 1;
	}
	r = bus_dmamem_map(vsc->sc_dmat, &sc->sc_reqs_segs[0], 1,
	    allocsize, (caddr_t *)&vaddr, BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("bus_dmamem_map failed, error %d\n", r);
		bus_dmamem_free(vsc->sc_dmat, &sc->sc_reqs_segs[0], 1);
		return 1;
	}
	sc->sc_reqs = vaddr;
	memset(vaddr, 0, allocsize);
	return 0;
}
