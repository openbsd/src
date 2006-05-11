/*	$OpenBSD: vme.c,v 1.2 2006/05/11 19:50:28 miod Exp $	*/
/*
 * Copyright (c) 2006, Miodrag Vallat.
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

/*
 * XXX TODO: Finish /dev/vme{a16,a24,a32}{d8,d16,d32} interface.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/uio.h>

#include <machine/bus.h>
#include <machine/conf.h>

#include <uvm/uvm_extern.h>

#include <aviion/dev/vmevar.h>

#include <machine/av400.h>
#include <aviion/dev/sysconreg.h>

struct vmesoftc {
	struct device	sc_dev;

	struct extent	*sc_ext_a16;
	struct extent	*sc_ext_a24;
	struct extent	*sc_ext_a32;
};

int	vmematch(struct device *, void *, void *);
void	vmeattach(struct device *, struct device *, void *);

struct cfattach vme_ca = {
        sizeof(struct vmesoftc), vmematch, vmeattach
};

struct cfdriver vme_cd = {
        NULL, "vme", DV_DULL
};

int	vme16_map(bus_addr_t, bus_size_t, int, bus_space_handle_t *);
void	vme16_unmap(bus_space_handle_t, bus_size_t);
int	vme24_map(bus_addr_t, bus_size_t, int, bus_space_handle_t *);
void	vme24_unmap(bus_space_handle_t, bus_size_t);
int	vme32_map(bus_addr_t, bus_size_t, int, bus_space_handle_t *);
void	vme32_unmap(bus_space_handle_t, bus_size_t);
int	vme_subregion(bus_space_handle_t, bus_size_t, bus_size_t,
	    bus_space_handle_t *);
void *	vme_vaddr(bus_space_handle_t);

int	vme_map(struct extent *, paddr_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
void	vme_unmap(struct extent *, vme_addr_t, vaddr_t, bus_size_t);
int	vmeprint(void *, const char *);
int	vmescan(struct device *, void *, void *);

u_int	vmevecbase;

int
vmematch(struct device *parent, void *vcf, void *aux)
{
	/* XXX no VME on AV100/AV200/AV300, though */
	return (1);
}

void
vmeattach(struct device *parent, struct device *self, void *aux)
{
	struct vmesoftc *sc = (struct vmesoftc *)self;
	u_int32_t ucsr;

	printf("\n");

	/*
	 * Initialize extents
	 */
	sc->sc_ext_a16 = extent_create("vme a16", 0, 1 << (16 - PAGE_SHIFT),
	    M_DEVBUF, NULL, 0, EX_NOWAIT);
	sc->sc_ext_a24 = extent_create("vme a24", 0, 1 << (24 - PAGE_SHIFT),
	    M_DEVBUF, NULL, 0, EX_NOWAIT);
	sc->sc_ext_a32 = extent_create("vme a32", 0, 1 << (32 - PAGE_SHIFT),
	    M_DEVBUF, NULL, 0, EX_NOWAIT);

	vmevecbase = 0x80;  /* Hard coded for AV400 */

	/*
	 * Force a reasonable timeout for VME data transfers.
	 * We can not disable this, this would cause autoconf to hang
	 * on the first missing device we'll probe.
	 */
	ucsr = *(volatile u_int32_t*)AV400_UCSR;
	ucsr = (ucsr & ~VTOSELBITS) | VTO128US;
	*(volatile u_int32_t *)AV400_UCSR = ucsr;

	/*
	 * Clear EXTAD to allow VME A24 devices to access the first 16MB
	 * of memory.
	 */
	*(volatile u_int32_t *)AV400_EXTAD = 0x00000000;

	/*
	 * Use supervisor data address modifiers for VME accesses.
	 */
	*(volatile u_int32_t *)AV400_EXTAM = 0x0d;

	/*
	 * Display AV400 VME ranges.
	 */
	printf("%s: A32 %08x-%08x\n", self->dv_xname,
	    AV400_VME32_START1, AV400_VME32_END1);
	printf("%s: A32 %08x-%08x\n", self->dv_xname,
	    AV400_VME32_START2, AV400_VME32_END2);
	printf("%s: A24 %08x-%08x\n", self->dv_xname,
	    AV400_VME24_START, AV400_VME24_END);
	printf("%s: A16 %08x-%08x\n", self->dv_xname,
	    AV400_VME16_START, AV400_VME16_END);

	/* scan for child devices */
	config_search(vmescan, self, aux);
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
		return (0);

	config_attach(parent, cf, &vaa, vmeprint);
	return (1);
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

	return (UNCONF);
}

/*
 * Interrupt related code
 */

/* allocate interrupt vectors */
int
vmeintr_allocate(u_int count, int flags, u_int *array)
{
	u_int vec, v;

	if ((flags & VMEINTR_CONTIGUOUS) == 0) {
		for (vec = vmevecbase; vec <= NVMEINTR - count; vec++) {
			if (SLIST_EMPTY(&intr_handlers[vec])) {
				*array++ = vec;
				if (--count == 0)
					return (0);
			}
		}
	} else {
		for (vec = vmevecbase; vec <= NVMEINTR - count; vec++) {
			/* do we have count contiguous unassigned vectors? */
			for (v = count; v != 0; v--)
				if (!SLIST_EMPTY(&intr_handlers[vec + v - 1]))
					break;

			if (v == 0) {
				*array = vec;
				return (0);
			}
		}
	}

	return (EPERM);
}

/* enable and establish interrupt */
int
vmeintr_establish(u_int vec, struct intrhand *ih, const char *name)
{
	/*
	 * No need to enable the VME interrupt source in the AV400 interrupt
	 * controller, as they are enabled by default.
	 */
	return intr_establish(vec, ih, name);
}

/*
 * bus_space specific functions
 */

int
vme16_map(bus_addr_t addr, bus_size_t size, int flags, bus_space_handle_t *ret)
{
	struct vmesoftc *sc = (void *)vme_cd.cd_devs[0];

	if (AV400_ISVMEA16(addr) && AV400_ISVMEA16(addr + size - 1))
		return (vme_map(sc->sc_ext_a16, addr + AV400_VME16_BASE, addr,
		    size, flags, ret));
	else
		return (EINVAL);
}

int
vme24_map(bus_addr_t addr, bus_size_t size, int flags, bus_space_handle_t *ret)
{
	struct vmesoftc *sc = (void *)vme_cd.cd_devs[0];

	if (AV400_ISVMEA24(addr) && AV400_ISVMEA24(addr + size - 1))
		return (vme_map(sc->sc_ext_a24, addr + AV400_VME24_BASE, addr,
		    size, flags, ret));
	else
		return (EINVAL);
}

int
vme32_map(bus_addr_t addr, bus_size_t size, int flags, bus_space_handle_t *ret)
{
	struct vmesoftc *sc = (void *)vme_cd.cd_devs[0];

	if (AV400_ISVMEA32(addr) && AV400_ISVMEA32(addr + size - 1))
		return (vme_map(sc->sc_ext_a32, addr + AV400_VME32_BASE, addr,
		    size, flags, ret));
	else
		return (EINVAL);
}

int
vme_map(struct extent *ext, paddr_t paddr, bus_addr_t addr, bus_size_t size,
    int flags, bus_space_handle_t *ret)
{
	int rc;
	paddr_t pa;
	psize_t len;
	vaddr_t ova, va;
	u_int pg;
	extern vaddr_t pmap_map(vaddr_t, paddr_t, paddr_t, vm_prot_t, u_int);

	pa = trunc_page(paddr);
	len = round_page(paddr + size) - pa;

	if (ext != NULL) {
		rc = extent_alloc_region(ext, atop(addr), atop(len),
		    EX_NOWAIT | EX_MALLOCOK);
		if (rc != 0)
			return (rc);
	}

	ova = va = uvm_km_valloc(kernel_map, len);
	if (va == NULL) {
		rc = ENOMEM;
		goto fail;
	}

	*ret = (bus_space_handle_t)va;

	for (pg = atop(len); pg !=0; pg--) {
		pmap_kenter_pa(va, pa, UVM_PROT_RW);
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	if (flags & BUS_SPACE_MAP_CACHEABLE)
		pmap_cache_ctrl(pmap_kernel(), ova, ova + len, CACHE_GLOBAL);
	pmap_update(pmap_kernel());

	return (0);

fail:
	if (ext != NULL)
		extent_free(ext, atop(addr), atop(len), EX_NOWAIT | EX_MALLOCOK);
	return (rc);
}

void
vme16_unmap(bus_space_handle_t handle, bus_size_t size)
{
	struct vmesoftc *sc = (void *)vme_cd.cd_devs[0];
	paddr_t pa;

	if (pmap_extract(pmap_kernel(), (vaddr_t)handle, &pa) == FALSE)
		return;

	pa -= AV400_VME16_BASE;
	return (vme_unmap(sc->sc_ext_a16, pa, (vaddr_t)handle, size));
}

void
vme24_unmap(bus_space_handle_t handle, bus_size_t size)
{
	struct vmesoftc *sc = (void *)vme_cd.cd_devs[0];
	paddr_t pa;

	if (pmap_extract(pmap_kernel(), (vaddr_t)handle, &pa) == FALSE)
		return;

	pa -= AV400_VME24_BASE;
	return (vme_unmap(sc->sc_ext_a24, pa, (vaddr_t)handle, size));
}

void
vme32_unmap(bus_space_handle_t handle, bus_size_t size)
{
	struct vmesoftc *sc = (void *)vme_cd.cd_devs[0];
	paddr_t pa;

	if (pmap_extract(pmap_kernel(), (vaddr_t)handle, &pa) == FALSE)
		return;

	pa -= AV400_VME32_BASE;
	return (vme_unmap(sc->sc_ext_a32, pa, (vaddr_t)handle, size));
}

void
vme_unmap(struct extent *ext, vme_addr_t addr, vaddr_t vaddr, bus_size_t size)
{
	vaddr_t va;
	vsize_t len;

	va = trunc_page(vaddr);
	len = round_page(vaddr + size) - va;

	pmap_kremove(va, len);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, va, len);

	if (ext != NULL)
		extent_free(ext, atop(addr), atop(len),
		    EX_NOWAIT | EX_MALLOCOK);
}

int
vme_subregion(bus_space_handle_t handle, bus_addr_t offset, bus_size_t size,
    bus_space_handle_t *ret)
{
	/* since vme_map produces linear mappings, this is safe */
	*ret = handle + offset;
	return (0);
}

void *
vme_vaddr(bus_space_handle_t handle)
{
	return ((void *)handle);
}

/*
 * Get a bus_space_tag for the requested address and data access modes.
 *
 * On aviion, we do not honour the dspace yet.
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
		return (EINVAL);
	}
	
	switch (aspace) {
	case VME_A32:
	case VME_A24:
	case VME_A16:
		break;
	default:
		return (EINVAL);
	}

	tag = (struct aviion_bus_space_tag *)malloc(sizeof *tag, M_DEVBUF,
	    M_NOWAIT);
	if (tag == NULL)
		return (ENOMEM);

	switch (aspace) {
	default:
	case VME_A32:
		tag->bs_map = vme32_map;
		tag->bs_unmap = vme32_unmap;
		tag->bs_subregion = vme_subregion;
		tag->bs_vaddr = vme_vaddr;
		break;
	case VME_A24:
		tag->bs_map = vme24_map;
		tag->bs_unmap = vme24_unmap;
		tag->bs_subregion = vme_subregion;
		tag->bs_vaddr = vme_vaddr;
		break;
	case VME_A16:
		tag->bs_map = vme16_map;
		tag->bs_unmap = vme16_unmap;
		tag->bs_subregion = vme_subregion;
		tag->bs_vaddr = vme_vaddr;
		break;
	}

	*bst = tag;
	return (0);
}

void
vmebus_release_bst(struct device *vsc, bus_space_tag_t b)
{
	free((void *)b, M_DEVBUF);
}

/*
 * /dev/vme* access routines
 */

/* minor device number encoding */
#define	AWIDTH_FIELD(minor)	(minor & 0x0f)
#define	AWIDTH(w)		((w) << 3)
#define	DWIDTH_FIELD(minor)	((minor & 0xf0) >> 4)
#define	DWIDTH(w)		((w) << 3)

int
vmeopen(dev_t dev, int flags, int type, struct proc *p)
{
	if (vme_cd.cd_ndevs == 0 || vme_cd.cd_devs[0] == NULL)
		return (ENODEV);

	switch (AWIDTH_FIELD(minor(dev))) {
	case VME_A32:
	case VME_A24:
	case VME_A16:
		break;
	default:
		return (ENODEV);
	}

	switch (DWIDTH_FIELD(minor(dev))) {
	case VME_D32:
	case VME_D16:
	case VME_D8:
		break;
	default:
		return (ENODEV);
	}

	return (0);
}

int
vmeclose(dev_t dev, int flags, int type, struct proc *p)
{
	return (0);
}

int
vmeread(dev_t dev, struct uio *uio, int flags)
{
	return (EIO);
}

int
vmewrite(dev_t dev, struct uio *uio, int flags)
{
	return (EIO);
}

int
vmeioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	switch (cmd) {
	default:
		return (ENOTTY);
	}
}

paddr_t
vmemmap(dev_t dev, off_t off, int prot)
{
	int awidth;
	paddr_t pa;

	if ((off & PAGE_MASK) != 0)
		return (-1);

	awidth = AWIDTH_FIELD(minor(dev));

	/* check offset range */
	if (off < 0 || off >= (1ULL << AWIDTH(awidth)))
		return (-1);

	pa = (paddr_t)off;

	switch (awidth) {
	case VME_A32:
		if (!AV400_ISVMEA32(pa))
			return (-1);
		pa += AV400_VME32_BASE;
		break;
	case VME_A24:
		if (!AV400_ISVMEA24(pa))
			return (-1);
		pa += AV400_VME24_BASE;
		break;
	case VME_A16:
		if (!AV400_ISVMEA16(pa))
			return (-1);
		pa += AV400_VME16_BASE;
		break;
	}

	return (atop(pa));
}
