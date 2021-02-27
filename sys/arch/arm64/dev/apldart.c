/*	$OpenBSD: apldart.c,v 1.1 2021/02/27 16:19:14 kettenis Exp $	*/
/*
 * Copyright (c) 2021 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/extent.h>
#include <sys/malloc.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

/*
 * This driver is based on preliminary device tree bindings and will
 * almost certainly need changes once the official bindings land in
 * mainline Linux.  Support for these preliminary bindings will be
 * dropped as soon as official bindings are available.
 *
 * This driver largely ignores stream IDs and simply uses a single
 * translation table for all the devices that it serves.  This is good
 * enough for the PCIe host bridge that serves the on-board devices on
 * the current generation Apple Silicon Macs as these only have a
 * single PCIe device behind each DART.
 */

#define DART_TLB_OP		0x0020
#define  DART_TLB_OP_FLUSH	(1 << 20)
#define  DART_TLB_OP_BUSY	(1 << 2)
#define DART_TLB_OP_SIDMASK	0x0034
#define DART_CONFIG(sid)	(0x0100 + 4 *(sid))
#define  DART_CONFIG_TXEN	(1 << 7)
#define DART_TTBR(sid, idx)	(0x0200 + 16 * (sid) + 4 * (idx))
#define  DART_TTBR_VALID	(1U << 31)
#define  DART_TTBR_SHIFT	12

#define DART_PAGE_SIZE		16384
#define DART_PAGE_MASK		(DART_PAGE_SIZE - 1)

#define DART_L1_TABLE		0xb
#define DART_L2_INVAL		0x0
#define DART_L2_PAGE		0x3

inline paddr_t
apldart_round_page(paddr_t pa)
{
	return ((pa + DART_PAGE_MASK) & ~DART_PAGE_MASK);
}

inline paddr_t
apldart_trunc_page(paddr_t pa)
{
	return (pa & ~DART_PAGE_MASK);
}

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct apldart_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_dma_tag_t		sc_dmat;

	uint32_t		sc_sid_mask;
	int			sc_nsid;

	bus_addr_t		sc_dvabase;
	bus_addr_t		sc_dvaend;
	struct extent		*sc_dvamap;
	struct mutex		sc_dvamap_mtx;

	struct apldart_dmamem	*sc_l1;
	struct apldart_dmamem	**sc_l2;

	struct machine_bus_dma_tag sc_bus_dmat;
	struct iommu_device	sc_id;
};

struct apldart_map_state {
	struct extent_region	ams_er;
	bus_addr_t		ams_dva;
	bus_size_t		ams_len;
};

struct apldart_dmamem {
	bus_dmamap_t		adm_map;
	bus_dma_segment_t	adm_seg;
	size_t			adm_size;
	caddr_t			adm_kva;
};

#define APLDART_DMA_MAP(_adm)	((_adm)->adm_map)
#define APLDART_DMA_LEN(_adm)	((_adm)->adm_size)
#define APLDART_DMA_DVA(_adm)	((_adm)->adm_map->dm_segs[0].ds_addr)
#define APLDART_DMA_KVA(_adm)	((void *)(_adm)->adm_kva)

struct apldart_dmamem *apldart_dmamem_alloc(bus_dma_tag_t, bus_size_t,
	    bus_size_t);
void	apldart_dmamem_free(bus_dma_tag_t, struct apldart_dmamem *);

int	apldart_match(struct device *, void *, void *);
void	apldart_attach(struct device *, struct device *, void *);

struct cfattach	apldart_ca = {
	sizeof (struct apldart_softc), apldart_match, apldart_attach
};

struct cfdriver apldart_cd = {
	NULL, "apldart", DV_DULL
};

bus_dma_tag_t apldart_map(void *, uint32_t *, bus_dma_tag_t);
int	apldart_intr(void *);

void	apldart_flush_tlb(struct apldart_softc *);
int	apldart_load_map(struct apldart_softc *, bus_dmamap_t);
void	apldart_unload_map(struct apldart_softc *, bus_dmamap_t);

int	apldart_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t boundary, int, bus_dmamap_t *);
void	apldart_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
int	apldart_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int);
int	apldart_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int);
int	apldart_dmamap_load_uio(bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int);
int	apldart_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int);
void	apldart_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);

int
apldart_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "apple,dart-m1");
}

void
apldart_attach(struct device *parent, struct device *self, void *aux)
{
	struct apldart_softc *sc = (struct apldart_softc *)self;
	struct fdt_attach_args *faa = aux;
	paddr_t pa;
	volatile uint64_t *l1;
	int ntte, nl1, nl2;
	int sid, idx;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_dmat = faa->fa_dmat;

	printf("\n");

	if (OF_getproplen(faa->fa_node, "pcie-dart") != 0)
		return;

	sc->sc_sid_mask = OF_getpropint(faa->fa_node, "sid-mask", 0xffff);
	sc->sc_nsid = fls(sc->sc_sid_mask);

	/* Default aperture for PCIe DART. */
	sc->sc_dvabase = 0x00100000UL;
	sc->sc_dvaend = 0x3fefffffUL;

	/* Disable translations. */
	for (sid = 0; sid < sc->sc_nsid; sid++)
		HWRITE4(sc, DART_CONFIG(sid), 0);

	/* Remove page tables. */
	for (sid = 0; sid < sc->sc_nsid; sid++) {
		for (idx = 0; idx < 4; idx++)
			HWRITE4(sc, DART_TTBR(sid, idx), 0);
	}
	apldart_flush_tlb(sc);

	/*
	 * Build translation tables.  We pre-allocate the translation
	 * tables for the entire aperture such that we don't have to
	 * worry about growing them in an mpsafe manner later.
	 */

	ntte = howmany(sc->sc_dvaend, DART_PAGE_SIZE);
	nl2 = howmany(ntte, DART_PAGE_SIZE / sizeof(uint64_t));
	nl1 = howmany(nl2, DART_PAGE_SIZE / sizeof(uint64_t));

	sc->sc_l1 = apldart_dmamem_alloc(sc->sc_dmat,
	    nl1 * DART_PAGE_SIZE, DART_PAGE_SIZE);
	sc->sc_l2 = mallocarray(nl2, sizeof(*sc->sc_l2),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	l1 = APLDART_DMA_KVA(sc->sc_l1);
	for (idx = 0; idx < nl2; idx++) {
		sc->sc_l2[idx] = apldart_dmamem_alloc(sc->sc_dmat,
		    DART_PAGE_SIZE, DART_PAGE_SIZE);
		l1[idx] = APLDART_DMA_DVA(sc->sc_l2[idx]) | DART_L1_TABLE;
	}

	/* Install page tables. */
	for (sid = 0; sid < sc->sc_nsid; sid++) {
		pa = APLDART_DMA_DVA(sc->sc_l1);
		for (idx = 0; idx < nl1; idx++) {
			HWRITE4(sc, DART_TTBR(sid, idx),
			    (pa >> DART_TTBR_SHIFT) | DART_TTBR_VALID);
			pa += DART_PAGE_SIZE;
		}
	}
	apldart_flush_tlb(sc);

	/* Enable translations. */
	for (sid = 0; sid < sc->sc_nsid; sid++)
		HWRITE4(sc, DART_CONFIG(sid), DART_CONFIG_TXEN);

	fdt_intr_establish(faa->fa_node, IPL_NET, apldart_intr,
	    sc, sc->sc_dev.dv_xname);

	sc->sc_dvamap = extent_create(sc->sc_dev.dv_xname,
	    sc->sc_dvabase, sc->sc_dvaend, M_DEVBUF,
	    NULL, 0, EX_NOCOALESCE);
	mtx_init(&sc->sc_dvamap_mtx, IPL_HIGH);

	memcpy(&sc->sc_bus_dmat, sc->sc_dmat, sizeof(sc->sc_bus_dmat));
	sc->sc_bus_dmat._cookie = sc;
	sc->sc_bus_dmat._dmamap_create = apldart_dmamap_create;
	sc->sc_bus_dmat._dmamap_destroy = apldart_dmamap_destroy;
	sc->sc_bus_dmat._dmamap_load = apldart_dmamap_load;
	sc->sc_bus_dmat._dmamap_load_mbuf = apldart_dmamap_load_mbuf;
	sc->sc_bus_dmat._dmamap_load_uio = apldart_dmamap_load_uio;
	sc->sc_bus_dmat._dmamap_load_raw = apldart_dmamap_load_raw;
	sc->sc_bus_dmat._dmamap_unload = apldart_dmamap_unload;
	sc->sc_bus_dmat._flags |= BUS_DMA_COHERENT;

	sc->sc_id.id_node = faa->fa_node;
	sc->sc_id.id_cookie = sc;
	sc->sc_id.id_map = apldart_map;
	iommu_device_register(&sc->sc_id);
}

bus_dma_tag_t
apldart_map(void *cookie, uint32_t *cells, bus_dma_tag_t dmat)
{
	struct apldart_softc *sc = cookie;

	return &sc->sc_bus_dmat;
}

int
apldart_intr(void *arg)
{
	struct apldart_softc *sc = arg;

	panic("%s: %s", sc->sc_dev.dv_xname, __func__);
}

void
apldart_flush_tlb(struct apldart_softc *sc)
{
	__asm volatile ("dsb sy" ::: "memory");

	HWRITE4(sc, DART_TLB_OP_SIDMASK, sc->sc_sid_mask);
	HWRITE4(sc, DART_TLB_OP, DART_TLB_OP_FLUSH);
	while (HREAD4(sc, DART_TLB_OP) & DART_TLB_OP_BUSY)
		CPU_BUSY_CYCLE();
}

volatile uint64_t *
apldart_lookup_tte(struct apldart_softc *sc, bus_addr_t dva)
{
	int idx = dva / DART_PAGE_SIZE;
	int l2_idx = idx / (DART_PAGE_SIZE / sizeof(uint64_t));
	int tte_idx = idx % (DART_PAGE_SIZE / sizeof(uint64_t));
	volatile uint64_t *l2;

	l2 = APLDART_DMA_KVA(sc->sc_l2[l2_idx]);
	return &l2[tte_idx];
}

int
apldart_load_map(struct apldart_softc *sc, bus_dmamap_t map)
{
	struct apldart_map_state *ams = map->_dm_cookie;
	volatile uint64_t *tte;
	int seg, error;

	/* For each segment. */
	for (seg = 0; seg < map->dm_nsegs; seg++) {
		paddr_t pa = map->dm_segs[seg]._ds_paddr;
		psize_t off = pa - apldart_trunc_page(pa);
		u_long len, dva;

		len = apldart_round_page(map->dm_segs[seg].ds_len + off);

		mtx_enter(&sc->sc_dvamap_mtx);
		error = extent_alloc_with_descr(sc->sc_dvamap, len,
		    DART_PAGE_SIZE, 0, 0, EX_NOWAIT, &ams[seg].ams_er, &dva);
		mtx_leave(&sc->sc_dvamap_mtx);
		if (error) {
			apldart_unload_map(sc, map);
			return error;
		}

		ams[seg].ams_dva = dva;
		ams[seg].ams_len = len;

		map->dm_segs[seg].ds_addr = dva + off;

		pa = apldart_trunc_page(pa);
		while (len > 0) {
			tte = apldart_lookup_tte(sc, dva);
			*tte = pa | DART_L2_PAGE;

			pa += DART_PAGE_SIZE;
			dva += DART_PAGE_SIZE;
			len -= DART_PAGE_SIZE;
		}
	}

	apldart_flush_tlb(sc);

	return 0;
}

void
apldart_unload_map(struct apldart_softc *sc, bus_dmamap_t map)
{
	struct apldart_map_state *ams = map->_dm_cookie;
	volatile uint64_t *tte;
	int seg, error;

	/* For each segment. */
	for (seg = 0; seg < map->dm_nsegs; seg++) {
		u_long len, dva;

		if (ams[seg].ams_len == 0)
			continue;

		dva = ams[seg].ams_dva;
		len = ams[seg].ams_len;

		while (len > 0) {
			tte = apldart_lookup_tte(sc, dva);
			*tte = DART_L2_INVAL;

			dva += DART_PAGE_SIZE;
			len -= DART_PAGE_SIZE;
		}

		mtx_enter(&sc->sc_dvamap_mtx);
		error = extent_free(sc->sc_dvamap, ams[seg].ams_dva,
		    ams[seg].ams_len, EX_NOWAIT);
		mtx_leave(&sc->sc_dvamap_mtx);

		KASSERT(error == 0);

		ams[seg].ams_dva = 0;
		ams[seg].ams_len = 0;
	}

	apldart_flush_tlb(sc);
}

int
apldart_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamap)
{
	struct apldart_softc *sc = t->_cookie;
	struct apldart_map_state *ams;
	bus_dmamap_t map;
	int error;

	error = sc->sc_dmat->_dmamap_create(sc->sc_dmat, size, nsegments,
	    maxsegsz, boundary, flags, &map);
	if (error)
		return error;

	ams = mallocarray(map->_dm_segcnt, sizeof(*ams), M_DEVBUF,
	    (flags & BUS_DMA_NOWAIT) ? (M_NOWAIT|M_ZERO) : (M_WAITOK|M_ZERO));
	if (ams == NULL) {
		sc->sc_dmat->_dmamap_destroy(sc->sc_dmat, map);
		return ENOMEM;
	}

	map->_dm_cookie = ams;
	*dmamap = map;
	return 0;
}

void
apldart_dmamap_destroy(bus_dma_tag_t t, bus_dmamap_t map)
{
	struct apldart_softc *sc = t->_cookie;
	struct apldart_map_state *ams = map->_dm_cookie;

	free(ams, M_DEVBUF, map->_dm_segcnt * sizeof(*ams));
	sc->sc_dmat->_dmamap_destroy(sc->sc_dmat, map);
}

int
apldart_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    size_t buflen, struct proc *p, int flags)
{
	struct apldart_softc *sc = t->_cookie;
	int error;

	error = sc->sc_dmat->_dmamap_load(sc->sc_dmat, map,
	    buf, buflen, p, flags);
	if (error)
		return error;

	error = apldart_load_map(sc, map);
	if (error)
		sc->sc_dmat->_dmamap_unload(sc->sc_dmat, map);

	return error;
}

int
apldart_dmamap_load_mbuf(bus_dma_tag_t t, bus_dmamap_t map,
    struct mbuf *m, int flags)
{
	struct apldart_softc *sc = t->_cookie;
	int error;

	error = sc->sc_dmat->_dmamap_load_mbuf(sc->sc_dmat, map,
	    m, flags);
	if (error)
		return error;

	error = apldart_load_map(sc, map);
	if (error)
		sc->sc_dmat->_dmamap_unload(sc->sc_dmat, map);

	return error;
}

int
apldart_dmamap_load_uio(bus_dma_tag_t t, bus_dmamap_t map,
    struct uio *uio, int flags)
{
	struct apldart_softc *sc = t->_cookie;
	int error;

	error = sc->sc_dmat->_dmamap_load_uio(sc->sc_dmat, map,
	    uio, flags);
	if (error)
		return error;

	error = apldart_load_map(sc, map);
	if (error)
		sc->sc_dmat->_dmamap_unload(sc->sc_dmat, map);

	return error;
}

int
apldart_dmamap_load_raw(bus_dma_tag_t t, bus_dmamap_t map,
    bus_dma_segment_t *segs, int nsegs, bus_size_t size, int flags)
{
	struct apldart_softc *sc = t->_cookie;
	int error;

	error = sc->sc_dmat->_dmamap_load_raw(sc->sc_dmat, map,
	     segs, nsegs, size, flags);
	if (error)
		return error;

	error = apldart_load_map(sc, map);
	if (error)
		sc->sc_dmat->_dmamap_unload(sc->sc_dmat, map);

	return error;
}

void
apldart_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	struct apldart_softc *sc = t->_cookie;

	apldart_unload_map(sc, map);
	sc->sc_dmat->_dmamap_unload(sc->sc_dmat, map);
}

struct apldart_dmamem *
apldart_dmamem_alloc(bus_dma_tag_t dmat, bus_size_t size, bus_size_t align)
{
	struct apldart_dmamem *adm;
	int nsegs;

	adm = malloc(sizeof(*adm), M_DEVBUF, M_WAITOK | M_ZERO);
	adm->adm_size = size;

	if (bus_dmamap_create(dmat, size, 1, size, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW, &adm->adm_map) != 0)
		goto admfree;

	if (bus_dmamem_alloc(dmat, size, align, 0, &adm->adm_seg, 1,
	    &nsegs, BUS_DMA_WAITOK | BUS_DMA_ZERO) != 0)
		goto destroy;

	if (bus_dmamem_map(dmat, &adm->adm_seg, nsegs, size,
	    &adm->adm_kva, BUS_DMA_WAITOK | BUS_DMA_NOCACHE) != 0)
		goto free;

	if (bus_dmamap_load_raw(dmat, adm->adm_map, &adm->adm_seg,
	    nsegs, size, BUS_DMA_WAITOK) != 0)
		goto unmap;

	return adm;

unmap:
	bus_dmamem_unmap(dmat, adm->adm_kva, size);
free:
	bus_dmamem_free(dmat, &adm->adm_seg, 1);
destroy:
	bus_dmamap_destroy(dmat, adm->adm_map);
admfree:
	free(adm, M_DEVBUF, sizeof(*adm));

	return NULL;
}

void
apldart_dmamem_free(bus_dma_tag_t dmat, struct apldart_dmamem *adm)
{
	bus_dmamem_unmap(dmat, adm->adm_kva, adm->adm_size);
	bus_dmamem_free(dmat, &adm->adm_seg, 1);
	bus_dmamap_destroy(dmat, adm->adm_map);
	free(adm, M_DEVBUF, sizeof(*adm));
}
