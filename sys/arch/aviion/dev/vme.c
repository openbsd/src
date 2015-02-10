/*	$OpenBSD: vme.c,v 1.18 2015/02/10 22:42:35 miod Exp $	*/
/*
 * Copyright (c) 2006, 2007, 2010 Miodrag Vallat.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/uio.h>

#include <machine/board.h>
#include <machine/bus.h>
#include <machine/conf.h>

#include <uvm/uvm_extern.h>

#include <aviion/dev/vmevar.h>

#include <machine/avcommon.h>

struct vme_softc {
	struct device	sc_dev;

	struct extent	*sc_ext_a16;
	struct extent	*sc_ext_a24;
	struct extent	*sc_ext_a32;

	const struct vme_range *sc_ranges;
};

int	vmematch(struct device *, void *, void *);
void	vmeattach(struct device *, struct device *, void *);

const struct cfattach vme_ca = {
	sizeof(struct vme_softc), vmematch, vmeattach
};

struct cfdriver vme_cd = {
	NULL, "vme", DV_DULL
};

/* minor device number encoding */
#define	AWIDTH_FIELD(minor)	(minor & 0x0f)
#define	AWIDTH(w)		((w) << 3)
#define	DWIDTH_FIELD(minor)	((minor & 0xf0) >> 4)
#define	DWIDTH(w)		((w) << 3)

uint16_t vme_d8_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
void	vme_d8_read_raw_2(bus_space_tag_t, bus_space_handle_t,
	    bus_addr_t, uint8_t *, bus_size_t);
void	vme_d8_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    uint16_t);
void	vme_d8_write_raw_2(bus_space_tag_t, bus_space_handle_t,
	    bus_addr_t, const uint8_t *, bus_size_t);
uint32_t vme_d8_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
void	vme_d8_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    uint32_t);
void	vme_d8_read_raw_4(bus_space_tag_t, bus_space_handle_t,
	    bus_addr_t, uint8_t *, bus_size_t);
void	vme_d8_write_raw_4(bus_space_tag_t, bus_space_handle_t,
	    bus_addr_t, const uint8_t *, bus_size_t);

uint32_t vme_d16_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
void	vme_d16_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    uint32_t);
void	vme_d16_read_raw_4(bus_space_tag_t, bus_space_handle_t,
	    bus_addr_t, uint8_t *, bus_size_t);
void	vme_d16_write_raw_4(bus_space_tag_t, bus_space_handle_t,
	    bus_addr_t, const uint8_t *, bus_size_t);

int	vme_a16_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
void	vme_a16_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	vme_a24_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
void	vme_a24_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	vme_a32_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
void	vme_a32_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	vme_subregion(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    bus_size_t, bus_space_handle_t *);
void *	vme_vaddr(bus_space_tag_t, bus_space_handle_t);

int	vme_map(struct vme_softc *, struct extent *, u_int,
	    bus_addr_t, bus_size_t, int, vaddr_t *);
int	vme_map_r(const struct vme_range *, paddr_t, psize_t, int, vm_prot_t,
	    vaddr_t *);
void	vme_unmap(struct vme_softc *, struct extent *, u_int,
	    vaddr_t, paddr_t, bus_size_t);
int	vmeprint(void *, const char *);
int	vmescan(struct device *, void *, void *);

int	vmerw(struct vme_softc *, int, int, struct uio *, int);

int
vmematch(struct device *parent, void *vcf, void *aux)
{
	return (platform->get_vme_ranges() != NULL && vme_cd.cd_ndevs == 0);
}

void
vmeattach(struct device *parent, struct device *self, void *aux)
{
	struct vme_softc *sc = (struct vme_softc *)self;
	const struct vme_range *r;
	const char *fmt;
	u_int32_t ucsr;
	int i;

	/*
	 * Set up interrupt handlers.
	 */
	for (i = 0; i < NVMEINTR; i++)
		SLIST_INIT(&vmeintr_handlers[i]);

	/*
	 * Initialize extents
	 */
	sc->sc_ext_a16 = extent_create("vme a16", 0, 1 << (16 - PAGE_SHIFT),
	    M_DEVBUF, NULL, 0, EX_NOWAIT);
	if (sc->sc_ext_a16 == NULL)
		goto out1;
	sc->sc_ext_a24 = extent_create("vme a24", 0, 1 << (24 - PAGE_SHIFT),
	    M_DEVBUF, NULL, 0, EX_NOWAIT);
	if (sc->sc_ext_a24 == NULL)
		goto out2;
	sc->sc_ext_a32 = extent_create("vme a32", 0, 1 << (32 - PAGE_SHIFT),
	    M_DEVBUF, NULL, 0, EX_NOWAIT);
	if (sc->sc_ext_a32 == NULL)
		goto out3;

	/*
	 * Force a reasonable timeout for VME data transfers.
	 * We can not disable this, this would cause autoconf to hang
	 * on the first missing device we'll probe.
	 */
	ucsr = *(volatile u_int32_t*)AV_UCSR;
	ucsr = (ucsr & ~VTOSELBITS) | VTO128US;
	*(volatile u_int32_t *)AV_UCSR = ucsr;

	/*
	 * Clear EXTAD to allow VME A24 devices to access the first 16MB
	 * of memory.
	 */
	*(volatile u_int32_t *)AV_EXTAD = 0x00000000;

	/*
	 * Use supervisor data address modifiers for VME accesses.
	 */
	*(volatile u_int32_t *)AV_EXTAM = 0x0d;

	sc->sc_ranges = platform->get_vme_ranges();
	printf("\n");

	/*
	 * Display VME ranges.
	 */
	for (r = sc->sc_ranges; r->vr_width != 0; r++) {
		switch (r->vr_width) {
		default:
		case VME_A32:
			fmt = "%s: A32 %08x-%08x\n";
			break;
		case VME_A24:
			fmt = "%s: A24 %06x-%06x\n";
			break;
		case VME_A16:
			fmt = "%s: A16 %04x-%04x\n";
			break;
		}
		printf(fmt, self->dv_xname, r->vr_start, r->vr_end);
	}

	/* scan for child devices */
	config_search(vmescan, self, aux);
	return;

out3:
	extent_destroy(sc->sc_ext_a24);
out2:
	extent_destroy(sc->sc_ext_a16);
out1:
	printf(": can't allocate memory\n");
}

int
vmescan(struct device *parent, void *vcf, void *aux)
{
	struct cfdata *cf = vcf;
	struct vme_attach_args vaa;

	bzero(&vaa, sizeof vaa);
	vaa.vaa_addr_a16 = (vme_addr_t)cf->cf_loc[0];
	vaa.vaa_addr_a24 = (vme_addr_t)cf->cf_loc[1];
	vaa.vaa_addr_a32 = (vme_addr_t)cf->cf_loc[2];
	vaa.vaa_ipl = (u_int)cf->cf_loc[3];

	if ((*cf->cf_attach->ca_match)(parent, cf, &vaa) == 0)
		return 0;

	config_attach(parent, cf, &vaa, vmeprint);
	return 1;
}

int
vmeprint(void *aux, const char *pnp)
{
	struct vme_attach_args *vaa = aux;

	if (vaa->vaa_addr_a16 != (vme_addr_t)-1)
		printf(" a16 0x%04x", vaa->vaa_addr_a16);
	if (vaa->vaa_addr_a24 != (vme_addr_t)-1)
		printf(" a24 0x%06x", vaa->vaa_addr_a24);
	if (vaa->vaa_addr_a32 != (vme_addr_t)-1)
		printf(" a32 0x%08x", vaa->vaa_addr_a32);
	if (vaa->vaa_ipl != (u_int)-1)
		printf(" ipl %u", vaa->vaa_ipl);

	return UNCONF;
}

/*
 * Interrupt related code
 */

intrhand_t vmeintr_handlers[NVMEINTR];

int
vmeintr_allocate(u_int count, int flags, int ipl, u_int *array)
{
	u_int vec, v;
	struct intrhand *ih;

	if (count > 1 && ISSET(flags, VMEINTR_CONTIGUOUS)) {
		/*
		 * Try to find a range of count unused vectors first.
		 * If there isn't, it is not possible to provide exclusive
		 * contiguous vectors.
		 */
		for (vec = 0; vec <= NVMEINTR - count; vec++) {
			for (v = count; v != 0; v--)
				if (!SLIST_EMPTY(&vmeintr_handlers[vec + v - 1]))
					break;

			if (v == 0) {
				for (v = 0; v < count; v++)
					*array++ = vec++;
				return (0);
			}
		}
		if (ISSET(flags, VMEINTR_EXCLUSIVE))
			return EPERM;

		/*
		 * Try to find a range of count contiguous vectors,
		 * sharing the level we intend to register at. If there
		 * isn't, it is not possible to provide shared contiguous
		 * vectors.
		 */
		for (vec = 0; vec <= NVMEINTR - count; vec++) {
			for (v = count; v != 0; v--) {
				ih = SLIST_FIRST(&vmeintr_handlers[vec + v - 1]);
				if (ih == NULL)
					continue;
				if (ih->ih_ipl != ipl ||
				    ISSET(ih->ih_flags, INTR_EXCLUSIVE))
					break;
			}

			if (v == 0) {
				for (v = 0; v < count; v++)
					*array++ = vec++;
				return 0;
			}
		}
		return EPERM;
	}

	/*
	 * Pick as many unused vectors as possible.
	 */
	for (vec = 0; vec < NVMEINTR; vec++) {
		if (SLIST_EMPTY(&vmeintr_handlers[vec])) {
			*array++ = vec;
			if (--count == 0)
				return 0;
		}
	}

	/*
	 * There are not enough free vectors, so we'll have to share.
	 */
	for (vec = 0; vec < NVMEINTR; vec++) {
		ih = SLIST_FIRST(&vmeintr_handlers[vec]);
		if (ih->ih_ipl == ipl && !ISSET(ih->ih_flags, INTR_EXCLUSIVE)) {
			*array++ = vec;
			if (--count == 0)
				return 0;
		}
	}

	/*
	 * There are not enough vectors to share.
	 */
	return EPERM;
}

int
vmeintr_establish(u_int vec, struct intrhand *ih, const char *name)
{
	struct intrhand *intr;
	intrhand_t *list;

	list = &vmeintr_handlers[vec];
	intr = SLIST_FIRST(list);
	if (intr != NULL) {
		if (intr->ih_ipl != ih->ih_ipl) {
#ifdef DIAGNOSTIC
			printf("%s: can't use ipl %d for vector %x,"
			    " it uses ipl %d\n",
			    __func__, ih->ih_ipl, vec, intr->ih_ipl);
#endif
			return EINVAL;
		}
		if (ISSET(intr->ih_flags, INTR_EXCLUSIVE) ||
		    ISSET(ih->ih_flags, INTR_EXCLUSIVE))  {
#ifdef DIAGNOSTIC
			printf("%s: can't share vector %x\n", __func__, vec);
#endif
			return EINVAL;
		}
	}

	evcount_attach(&ih->ih_count, name, &ih->ih_ipl);
	SLIST_INSERT_HEAD(list, ih, ih_link);

	/*
	 * Enable VME interrupt source for this level.
	 */
	intsrc_enable(INTSRC_VME(ih->ih_ipl), ih->ih_ipl);

	return 0;
}

void
vmeintr_disestablish(u_int vec, struct intrhand *ih)
{
	struct intrhand *intr;
	intrhand_t *list;

	list = &vmeintr_handlers[vec];
	evcount_detach(&ih->ih_count);
	SLIST_REMOVE(list, ih, intrhand, ih_link);

	if (!SLIST_EMPTY(list))
		return;

	/*
	 * Walk the interrupts table to check if this level needs
	 * to be disabled.
	 */
	for (vec = 0; vec < NVMEINTR; vec++) {
		intr = SLIST_FIRST(&vmeintr_handlers[vec]);
		if (intr != NULL && intr->ih_ipl == ih->ih_ipl)
			break;
	}
	if (vec == NVMEINTR)
		intsrc_disable(INTSRC_VME(ih->ih_ipl));
}

/*
 * bus_space specific functions
 */

int
vme_a16_map(bus_space_tag_t tag, bus_addr_t addr, bus_size_t size, int flags,
    bus_space_handle_t *ret)
{
	struct vme_softc *sc = (void *)vme_cd.cd_devs[0];
	vaddr_t va;
	int rc;

	rc = vme_map(sc, sc->sc_ext_a16, VME_A16, addr, size, flags, &va);
	*ret = (bus_space_handle_t)va;
	return rc;
}

int
vme_a24_map(bus_space_tag_t tag, bus_addr_t addr, bus_size_t size, int flags,
    bus_space_handle_t *ret)
{
	struct vme_softc *sc = (void *)vme_cd.cd_devs[0];
	vaddr_t va;
	int rc;

	rc = vme_map(sc, sc->sc_ext_a24, VME_A24, addr, size, flags, &va);
	*ret = (bus_space_handle_t)va;
	return rc;
}

int
vme_a32_map(bus_space_tag_t tag, bus_addr_t addr, bus_size_t size, int flags,
    bus_space_handle_t *ret)
{
	struct vme_softc *sc = (void *)vme_cd.cd_devs[0];
	vaddr_t va;
	int rc;

	rc = vme_map(sc, sc->sc_ext_a32, VME_A32, addr, size, flags, &va);
	*ret = (bus_space_handle_t)va;
	return rc;
}

int
vme_map(struct vme_softc *sc, struct extent *ext, u_int awidth,
    bus_addr_t addr, bus_size_t size, int flags, vaddr_t *rva)
{
	const struct vme_range *r;
	int rc;
	paddr_t pa;
	psize_t offs, len;

	/*
	 * Since we need to map VME address ranges on demand, we will allocate
	 * with a page granularity.
	 */
	pa = trunc_page(addr);
	offs = addr - pa;
	len = round_page(addr + size) - pa;

	/*
	 * Check that the mapping fits within the available address ranges.
	 */
	for (r = sc->sc_ranges; r->vr_width != 0; r++) {
		if (r->vr_width == awidth &&
		    r->vr_start <= addr && r->vr_end >= addr + size - 1)
			break;
	}
	if (r->vr_width == 0)
		return EINVAL;

	/*
	 * Register this range in the per-width extent.
	 */
	if (ext != NULL) {
		rc = extent_alloc_region(ext, atop(pa), atop(len),
		    EX_NOWAIT | EX_MALLOCOK);
		if (rc != 0)
			return rc;
	}

	/*
	 * Allocate virtual memory for the range and map it.
	 */
	rc = vme_map_r(r, pa, len, flags, PROT_READ | PROT_WRITE, rva);
	if (rc != 0) {
		if (ext != NULL)
			(void)extent_free(ext, atop(pa), atop(len),
			    EX_NOWAIT | EX_MALLOCOK);
		return rc;
	}

	*rva += offs;
	return 0;
}

int
vme_map_r(const struct vme_range *r, paddr_t pa, psize_t len, int flags,
    vm_prot_t prot, vaddr_t *rva)
{
	vaddr_t ova, va;
	u_int pg;

	ova = va = uvm_km_valloc(kernel_map, len);
	if (va == 0)
		return ENOMEM;

	pa += r->vr_base;
	for (pg = atop(len); pg != 0; pg--) {
		pmap_kenter_pa(va, pa, prot);
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	if (flags & BUS_SPACE_MAP_CACHEABLE)
		pmap_cache_ctrl(ova, ova + len, CACHE_GLOBAL);
	pmap_update(pmap_kernel());

	*rva = ova;

	return 0;
}

void
vme_a16_unmap(bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t size)
{
	struct vme_softc *sc = (void *)vme_cd.cd_devs[0];
	vaddr_t va = (vaddr_t)handle;
	paddr_t pa;

	if (pmap_extract(pmap_kernel(), va, &pa) == FALSE)
		return;

	return vme_unmap(sc, sc->sc_ext_a16, VME_A16, va, pa, size);
}

void
vme_a24_unmap(bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t size)
{
	struct vme_softc *sc = (void *)vme_cd.cd_devs[0];
	vaddr_t va = (vaddr_t)handle;
	paddr_t pa;

	if (pmap_extract(pmap_kernel(), va, &pa) == FALSE)
		return;

	return vme_unmap(sc, sc->sc_ext_a24, VME_A24, va, pa, size);
}

void
vme_a32_unmap(bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t size)
{
	struct vme_softc *sc = (void *)vme_cd.cd_devs[0];
	vaddr_t va = (vaddr_t)handle;
	paddr_t pa;

	if (pmap_extract(pmap_kernel(), va, &pa) == FALSE)
		return;

	return vme_unmap(sc, sc->sc_ext_a32, VME_A32, va, pa, size);
}

void
vme_unmap(struct vme_softc *sc, struct extent *ext, u_int awidth,
    vaddr_t vaddr, paddr_t paddr, bus_size_t size)
{
	const struct vme_range *r;
	vaddr_t va;
	paddr_t pa, addr;
	psize_t len;

	va = trunc_page(vaddr);
	pa = trunc_page(paddr);
	len = round_page(paddr + size) - pa;

	/*
	 * Retrieve the address range this mapping comes from.
	 */
	for (r = sc->sc_ranges; r->vr_width != 0; r++) {
		if (r->vr_width != awidth)
			continue;
		addr = paddr - r->vr_base;
		if (r->vr_width == awidth &&
		    r->vr_start <= addr && r->vr_end >= addr + size - 1)
			break;
	}
	if (r->vr_width == 0) {
#ifdef DIAGNOSTIC
		printf("%s: nonsensical A%d mapping at va 0x%08lx pa 0x%08lx\n",
		    __func__, AWIDTH(awidth), vaddr, paddr);
#endif
		return;
	}

	/*
	 * Undo the mapping.
	 */
	pmap_kremove(va, len);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, va, len);

	/*
	 * Unregister mapping.
	 */
	if (ext != NULL) {
		pa -= r->vr_base;
		extent_free(ext, atop(pa), atop(len), EX_NOWAIT | EX_MALLOCOK);
	}
}

int
vme_subregion(bus_space_tag_t tag, bus_space_handle_t handle, bus_addr_t offset,
    bus_size_t size, bus_space_handle_t *ret)
{
	/* since vme_map produces linear mappings, this is safe */
	/* XXX does not check range overflow */
	*ret = handle + offset;
	return 0;
}

void *
vme_vaddr(bus_space_tag_t tag, bus_space_handle_t handle)
{
	return (void *)handle;
}

/*
 * D8 access routines
 */

uint16_t
vme_d8_read_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	volatile uint8_t *addr = (volatile uint8_t *)(h + o);
	return ((uint16_t)addr[0] << 8) | ((uint16_t)addr[1]);
}

uint32_t
vme_d8_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	volatile uint8_t *addr = (volatile uint8_t *)(h + o);
	return ((uint32_t)addr[0] << 24) | ((uint32_t)addr[1] << 16) |
	    ((uint32_t)addr[2] << 8) | ((uint32_t)addr[3]);
}

void
vme_d8_write_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint16_t v)
{
	volatile uint8_t *addr = (volatile uint8_t *)(h + o);
	addr[0] = v >> 8;
	addr[1] = v;
}

void
vme_d8_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint32_t v)
{
	volatile uint8_t *addr = (volatile uint8_t *)(h + o);
	addr[0] = v >> 24;
	addr[1] = v >> 16;
	addr[2] = v >> 8;
	addr[3] = v;
}

void
vme_d8_read_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	len >>= 1;
	while (len-- != 0) {
		*(uint16_t *)buf = vme_d8_read_2(t, h, o);
		buf += 2;
	}
}

void
vme_d8_write_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	len >>= 1;
	while (len-- != 0) {
		vme_d8_write_2(t, h, o, *(uint16_t *)buf);
		buf += 2;
	}
}

void
vme_d8_read_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	len >>= 2;
	while (len-- != 0) {
		*(uint32_t *)buf = vme_d8_read_4(t, h, o);
		buf += 4;
	}
}

void
vme_d8_write_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	len >>= 2;
	while (len-- != 0) {
		vme_d8_write_4(t, h, o, *(uint32_t *)buf);
		buf += 4;
	}
}
/*
 * D16 access routines
 */

uint32_t
vme_d16_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	volatile uint16_t *addr = (volatile uint16_t *)(h + o);
	return ((uint32_t)addr[0] << 16) | ((uint32_t)addr[1]);
}

void
vme_d16_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint32_t v)
{
	volatile uint16_t *addr = (volatile uint16_t *)(h + o);
	addr[0] = v >> 16;
	addr[1] = v;
}

void
vme_d16_read_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	len >>= 2;
	while (len-- != 0) {
		*(uint32_t *)buf = vme_d16_read_4(t, h, o);
		buf += 4;
	}
}

void
vme_d16_write_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	len >>= 2;
	while (len-- != 0) {
		vme_d16_write_4(t, h, o, *(uint32_t *)buf);
		buf += 4;
	}
}

/*
 * Get a bus_space_tag for the requested address and data access modes.
 */
int
vmebus_get_bst(struct device *vsc, u_int aspace, u_int dspace,
    bus_space_tag_t *bst)
{
	struct aviion_bus_space_tag *tag;

	switch (dspace) {
	case VME_D32:
	case VME_D16:
	case VME_D8:
		break;
	default:
		return EINVAL;
	}
	
	switch (aspace) {
	case VME_A32:
	case VME_A24:
	case VME_A16:
		break;
	default:
		return EINVAL;
	}

	tag = (struct aviion_bus_space_tag *)malloc(sizeof *tag, M_DEVBUF,
	    M_NOWAIT);
	if (tag == NULL)
		return ENOMEM;

	switch (aspace) {
	default:
	case VME_A32:
		tag->_space_map = vme_a32_map;
		tag->_space_unmap = vme_a32_unmap;
		break;
	case VME_A24:
		tag->_space_map = vme_a24_map;
		tag->_space_unmap = vme_a24_unmap;
		break;
	case VME_A16:
		tag->_space_map = vme_a16_map;
		tag->_space_unmap = vme_a16_unmap;
		break;
	}

	tag->_space_subregion = vme_subregion;
	tag->_space_vaddr = vme_vaddr;
	tag->_space_read_1 = generic_space_read_1;
	tag->_space_write_1 = generic_space_write_1;

	switch (dspace) {
	default:
	case VME_D32:
		tag->_space_read_2 = generic_space_read_2;
		tag->_space_write_2 = generic_space_write_2;
		tag->_space_read_4 = generic_space_read_4;
		tag->_space_write_4 = generic_space_write_4;
		tag->_space_read_raw_2 = generic_space_read_raw_2;
		tag->_space_write_raw_2 = generic_space_write_raw_2;
		tag->_space_read_raw_4 = generic_space_read_raw_4;
		tag->_space_write_raw_4 = generic_space_write_raw_4;
		break;
	case VME_D16:
		tag->_space_read_2 = generic_space_read_2;
		tag->_space_write_2 = generic_space_write_2;
		tag->_space_read_4 = vme_d16_read_4;
		tag->_space_write_4 = vme_d16_write_4;
		tag->_space_read_raw_2 = generic_space_read_raw_2;
		tag->_space_write_raw_2 = generic_space_write_raw_2;
		tag->_space_read_raw_4 = vme_d16_read_raw_4;
		tag->_space_write_raw_4 = vme_d16_write_raw_4;
		break;
	case VME_D8:
		tag->_space_read_2 = vme_d8_read_2;
		tag->_space_write_2 = vme_d8_write_2;
		tag->_space_read_4 = vme_d8_read_4;
		tag->_space_write_4 = vme_d8_write_4;
		tag->_space_read_raw_2 = vme_d8_read_raw_2;
		tag->_space_write_raw_2 = vme_d8_write_raw_2;
		tag->_space_read_raw_4 = vme_d8_read_raw_4;
		tag->_space_write_raw_4 = vme_d8_write_raw_4;
		break;
	}

	*bst = tag;
	return 0;
}

void
vmebus_release_bst(struct device *vsc, bus_space_tag_t b)
{
	free((void *)b, M_DEVBUF, sizeof(struct aviion_bus_space_tag));
}

/*
 * /dev/vme* access routines
 */

int
vmeopen(dev_t dev, int flags, int type, struct proc *p)
{
	struct vme_softc *sc;

	if (minor(dev) >= vme_cd.cd_ndevs ||
	    (sc = vme_cd.cd_devs[minor(dev)]) == NULL)
		return ENODEV;

	if (sc->sc_ranges == NULL)	/* failed attach */
		return ENODEV;

	switch (AWIDTH_FIELD(minor(dev))) {
	case VME_A32:
	case VME_A24:
	case VME_A16:
		break;
	default:
		return ENODEV;
	}

	switch (DWIDTH_FIELD(minor(dev))) {
	case VME_D32:
	case VME_D16:
	case VME_D8:
		break;
	default:
		return ENODEV;
	}

	return 0;
}

int
vmeclose(dev_t dev, int flags, int type, struct proc *p)
{
	return 0;
}

int
vmeread(dev_t dev, struct uio *uio, int flags)
{
	struct vme_softc *sc;
	int awidth, dwidth;

	sc = vme_cd.cd_devs[minor(dev)];
	awidth = AWIDTH_FIELD(minor(dev));
	dwidth = DWIDTH_FIELD(minor(dev));

	return vmerw(sc, awidth, dwidth, uio, flags);
}

int
vmewrite(dev_t dev, struct uio *uio, int flags)
{
	struct vme_softc *sc;
	int awidth, dwidth;

	sc = vme_cd.cd_devs[minor(dev)];
	awidth = AWIDTH_FIELD(minor(dev));
	dwidth = DWIDTH_FIELD(minor(dev));

	return vmerw(sc, awidth, dwidth, uio, flags);
}

int
vmerw(struct vme_softc *sc, int awidth, int dwidth, struct uio *uio, int flags)
{
	const struct vme_range *r;
	struct iovec *iov;
	psize_t delta, len;
	vaddr_t vmepg;
	int rc = 0;

	while (uio->uio_resid > 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("bogus uio %p", uio);
			continue;
		}

		/*
		 * Figure out which range we will be working on;
		 * if we hit the end of a range we'll report EFAULT.
		 */
		for (r = sc->sc_ranges; r->vr_width != 0; r++) {
			if (r->vr_width != awidth)
				continue;
			if ((off_t)r->vr_start <= uio->uio_offset &&
			    (off_t)r->vr_end >= uio->uio_offset)
				break;
		}
		if (r->vr_width == 0) {
			rc = EFAULT;	/* outside any valid range */
			break;
		}

		delta = uio->uio_offset & PAGE_MASK;
		len = ulmin(uio->uio_resid, PAGE_SIZE - delta);
		/* len = ulmin(len, (off_t)r->vr_end - uio->uio_offset); */

		rc = vme_map_r(r, trunc_page(uio->uio_offset), PAGE_SIZE, 0,
		    uio->uio_rw == UIO_READ ? PROT_READ : PROT_READ | PROT_WRITE,
		    &vmepg);
		if (rc != 0)
			break;

		/* XXX wrap this because of dwidth */
		rc = uiomove((caddr_t)vmepg + delta, len, uio);

		/* inline vme_unmap */
		pmap_kremove(vmepg, PAGE_SIZE);
		pmap_update(pmap_kernel());
		uvm_km_free(kernel_map, vmepg, PAGE_SIZE);

		if (rc != 0)
			break;

		iov->iov_base += len;
		iov->iov_len -= len;
		uio->uio_offset += len;
		uio->uio_resid -= len;
	}

	return rc;
}

int
vmeioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	switch (cmd) {
	default:
		return ENOTTY;
	}
}

paddr_t
vmemmap(dev_t dev, off_t off, int prot)
{
	struct vme_softc *sc;
	const struct vme_range *r;
	int awidth;

	sc = vme_cd.cd_devs[minor(dev)];
	awidth = AWIDTH_FIELD(minor(dev));

	if ((off & PAGE_MASK) != 0)
		return -1;

	/*
	 * Figure out which range we will be working on.
	 */
	for (r = sc->sc_ranges; r->vr_width != 0; r++) {
		if (r->vr_width != awidth)
			continue;
		if ((off_t)r->vr_start <= off &&
		    (off_t)r->vr_end >= off)
			break;
	}
	if (r->vr_width == 0)
		return -1;

	return r->vr_base + (paddr_t)off;
}
