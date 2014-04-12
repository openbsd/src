/*	$OpenBSD: nvme.c,v 1.1 2014/04/12 05:06:58 dlg Exp $ */

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

#include <dev/ic/nvmevar.h>
#include <dev/ic/nvmereg.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

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

struct nvme_queue *	nvme_queue_alloc(struct nvme_softc *,
			   u_int, u_int, u_int);
void			nvme_queue_free(struct nvme_softc *,
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
	printf("%s:  dstrd %llu\n", DEVNAME(sc), NVME_CAP_DSTRD(r8));
	printf("%s:  to %llu msec\n", DEVNAME(sc), NVME_CAP_TO(r8));
	printf("%s:  ams %llu\n", DEVNAME(sc), NVME_CAP_AMS(r8));
	printf("%s:  cqr %llu\n", DEVNAME(sc), NVME_CAP_CQR(r8));
	printf("%s:  mqes %llu\n", DEVNAME(sc), NVME_CAP_MQES(r8));

	printf("%s: vs   0x%08lx\n", DEVNAME(sc), nvme_read4(sc, NVME_VS));

	r4 = nvme_read4(sc, NVME_CC);
	printf("%s: cc   0x%08lx\n", DEVNAME(sc), r4);
	printf("%s:  iocqes %u\n", DEVNAME(sc), NVME_CC_IOCQES_R(r4));
	printf("%s:  iosqes %u\n", DEVNAME(sc), NVME_CC_IOSQES_R(r4));
	printf("%s:  shn %u\n", DEVNAME(sc), NVME_CC_SHN_R(r4));
	printf("%s:  ams %u\n", DEVNAME(sc), NVME_CC_AMS_R(r4));
	printf("%s:  mps %u\n", DEVNAME(sc), NVME_CC_MPS_R(r4));
	printf("%s:  css %u\n", DEVNAME(sc), NVME_CC_CSS_R(r4));
	printf("%s:  en %u\n", DEVNAME(sc), ISSET(r4, NVME_CC_EN));
	
	printf("%s: csts 0x%08lx\n", DEVNAME(sc), nvme_read4(sc, NVME_CSTS));
	printf("%s: aqa  0x%08lx\n", DEVNAME(sc), nvme_read4(sc, NVME_AQA));
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
		    nvme_ready(sc, NVME_CSTS) != 0)
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

	nvme_dumpregs(sc);
	if (nvme_disable(sc) != 0) {
		printf("%s: unable to disable controller\n", DEVNAME(sc));
		return (1);
	}
	nvme_dumpregs(sc);

	cap = nvme_read8(sc, NVME_CAP);
	dstrd = NVME_CAP_DSTRD(cap);
	if (NVME_CAP_MPSMIN(cap) > mps)
		mps = NVME_CAP_MPSMIN(cap);
	else if (NVME_CAP_MPSMAX(cap) < mps)
		mps = NVME_CAP_MPSMAX(cap);

	sc->sc_rdy_to = NVME_CAP_TO(cap);

	sc->sc_admin_q = nvme_queue_alloc(sc, NVME_ADMIN_Q, 128, dstrd);
	if (sc->sc_admin_q == NULL) {
		printf("%s: unable to allocate admin queue\n", DEVNAME(sc));
		return (1);
	}

	sc->sc_q = nvme_queue_alloc(sc, 1, 128, dstrd);
	if (sc->sc_q == NULL) {
		printf("%s: unable to allocate queue\n", DEVNAME(sc));
		goto free_admin_q;
	}

	nvme_enable(sc, mps);
	nvme_dumpregs(sc);

	return (0);

free_admin_q:
	nvme_queue_free(sc, sc->sc_admin_q);

	return (1);
}

struct nvme_queue *
nvme_queue_alloc(struct nvme_softc *sc, u_int idx, u_int entries, u_int dstrd)
{
	struct nvme_queue *q;

	q = malloc(sizeof(*q), M_DEVBUF, M_WAITOK | M_CANFAIL);
	if (q == NULL)
		return (NULL);

	q->q_sq_dmamem = nvme_dmamem_alloc(sc, 64 * entries); /* XXX */
	if (q->q_sq_dmamem == NULL)
		goto free;

	q->q_cq_dmamem = nvme_dmamem_alloc(sc, 16 * entries); /* XXX */
	if (q->q_sq_dmamem == NULL)
		goto free_sq;

	q->q_sqtdbl = NVME_SQTDBL(idx, dstrd);
	q->q_cqhdbl = NVME_CQTDBL(idx, dstrd);
	q->q_entries = entries;
	q->q_sq_head = 0;
	q->q_cq_tail = 0;

	return (q);

free_sq:
	nvme_dmamem_free(sc, q->q_sq_dmamem);
free:
	free(q, M_DEVBUF);

	return (NULL);
}

void
nvme_queue_free(struct nvme_softc *sc, struct nvme_queue *q)
{
	nvme_dmamem_free(sc, q->q_cq_dmamem);
	nvme_dmamem_free(sc, q->q_sq_dmamem);
	free(q, M_DEVBUF);
}

int
nvme_intr(void *xsc)
{
	return (-1);
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

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &ndm->ndm_seg,
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
	free(ndm, M_DEVBUF);

	return (NULL);
}

void
nvme_dmamem_free(struct nvme_softc *sc, struct nvme_dmamem *ndm)
{
	bus_dmamap_unload(sc->sc_dmat, ndm->ndm_map);
	bus_dmamem_unmap(sc->sc_dmat, ndm->ndm_kva, ndm->ndm_size);
	bus_dmamem_free(sc->sc_dmat, &ndm->ndm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, ndm->ndm_map);
	free(ndm, M_DEVBUF);
}

