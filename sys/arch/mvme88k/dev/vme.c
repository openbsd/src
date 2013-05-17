/*	$OpenBSD: vme.c,v 1.52 2013/05/17 22:46:27 miod Exp $ */
/*
 * Copyright (c) 2004, Miodrag Vallat.
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1995 Theo de Raadt
 * All rights reserved.
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

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/board.h>
#include <machine/cpu.h>

#include <mvme88k/dev/vme.h>

int	vmematch(struct device *, void *, void *);
void	vmeattach(struct device *, struct device *, void *);

u_long	vme2chip_map(u_long, int);
int	vmeprint(void *, const char *);

int vmebustype;
unsigned int vmevecbase;

const struct cfattach vme_ca = {
        sizeof(struct device), vmematch, vmeattach
};

struct cfdriver vme_cd = {
        NULL, "vme", DV_DULL
};

/*
 * bus_space routines for VME mappings
 */

int	vme_map(bus_addr_t, bus_size_t, int, bus_space_handle_t *);
void	vme_unmap(bus_space_handle_t, bus_size_t);
int	vme_subregion(bus_space_handle_t, bus_size_t, bus_size_t,
	    bus_space_handle_t *);
void *	vme_vaddr(bus_space_handle_t);

const struct mvme88k_bus_space_tag vme_bustag = {
	vme_map,
	vme_unmap,
	vme_subregion,
	vme_vaddr
};

/*
 * VME space mapping functions
 */

int
vme_map(bus_addr_t addr, bus_size_t size, int flags, bus_space_handle_t *ret)
{
	vaddr_t map;

	map = (vaddr_t)mapiodev((paddr_t)addr, size);
	if (map == 0)
		return ENOMEM;

	*ret = (bus_space_handle_t)map;
	return 0;
}

void
vme_unmap(bus_space_handle_t handle, bus_size_t size)
{
	unmapiodev((vaddr_t)handle, size);
}

int
vme_subregion(bus_space_handle_t handle, bus_addr_t offset, bus_size_t size,
    bus_space_handle_t *ret)
{
	*ret = handle + offset;
	return (0);
}

void *
vme_vaddr(bus_space_handle_t handle)
{
	return (void *)handle;
}

/*
 * Extra D16 access functions
 *
 * D16 cards will trigger bus errors on attempting to read or write more
 * than 16 bits on the bus. Given how the m88k processor works, this means
 * basically that all long (D32) accesses must be carefully taken care of.
 *
 * Since the kernels bcopy() and bzero() routines will use 32 bit accesses
 * for performance, here are specific D16-compatible routines. They will
 * also revert to D8 operations if neither of the operands is properly
 * aligned.
 */

void d16_bcopy(const void *, void *, size_t);
void d16_bzero(void *, size_t);

void
d16_bcopy(const void *src, void *dst, size_t len)
{
	if ((vaddr_t)src & 1 || (vaddr_t)dst & 1)
		bus_space_write_region_1(&vme_bustag, 0, (vaddr_t)dst,
		    (void *)src, len);
	else {
		bus_space_write_region_2(&vme_bustag, 0, (vaddr_t)dst,
		    (void *)src, len / 2);
		if (len & 1)
			bus_space_write_1(&vme_bustag, 0,
			    dst + len - 1, *(u_int8_t *)(src + len - 1));
	}
}

void
d16_bzero(void *dst, size_t len)
{
	if ((vaddr_t)dst & 1)
		bus_space_set_region_1(&vme_bustag, 0, (vaddr_t)dst, 0, len);
	else {
		bus_space_set_region_2(&vme_bustag, 0, (vaddr_t)dst, 0, len / 2);
		if (len & 1)
			bus_space_write_1(&vme_bustag, 0, dst + len - 1, 0);
	}
}

/*
 * Configuration glue
 */

int
vmematch(struct device *parent, void *cf, void *args)
{
#ifdef MVME187
	if (brdtyp == BRD_8120)
		return 0;
#endif
	return 1;
}

int
vmeprint(void *args, const char *bus)
{
	struct confargs *ca = args;

	printf(" addr 0x%x", ca->ca_paddr);
	if (ca->ca_ipl > 0)
		printf(" ipl %d", ca->ca_ipl);
	if (ca->ca_vec >= 0)
		printf(" vec 0x%x", ca->ca_vec);
	return UNCONF;
}

int
vmescan(struct device *parent, void *child, void *args, int bustype)
{
	struct cfdata *cf = child;
	struct confargs oca, *ca = args;

	bzero(&oca, sizeof oca);
	oca.ca_iot = &vme_bustag;
	oca.ca_dmat = ca->ca_dmat;
	oca.ca_bustype = bustype;
	oca.ca_paddr = cf->cf_loc[0];
	oca.ca_vec = cf->cf_loc[1];
	oca.ca_ipl = cf->cf_loc[2];
	if (oca.ca_ipl > 0 && oca.ca_vec < 0)
		oca.ca_vec = vme_findvec(-1);
	oca.ca_name = cf->cf_driver->cd_name;

	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return 0;

	config_attach(parent, cf, &oca, vmeprint);
	return 1;
}

void
vmeattach(struct device *parent, struct device *self, void *args)
{
	struct confargs *ca = args;

	if (platform->is_syscon())
		printf(": system controller");
	printf("\n");

	vmevecbase = platform->init_vme(self->dv_xname);
	vmebustype = ca->ca_bustype;

	while (config_found(self, args, NULL))
		;
}

/* find a VME vector based on what is in NVRAM settings. */
int
vme_findvec(int skip)
{
	return intr_findvec(vmevecbase, 0xff, skip);
}

/*
 * make local addresses 1G-2G correspond to VME addresses 3G-4G,
 * as D32
 */
#define VME2_D32STARTPHYS	(1*1024*1024*1024UL)
#define VME2_D32ENDPHYS		(2*1024*1024*1024UL)
#define VME2_D32STARTVME	(3*1024*1024*1024UL)
#define VME2_D32BITSVME		(3*1024*1024*1024UL)

/*
 * make local addresses 3G-3.75G correspond to VME addresses 3G-3.75G,
 * as D16
 */
#define VME2_D16STARTPHYS	(3*1024*1024*1024UL)
#define VME2_D16ENDPHYS		(3*1024*1024*1024UL + 768*1024*1024UL)
#define VME2_A32D16STARTPHYS	(0xff000000UL)
#define VME2_A32D16ENDPHYS	(0xff7fffffUL)

/*
 * Returns a physical address mapping for a VME address & length.
 * Note: on some hardware it is not possible to create certain
 * mappings, ie. the MVME147 cannot do 32 bit accesses to VME bus
 * addresses from 0 to physmem.
 */
paddr_t
vmepmap(struct device *sc, off_t vmeaddr, int bustype)
{
	u_int32_t base = (u_int32_t)vmeaddr;	/* wrap around 4GB */

	switch (bustype) {
	case BUS_VMES:		/* D16 VME Transfers */
#ifdef DEBUG
		printf("base 0x%8llx/0x%8x\n", vmeaddr, base);
#endif
		base = vme2chip_map(base, 16);
#ifdef DEBUG
		if (base == 0)
			printf("%s: cannot map pa 0x%x\n", sc->dv_xname, base);
#endif
		break;
	case BUS_VMEL:		/* D32 VME Transfers */
#ifdef DEBUG
		printf("base 0x%8llx/0x%8x\n", vmeaddr, base);
#endif
		base = vme2chip_map(base, 32);
#ifdef DEBUG
		if (base == 0)
			printf("%s: cannot map pa 0x%x\n", sc->dv_xname, base);
#endif
		break;
	}

	return base;
}

static vaddr_t vmemap(struct device *, off_t);
static void vmeunmap(paddr_t);

/* if successful, returns the va of a vme bus mapping */
static __inline__ vaddr_t
vmemap(struct device *sc, off_t vmeaddr)
{
	paddr_t pa;

	pa = vmepmap(sc, vmeaddr, BUS_VMES);
	if (pa == 0)
		return (0);
	return mapiodev(pa, PAGE_SIZE);
}

static __inline__ void
vmeunmap(vaddr_t va)
{
	unmapiodev(va, PAGE_SIZE);
}

int
vmerw(struct device *sc, struct uio *uio, int flags, int bus)
{
	vaddr_t v;
	size_t c;
	struct iovec *iov;
	paddr_t vme;
	int error = 0;

	while (uio->uio_resid > 0 && error == 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("vmerw");
			continue;
		}

		v = uio->uio_offset;
		c = ulmin(iov->iov_len, MAXPHYS);
		if ((v & PGOFSET) + c > PAGE_SIZE) /* max 1 page at a time */
			c = PAGE_SIZE - (v & PGOFSET);
		if (c == 0)
			return 0;
		vme = vmemap(sc, v & ~PGOFSET);
		if (vme == 0)
			return EACCES;
		error = uiomove((void *)vme + (v & PGOFSET), c, uio);
		vmeunmap(vme);
	}
	return error;
}

/*
 * Currently registered VME interrupt vectors for a given IPL, if they
 * are unique. Used to help the MVME181 and MVME188 interrupt handler when
 * they fail to complete the VME interrupt acknowledge cycle to get the
 * interrupt vector number.
 */
u_int vmevec_hints[NIPLS] = {
	(u_int)-1, (u_int)-1, (u_int)-1, (u_int)-1,
	(u_int)-1, (u_int)-1, (u_int)-1, (u_int)-1
};

/*
 * On the VMEbus, only one cpu may be configured to respond to any
 * particular vme ipl. Therefore, it wouldn't make sense to globally
 * enable all the interrupts all the time -- it would not be possible
 * to put two cpu's and one vme card into a single cage. Rather, we
 * enable each vme interrupt only when we are attaching a device that
 * uses it. This makes it easier (though not trivial) to put two cpu
 * cards in one VME cage, and both can have some limited access to vme
 * interrupts (just can't share the same irq).
 * Obviously no check is made to see if another cpu is using that
 * interrupt. If you share you will lose.
 */

int
vmeintr_establish(int vec, struct intrhand *ih, const char *name)
{
	int rc;

#ifdef DIAGNOSTIC
	if (ih->ih_ipl < 1 || ih->ih_ipl > 7)
		return EINVAL;
#endif

	if (platform->intsrc_available(INTSRC_VME, ih->ih_ipl) != 0)
		return EINVAL;

	if ((rc = intr_establish(vec, ih, name)) != 0)
		return rc;

	if (vmevec_hints[ih->ih_ipl] == (u_int)-1)
		vmevec_hints[ih->ih_ipl] = vec;
	else
		vmevec_hints[ih->ih_ipl] = (u_int)-1;

	/*
	 * Enable VME interrupt source for this level, if necessary.
	 */
	platform->intsrc_enable(INTSRC_VME, ih->ih_ipl);

	return 0;
}

/*
 * A32 accesses on the MVME1[6789]x require setting up mappings in
 * the VME2 chip.
 * XXX VME address must be between 2G and 4G
 * XXX We only support D32 at the moment..
 * XXX smurph - This is bogus, get rid of it! Should check vme/syscon for offsets.
 */
u_long
vme2chip_map(base, dwidth)
	u_long base;
	int dwidth;
{
	/*
	 * Since we are checking range for one page only, no need to check
	 * for address wraparound.
	 */
	switch (dwidth) {
	case 16:
		if (base < VME2_D16STARTPHYS ||
		    base + PAGE_SIZE > VME2_D16ENDPHYS)
			return 0;
		break;
	case 32:
		if (base < VME2_D32STARTPHYS ||
		    base + PAGE_SIZE > VME2_D32ENDPHYS)
			return 0;
		break;
	default:
		return 0;
	}
	return base;
}
