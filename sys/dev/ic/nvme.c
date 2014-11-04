/*	$OpenBSD: nvme.c,v 1.9 2014/11/04 12:41:34 dlg Exp $ */

/*
 * Copyright (c) 2014 David Gwynne <dlg@openbsd.org>
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
#include <sys/queue.h>
#include <sys/mutex.h>
#include <sys/pool.h>

#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/ic/nvmereg.h>
#include <dev/ic/nvmevar.h>

struct cfdriver nvme_cd = {
	NULL,
	"nvme",
	DV_DULL
};

int			nvme_ready(struct nvme_softc *, u_int32_t);
int			nvme_enable(struct nvme_softc *, u_int);
int			nvme_disable(struct nvme_softc *);

void			nvme_version(struct nvme_softc *, u_int32_t);
void			nvme_dumpregs(struct nvme_softc *);
int			nvme_identify(struct nvme_softc *, u_int);
void			nvme_fill_identify(struct nvme_softc *,
			    struct nvme_ccb *, void *);

int			nvme_ccbs_alloc(struct nvme_softc *, u_int);
void			nvme_ccbs_free(struct nvme_softc *);

void *			nvme_ccb_get(void *);
void			nvme_ccb_put(void *, void *);

int			nvme_poll(struct nvme_softc *, struct nvme_queue *,
			    struct nvme_ccb *,
			    void (*fill)(struct nvme_softc *,
			     struct nvme_ccb *, void *));
void			nvme_poll_fill(struct nvme_softc *,
			    struct nvme_ccb *, void *);
void			nvme_poll_done(struct nvme_softc *,
			    struct nvme_ccb *, struct nvme_cqe *);
void			nvme_empty_done(struct nvme_softc *,
			    struct nvme_ccb *, struct nvme_cqe *);

struct nvme_queue *	nvme_q_alloc(struct nvme_softc *,
			    u_int, u_int, u_int);
void			nvme_q_submit(struct nvme_softc *,
			    struct nvme_queue *, struct nvme_ccb *,
			    void (*)(struct nvme_softc *,
			     struct nvme_ccb *, void *));
int			nvme_q_complete(struct nvme_softc *,
			    struct nvme_queue *q);
void			nvme_q_free(struct nvme_softc *,
			    struct nvme_queue *);

struct nvme_dmamem *	nvme_dmamem_alloc(struct nvme_softc *, size_t);
void			nvme_dmamem_free(struct nvme_softc *,
			    struct nvme_dmamem *);

#define nvme_read4(_s, _r) \
	bus_space_read_4((_s)->sc_iot, (_s)->sc_ioh, (_r))
#define nvme_write4(_s, _r, _v) \
	bus_space_write_4((_s)->sc_iot, (_s)->sc_ioh, (_r), (_v))
#ifdef __LP64__
#define nvme_read8(_s, _r) \
	bus_space_read_8((_s)->sc_iot, (_s)->sc_ioh, (_r))
#define nvme_write8(_s, _r, _v) \
	bus_space_write_8((_s)->sc_iot, (_s)->sc_ioh, (_r), (_v))
#else /* __LP64__ */
static inline u_int64_t
nvme_read8(struct nvme_softc *sc, bus_size_t r)
{
	u_int64_t v;
	u_int32_t *a = (u_int32_t *)&v;

#if _BYTE_ORDER == _LITTLE_ENDIAN
	a[0] = nvme_read4(sc, r);
	a[1] = nvme_read4(sc, r + 4);
#else /* _BYTE_ORDER == _LITTLE_ENDIAN */
	a[1] = nvme_read4(sc, r);
	a[0] = nvme_read4(sc, r + 4);
#endif

	return (v);
}

static inline void
nvme_write8(struct nvme_softc *sc, bus_size_t r, u_int64_t v)
{
	u_int32_t *a = (u_int32_t *)&v;

#if _BYTE_ORDER == _LITTLE_ENDIAN
	nvme_write4(sc, r, a[0]);
	nvme_write4(sc, r + 4, a[1]);
#else /* _BYTE_ORDER == _LITTLE_ENDIAN */
	nvme_write4(sc, r, a[1]);
	nvme_write4(sc, r + 4, a[0]);
#endif
}
#endif /* __LP64__ */
#define nvme_barrier(_s, _r, _l, _f) \
	bus_space_barrier((_s)->sc_iot, (_s)->sc_ioh, (_r), (_l), (_f))

void
nvme_version(struct nvme_softc *sc, u_int32_t version)
{
	u_int16_t minor;

	minor = NVME_VS_MNR(version);
	minor = ((minor >> 8) * 10) + (minor & 0xff);
	printf(", NVME %d.%d", NVME_VS_MJR(version), minor);
}

void
nvme_dumpregs(struct nvme_softc *sc)
{
	u_int64_t r8;
	u_int32_t r4;

	r8 = nvme_read8(sc, NVME_CAP);
	printf("%s: cap  0x%016llx\n", DEVNAME(sc), nvme_read8(sc, NVME_CAP));
	printf("%s:  mpsmax %u (%u)\n", DEVNAME(sc),
	    (u_int)NVME_CAP_MPSMAX(r8), (1 << NVME_CAP_MPSMAX(r8)));
	printf("%s:  mpsmin %u (%u)\n", DEVNAME(sc),
	    (u_int)NVME_CAP_MPSMIN(r8), (1 << NVME_CAP_MPSMIN(r8)));
	printf("%s:  css %llu\n", DEVNAME(sc), NVME_CAP_CSS(r8));
	printf("%s:  nssrs %llu\n", DEVNAME(sc), NVME_CAP_NSSRS(r8));
	printf("%s:  dstrd %u\n", DEVNAME(sc), NVME_CAP_DSTRD(r8));
	printf("%s:  to %llu msec\n", DEVNAME(sc), NVME_CAP_TO(r8));
	printf("%s:  ams %llu\n", DEVNAME(sc), NVME_CAP_AMS(r8));
	printf("%s:  cqr %llu\n", DEVNAME(sc), NVME_CAP_CQR(r8));
	printf("%s:  mqes %llu\n", DEVNAME(sc), NVME_CAP_MQES(r8));

	printf("%s: vs   0x%04x\n", DEVNAME(sc), nvme_read4(sc, NVME_VS));

	r4 = nvme_read4(sc, NVME_CC);
	printf("%s: cc   0x%04x\n", DEVNAME(sc), r4);
	printf("%s:  iocqes %u\n", DEVNAME(sc), NVME_CC_IOCQES_R(r4));
	printf("%s:  iosqes %u\n", DEVNAME(sc), NVME_CC_IOSQES_R(r4));
	printf("%s:  shn %u\n", DEVNAME(sc), NVME_CC_SHN_R(r4));
	printf("%s:  ams %u\n", DEVNAME(sc), NVME_CC_AMS_R(r4));
	printf("%s:  mps %u\n", DEVNAME(sc), NVME_CC_MPS_R(r4));
	printf("%s:  css %u\n", DEVNAME(sc), NVME_CC_CSS_R(r4));
	printf("%s:  en %u\n", DEVNAME(sc), ISSET(r4, NVME_CC_EN));
	
	printf("%s: csts 0x%08x\n", DEVNAME(sc), nvme_read4(sc, NVME_CSTS));
	printf("%s: aqa  0x%08x\n", DEVNAME(sc), nvme_read4(sc, NVME_AQA));
	printf("%s: asq  0x%016llx\n", DEVNAME(sc), nvme_read8(sc, NVME_ASQ));
	printf("%s: acq  0x%016llx\n", DEVNAME(sc), nvme_read8(sc, NVME_ACQ));
}

int
nvme_ready(struct nvme_softc *sc, u_int32_t rdy)
{
	u_int i = 0;

	while ((nvme_read4(sc, NVME_CSTS) & NVME_CSTS_RDY) != rdy) {
		if (i++ > sc->sc_rdy_to)
			return (1);

		delay(1000);
		nvme_barrier(sc, NVME_CSTS, 4, BUS_SPACE_BARRIER_READ);
	}

	return (0);
}

int
nvme_enable(struct nvme_softc *sc, u_int mps)
{
	u_int32_t cc;

	cc = nvme_read4(sc, NVME_CC);
	if (ISSET(cc, NVME_CC_EN))
		return (nvme_ready(sc, NVME_CSTS_RDY));

	nvme_write8(sc, NVME_ASQ, NVME_DMA_DVA(sc->sc_admin_q->q_sq_dmamem));
	nvme_barrier(sc, 0, sc->sc_ios, BUS_SPACE_BARRIER_WRITE);
	nvme_write8(sc, NVME_ACQ, NVME_DMA_DVA(sc->sc_admin_q->q_cq_dmamem));
	nvme_barrier(sc, 0, sc->sc_ios, BUS_SPACE_BARRIER_WRITE);

	nvme_write4(sc, NVME_AQA, NVME_AQA_ACQS(sc->sc_admin_q->q_entries) |
	    NVME_AQA_ASQS(sc->sc_admin_q->q_entries));
	nvme_barrier(sc, 0, sc->sc_ios, BUS_SPACE_BARRIER_WRITE);

	CLR(cc, NVME_CC_IOCQES_MASK | NVME_CC_IOSQES_MASK | NVME_CC_SHN_MASK |
	    NVME_CC_AMS_MASK | NVME_CC_MPS_MASK | NVME_CC_CSS_MASK);
	SET(cc, NVME_CC_IOSQES(ffs(64) - 1) | NVME_CC_IOCQES(ffs(16) - 1));
	SET(cc, NVME_CC_SHN(NVME_CC_SHN_NONE));
	SET(cc, NVME_CC_CSS(NVME_CC_CSS_NVM));
	SET(cc, NVME_CC_AMS(NVME_CC_AMS_RR));
	SET(cc, NVME_CC_MPS(mps));
	SET(cc, NVME_CC_EN);

	nvme_write4(sc, NVME_CC, cc);
	nvme_barrier(sc, 0, sc->sc_ios,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);

	return (0);
}

int
nvme_disable(struct nvme_softc *sc)
{
	u_int32_t cc, csts;

	cc = nvme_read4(sc, NVME_CC);
	if (ISSET(cc, NVME_CC_EN)) {
		csts = nvme_read4(sc, NVME_CSTS);
		if (!ISSET(csts, NVME_CSTS_CFS) &&
		    nvme_ready(sc, NVME_CSTS_RDY) != 0)
			return (1);
	}

	CLR(cc, NVME_CC_EN);

	nvme_write4(sc, NVME_CC, cc);
	nvme_barrier(sc, 0, sc->sc_ios,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);

	return (nvme_ready(sc, 0));
}

int
nvme_attach(struct nvme_softc *sc)
{
	u_int64_t cap;
	u_int32_t reg;
	u_int dstrd;
	u_int mps = PAGE_SHIFT;

	reg = nvme_read4(sc, NVME_VS);
	if (reg == 0xffffffff) {
		printf(", invalid mapping\n");
		return (1);
	}

	nvme_version(sc, reg);
	printf("\n");

	if (nvme_disable(sc) != 0) {
		printf("%s: unable to disable controller\n", DEVNAME(sc));
		return (1);
	}

	cap = nvme_read8(sc, NVME_CAP);
	dstrd = NVME_CAP_DSTRD(cap);
	if (NVME_CAP_MPSMIN(cap) > mps)
		mps = NVME_CAP_MPSMIN(cap);
	else if (NVME_CAP_MPSMAX(cap) < mps)
		mps = NVME_CAP_MPSMAX(cap);

	sc->sc_rdy_to = NVME_CAP_TO(cap);
	sc->sc_mps = 1 << mps;
	sc->sc_mdts = MAXPHYS;
	sc->sc_max_sgl = 2;
	mtx_init(&sc->sc_ccb_mtx, IPL_BIO);
	SIMPLEQ_INIT(&sc->sc_ccb_list);
	scsi_iopool_init(&sc->sc_iopool, sc, nvme_ccb_get, nvme_ccb_put);

	sc->sc_admin_q = nvme_q_alloc(sc, NVME_ADMIN_Q, 128, dstrd);
	if (sc->sc_admin_q == NULL) {
		printf("%s: unable to allocate admin queue\n", DEVNAME(sc));
		return (1);
	}

	if (nvme_ccbs_alloc(sc, 16) != 0) {
		printf("%s: unable to allocate initial ccbs\n", DEVNAME(sc));
		goto free_admin_q;
	}

	if (nvme_enable(sc, mps) != 0) {
		printf("%s: unable to enable controller\n", DEVNAME(sc));
		goto free_ccbs;
	}

	if (nvme_identify(sc, NVME_CAP_MPSMIN(cap)) != 0) {
		printf("%s: unable to identify controller\n", DEVNAME(sc));
		goto disable;
	}

	return (0);

disable:
	nvme_disable(sc);
free_ccbs:
	nvme_ccbs_free(sc);
free_admin_q:
	nvme_q_free(sc, sc->sc_admin_q);

	return (1);
}

void
nvme_q_submit(struct nvme_softc *sc, struct nvme_queue *q, struct nvme_ccb *ccb,
    void (*fill)(struct nvme_softc *, struct nvme_ccb *, void *))
{
	struct nvme_sqe *sqe = NVME_DMA_KVA(q->q_sq_dmamem);
	u_int32_t tail;

	mtx_enter(&q->q_sq_mtx);
	tail = q->q_sq_tail;
	if (++q->q_sq_tail >= q->q_entries)
		q->q_sq_tail = 0;

	sqe += tail;

	bus_dmamap_sync(sc->sc_dmat, NVME_DMA_MAP(q->q_sq_dmamem),
	    sizeof(*sqe) * tail, sizeof(*sqe), BUS_DMASYNC_POSTWRITE);
	memset(sqe, 0, sizeof(*sqe));
	sqe->cid = ccb->ccb_id;
	(*fill)(sc, ccb, sqe);
	bus_dmamap_sync(sc->sc_dmat, NVME_DMA_MAP(q->q_sq_dmamem),
	    sizeof(*sqe) * tail, sizeof(*sqe), BUS_DMASYNC_PREWRITE);

	nvme_write4(sc, q->q_sqtdbl, q->q_sq_tail);
	mtx_leave(&q->q_sq_mtx);
}

struct nvme_poll_state {
	struct nvme_sqe s;
	struct nvme_cqe c;
};

int
nvme_poll(struct nvme_softc *sc, struct nvme_queue *q, struct nvme_ccb *ccb,
    void (*fill)(struct nvme_softc *, struct nvme_ccb *, void *))
{
	struct nvme_poll_state state;
	void (*done)(struct nvme_softc *, struct nvme_ccb *, struct nvme_cqe *);
	void *cookie;
	u_int16_t flags;

	memset(&state, 0, sizeof(state));
	state.s.cid = ccb->ccb_id;
	(*fill)(sc, ccb, &state.s);

	done = ccb->ccb_done;
	cookie = ccb->ccb_cookie;

	ccb->ccb_done = nvme_poll_done;
	ccb->ccb_cookie = &state;

	nvme_q_submit(sc, q, ccb, nvme_poll_fill);
	while (!ISSET(state.c.flags, htole16(NVME_CQE_PHASE))) {
		if (nvme_intr(sc) == 0)
			delay(10);

		/* XXX no timeout? */
	}

	ccb->ccb_cookie = cookie;
	done(sc, ccb, &state.c);

	flags = lemtoh16(&state.c.flags);

	return (NVME_CQE_SCT(flags) | NVME_CQE_SC(flags));
}

void
nvme_poll_fill(struct nvme_softc *sc, struct nvme_ccb *ccb, void *slot)
{
	struct nvme_sqe *sqe = slot;
	struct nvme_poll_state *state = ccb->ccb_cookie;

	*sqe = state->s;
}

void
nvme_poll_done(struct nvme_softc *sc, struct nvme_ccb *ccb,
    struct nvme_cqe *cqe)
{
	struct nvme_poll_state *state = ccb->ccb_cookie;

	SET(cqe->flags, htole16(NVME_CQE_PHASE));
	state->c = *cqe;
}

void
nvme_empty_done(struct nvme_softc *sc, struct nvme_ccb *ccb,
    struct nvme_cqe *cqe)
{
}

int
nvme_q_complete(struct nvme_softc *sc, struct nvme_queue *q)
{
	struct nvme_ccb *ccb;
	struct nvme_cqe *ring = NVME_DMA_KVA(q->q_cq_dmamem), *cqe;
	u_int32_t head;
	u_int16_t flags;
	int rv = 0;

	if (!mtx_enter_try(&q->q_cq_mtx))
		return (-1);

	head = q->q_cq_head;
	for (;;) {
		cqe = &ring[head];
		flags = lemtoh16(&cqe->flags);
		if ((flags & NVME_CQE_PHASE) != q->q_cq_phase)
			break;

		ccb = &sc->sc_ccbs[cqe->cid];
		ccb->ccb_done(sc, ccb, cqe);

		if (++head >= q->q_entries) {
			head = 0;
			q->q_cq_phase ^= NVME_CQE_PHASE;
		}

		rv = 1;
	}

	if (rv)
		nvme_write4(sc, q->q_cqhdbl, q->q_cq_head = head);
	mtx_leave(&q->q_cq_mtx);

	return (rv);
}

int
nvme_identify(struct nvme_softc *sc, u_int mps)
{
	char sn[41], mn[81], fr[17];
	struct nvm_identify_controller *identify;
	struct nvme_dmamem *mem;
	struct nvme_ccb *ccb;
	u_int mdts;
	int rv = 1;

	ccb = nvme_ccb_get(sc);
	if (ccb == NULL)
		panic("nvme_identify: nvme_ccb_get returned NULL");

	mem = nvme_dmamem_alloc(sc, sizeof(*identify));
	if (mem == NULL)
		return (1);

	identify = NVME_DMA_KVA(mem);

	ccb->ccb_done = nvme_empty_done;
	ccb->ccb_cookie = mem;

	bus_dmamap_sync(sc->sc_dmat, NVME_DMA_MAP(mem),
	    0, sizeof(*identify), BUS_DMASYNC_PREREAD);
	rv = nvme_poll(sc, sc->sc_admin_q, ccb, nvme_fill_identify);
	bus_dmamap_sync(sc->sc_dmat, NVME_DMA_MAP(mem),
	    0, sizeof(*identify), BUS_DMASYNC_POSTREAD);

	nvme_ccb_put(sc, ccb);

	if (rv != 0)
		goto done;

	scsi_strvis(sn, identify->sn, sizeof(identify->sn));
	scsi_strvis(mn, identify->mn, sizeof(identify->mn));
	scsi_strvis(fr, identify->fr, sizeof(identify->fr));

	printf("%s: %s, firmware %s, serial %s\n", DEVNAME(sc), mn, fr, sn);

	if (identify->mdts > 0) {
		mdts = (1 << identify->mdts) * (1 << mps);
		if (mdts < sc->sc_mdts)
			sc->sc_mdts = mdts;
	}

done:
	nvme_dmamem_free(sc, mem);

	return (rv);
}

void
nvme_fill_identify(struct nvme_softc *sc, struct nvme_ccb *ccb, void *slot)
{
	struct nvme_sqe *sqe = slot;
	struct nvme_dmamem *mem = ccb->ccb_cookie;

	sqe->opcode = NVM_ADMIN_IDENTIFY;
	htolem64(&sqe->entry.prp[0], NVME_DMA_DVA(mem));
	htolem32(&sqe->cdw10, 1);
}

int
nvme_ccbs_alloc(struct nvme_softc *sc, u_int nccbs)
{
	struct nvme_ccb *ccb;
	u_int i;

	sc->sc_ccbs = mallocarray(nccbs, sizeof(*ccb), M_DEVBUF,
	    M_WAITOK | M_CANFAIL);
	if (sc->sc_ccbs == NULL)
		return (1);

	for (i = 0; i < nccbs; i++) {
		ccb = &sc->sc_ccbs[i];

		if (bus_dmamap_create(sc->sc_dmat, sc->sc_mdts, sc->sc_max_sgl,
		    sc->sc_mdts, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &ccb->ccb_dmamap) != 0)
			goto free_maps;

		ccb->ccb_id = i;
		SIMPLEQ_INSERT_TAIL(&sc->sc_ccb_list, ccb, ccb_entry);
	}

	return (0);

free_maps:
	nvme_ccbs_free(sc);
	return (1);
}

void *
nvme_ccb_get(void *cookie)
{
	struct nvme_softc *sc = cookie;
	struct nvme_ccb *ccb;

	mtx_enter(&sc->sc_ccb_mtx);
	ccb = SIMPLEQ_FIRST(&sc->sc_ccb_list);
	if (ccb != NULL)
		SIMPLEQ_REMOVE_HEAD(&sc->sc_ccb_list, ccb_entry);
	mtx_leave(&sc->sc_ccb_mtx);

	return (ccb);
}

void
nvme_ccb_put(void *cookie, void *io)
{
	struct nvme_softc *sc = cookie;
	struct nvme_ccb *ccb = io;

	mtx_enter(&sc->sc_ccb_mtx);
	SIMPLEQ_INSERT_HEAD(&sc->sc_ccb_list, ccb, ccb_entry);
	mtx_leave(&sc->sc_ccb_mtx);
}

void
nvme_ccbs_free(struct nvme_softc *sc)
{
	struct nvme_ccb *ccb;

	while ((ccb = SIMPLEQ_FIRST(&sc->sc_ccb_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_ccb_list, ccb_entry);
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);
	}

	free(sc->sc_ccbs, M_DEVBUF, 0);
}

struct nvme_queue *
nvme_q_alloc(struct nvme_softc *sc, u_int idx, u_int entries, u_int dstrd)
{
	struct nvme_queue *q;

	q = malloc(sizeof(*q), M_DEVBUF, M_WAITOK | M_CANFAIL);
	if (q == NULL)
		return (NULL);

	q->q_sq_dmamem = nvme_dmamem_alloc(sc,
	    sizeof(struct nvme_sqe *) * entries);
	if (q->q_sq_dmamem == NULL)
		goto free;

	q->q_cq_dmamem = nvme_dmamem_alloc(sc,
	    sizeof(struct nvme_cqe *) * entries);
	if (q->q_sq_dmamem == NULL)
		goto free_sq;

	memset(NVME_DMA_KVA(q->q_sq_dmamem), 0, NVME_DMA_LEN(q->q_sq_dmamem));
	memset(NVME_DMA_KVA(q->q_cq_dmamem), 0, NVME_DMA_LEN(q->q_cq_dmamem));

	mtx_init(&q->q_sq_mtx, IPL_BIO);
	mtx_init(&q->q_cq_mtx, IPL_BIO);
	q->q_sqtdbl = NVME_SQTDBL(idx, dstrd);
	q->q_cqhdbl = NVME_CQHDBL(idx, dstrd);
	q->q_entries = entries;
	q->q_sq_tail = 0;
	q->q_cq_head = 0;
	q->q_cq_phase = NVME_CQE_PHASE;

	bus_dmamap_sync(sc->sc_dmat, NVME_DMA_MAP(q->q_sq_dmamem),
	    0, NVME_DMA_LEN(q->q_sq_dmamem), BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, NVME_DMA_MAP(q->q_cq_dmamem),
	    0, NVME_DMA_LEN(q->q_cq_dmamem), BUS_DMASYNC_PREREAD);

	return (q);

free_sq:
	nvme_dmamem_free(sc, q->q_sq_dmamem);
free:
	free(q, M_DEVBUF, 0);

	return (NULL);
}

void
nvme_q_free(struct nvme_softc *sc, struct nvme_queue *q)
{
	bus_dmamap_sync(sc->sc_dmat, NVME_DMA_MAP(q->q_cq_dmamem),
	    0, NVME_DMA_LEN(q->q_cq_dmamem), BUS_DMASYNC_POSTREAD);
	bus_dmamap_sync(sc->sc_dmat, NVME_DMA_MAP(q->q_sq_dmamem),
	    0, NVME_DMA_LEN(q->q_sq_dmamem), BUS_DMASYNC_POSTWRITE);
	nvme_dmamem_free(sc, q->q_cq_dmamem);
	nvme_dmamem_free(sc, q->q_sq_dmamem);
	free(q, M_DEVBUF, 0);
}

int
nvme_intr(void *xsc)
{
	struct nvme_softc *sc = xsc;

	return (nvme_q_complete(sc, sc->sc_admin_q));
}

struct nvme_dmamem *
nvme_dmamem_alloc(struct nvme_softc *sc, size_t size)
{
	struct nvme_dmamem *ndm;
	int nsegs;

	ndm = malloc(sizeof(*ndm), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ndm == NULL)
		return (NULL);

	ndm->ndm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &ndm->ndm_map) != 0)
		goto ndmfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, sc->sc_mps, 0, &ndm->ndm_seg,
	    1, &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &ndm->ndm_seg, nsegs, size,
	    &ndm->ndm_kva, BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, ndm->ndm_map, ndm->ndm_kva, size,
	    NULL, BUS_DMA_NOWAIT) != 0)
		goto unmap;

	return (ndm);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, ndm->ndm_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &ndm->ndm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, ndm->ndm_map);
ndmfree:
	free(ndm, M_DEVBUF, 0);

	return (NULL);
}

void
nvme_dmamem_free(struct nvme_softc *sc, struct nvme_dmamem *ndm)
{
	bus_dmamap_unload(sc->sc_dmat, ndm->ndm_map);
	bus_dmamem_unmap(sc->sc_dmat, ndm->ndm_kva, ndm->ndm_size);
	bus_dmamem_free(sc->sc_dmat, &ndm->ndm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, ndm->ndm_map);
	free(ndm, M_DEVBUF, 0);
}

