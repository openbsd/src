/*	$OpenBSD: isabr.c,v 1.2 2007/01/14 17:54:45 miod Exp $	*/

/*
 * Copyright (c) 2007 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
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
 * HP 9000/4xx model `t' single ISA slot attachment
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/hp300spu.h>

#include <uvm/uvm_extern.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <hp300/dev/frodoreg.h>
#include <hp300/dev/frodovar.h>
#include <hp300/dev/isabrreg.h>

int	isabr_match(struct device *, void *, void *);
void	isabr_attach(struct device *, struct device *, void *);
int	isabr_print(void *, const char *);

struct isabr_softc {
	struct device	 sc_dev;
	struct extent	*sc_io_extent;
	struct extent	*sc_mem_extent;
};

struct cfattach isabr_ca = {
	sizeof(struct isabr_softc), isabr_match, isabr_attach
};

struct cfdriver isabr_cd = {
	NULL, "isabr", DV_DULL
};

struct isabr_softc *isabr;

int	isabr_bus_space_setup(struct isabr_softc *, struct frodo_attach_args *,
	    struct isabus_attach_args *);

int
isabr_match(struct device *parent, void *match, void *aux)
{
	struct frodo_attach_args *fa = aux;
	static int isa_matched = 0;

	if (isa_matched != 0)
		return (0);

	if (strcmp(fa->fa_name, isabr_cd.cd_name) != 0)
		return (0);

	/*
	 * We assume parent has checked our physical existence for us.
	 */

	return (isa_matched = 1);
}

void
isabr_attach(struct device *parent, struct device *self, void *aux)
{
	struct isabus_attach_args iba;
	struct isabr_softc *sc = (struct isabr_softc *)self;
	struct frodo_attach_args *faa = (struct frodo_attach_args *)aux;

	bzero(&iba, sizeof(iba));

	if (isabr_bus_space_setup(sc, faa, &iba) != 0) {
		printf(": can not initialize bus_space\n");
		return;
	}
	printf("\n");

	iba.iba_busname = "isa";
	config_found(self, &iba, isabr_print);
}

int
isabr_print(void *aux, const char *pnp)
{
	if (pnp)
		printf("isa at %s", pnp);
	return (UNCONF);
}

/*
 * ISA support functions
 */

void
isa_attach_hook(struct device *parent, struct device *self,
    struct isabus_attach_args *iba)
{
	iba->iba_ic = parent;	/* isabr0 */
}

/*
 * Interrupt handling.
 *
 * We are currently registering ISA interrupt handlers as Frodo interrupt
 * handlers directly. We can because model `t' only have a single slot.
 *
 * Eventually this should be replaced with an isabr interrupt dispatcher,
 * allowing multiple interrupts on the same Frodo line. This would also
 * move the ISA-specific acknowledge out of non-ISA Frodo lines processing.
 * And this would allow a real intr_disestablish function.
 */
void *
isa_intr_establish(isa_chipset_tag_t ic, int irq, int type, int level,
    int (*handler)(void *), void *arg, char *name)
{
	struct isabr_softc *sc = (struct isabr_softc *)ic;
	struct isr *isr;
	int fline;

	/*
	 * Frodo is configured for edge interrupts.
	 */
	if (type != IST_EDGE) {
#ifdef DIAGNOSTIC
		printf("%s: non-edge interrupt type not supported on hp300\n",
		    name);
#endif
		return (NULL);
	}

	switch (irq) {
	case 3:
	case 4:
	case 5:
	case 6:
		fline = FRODO_INTR_ILOW;
		break;
	case 7:
	case 9:
	case 10:
	case 11:
		fline = FRODO_INTR_IMID;
		break;
	case 12:
	case 14:
	case 15:
		fline = FRODO_INTR_IHI;
		break;
	default:
#ifdef DIAGNOSTIC
		printf("%s: ISA irq %d not available on " MACHINE "\n", name);
#endif
		return (NULL);
	}

	isr = (struct isr *)malloc(sizeof(struct isr), M_DEVBUF, M_NOWAIT);
	if (isr == NULL)
		return (NULL);

	isr->isr_func = handler;
	isr->isr_arg = arg;
	isr->isr_priority = level;

	if (frodo_intr_establish(sc->sc_dev.dv_parent, fline, isr, name) == 0)
		return (isr);

	free(isr, M_DEVBUF);
	return (NULL);
}

void
isa_intr_disestablish(isa_chipset_tag_t ic, void *cookie)
{
#if 0
	struct isabr_softc *sc = (struct isabr_softc *)ic;
	struct isr *isr = cookie;

	/* XXX how to find fline back? */
	frodo_intr_disestablish(sc->dv_parent, fline);
	free(isr, M_DEVBUF);
#else
	panic("isa_intr_disestablish");
#endif
}

/*
 * Implementation of bus_space mapping for the hp300 isa slot.
 *
 * Everything is memory mapped, but the I/O space is scattered for better
 * userland access control granularity, should we ever provide iopl
 * facilities, thus i/o space accesses need their own routines set, while
 * memory space simply reuse the ``canonical'' bus_space routines.
 *
 * For the I/O space, all bus_space_map allocations are extended to a 8 ports
 * granularity, so that they span entire, contiguous pages; the handle value
 * however needs to keep track of the in-page offset if the first port is
 * not aligned to a ``line'' boundary.
 *
 * I.e, a bus_space_map(0x302, 0xe) call will map the 0x300-0x30f area,
 * and return a pointer the 0x302 port. Access routines will then, from
 * this pointer, construct a (0x300, 0x02) tuple, which they can use to
 * access the remainder of the range.
 */

/*
 * ISA I/O space
 */

int	hp300_isa_io_map(bus_addr_t, bus_size_t, int, bus_space_handle_t *);
void	hp300_isa_io_unmap(bus_space_handle_t, bus_size_t);
int	hp300_isa_io_subregion(bus_space_handle_t, bus_size_t, bus_size_t,
	    bus_space_handle_t *);
void *	hp300_isa_io_vaddr(bus_space_handle_t);

u_int8_t hp300_isa_io_bsr1(bus_space_handle_t, bus_size_t);
u_int16_t hp300_isa_io_bsr2(bus_space_handle_t, bus_size_t);
u_int32_t hp300_isa_io_bsr4(bus_space_handle_t, bus_size_t);
u_int16_t __hp300_isa_io_bsr2(bus_space_handle_t, bus_size_t);
u_int32_t __hp300_isa_io_bsr4(bus_space_handle_t, bus_size_t);
void	hp300_isa_io_bsrm1(bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
void	hp300_isa_io_bsrm2(bus_space_handle_t, bus_size_t, u_int16_t *, size_t);
void	hp300_isa_io_bsrm4(bus_space_handle_t, bus_size_t, u_int32_t *, size_t);
void	hp300_isa_io_bsrrm2(bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
void	hp300_isa_io_bsrrm4(bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
void	hp300_isa_io_bsrr1(bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
void	hp300_isa_io_bsrr2(bus_space_handle_t, bus_size_t, u_int16_t *, size_t);
void	hp300_isa_io_bsrr4(bus_space_handle_t, bus_size_t, u_int32_t *, size_t);
void	hp300_isa_io_bsrrr2(bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
void	hp300_isa_io_bsrrr4(bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
void	hp300_isa_io_bsw1(bus_space_handle_t, bus_size_t, u_int8_t);
void	hp300_isa_io_bsw2(bus_space_handle_t, bus_size_t, u_int16_t);
void	hp300_isa_io_bsw4(bus_space_handle_t, bus_size_t, u_int32_t);
void	__hp300_isa_io_bsw2(bus_space_handle_t, bus_size_t, u_int16_t);
void	__hp300_isa_io_bsw4(bus_space_handle_t, bus_size_t, u_int32_t);
void	hp300_isa_io_bswm1(bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
void	hp300_isa_io_bswm2(bus_space_handle_t, bus_size_t, const u_int16_t *, size_t);
void	hp300_isa_io_bswm4(bus_space_handle_t, bus_size_t, const u_int32_t *, size_t);
void	hp300_isa_io_bswrm2(bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
void	hp300_isa_io_bswrm4(bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
void	hp300_isa_io_bswr1(bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
void	hp300_isa_io_bswr2(bus_space_handle_t, bus_size_t, const u_int16_t *, size_t);
void	hp300_isa_io_bswr4(bus_space_handle_t, bus_size_t, const u_int32_t *, size_t);
void	hp300_isa_io_bswrr2(bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
void	hp300_isa_io_bswrr4(bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
void	hp300_isa_io_bssm1(bus_space_handle_t, bus_size_t, u_int8_t, size_t);
void	hp300_isa_io_bssm2(bus_space_handle_t, bus_size_t, u_int16_t, size_t);
void	hp300_isa_io_bssm4(bus_space_handle_t, bus_size_t, u_int32_t, size_t);
void	hp300_isa_io_bssr1(bus_space_handle_t, bus_size_t, u_int8_t, size_t);
void	hp300_isa_io_bssr2(bus_space_handle_t, bus_size_t, u_int16_t, size_t);
void	hp300_isa_io_bssr4(bus_space_handle_t, bus_size_t, u_int32_t, size_t);

struct hp300_bus_space_tag hp300_isa_io_tag = {
	hp300_isa_io_map,
	hp300_isa_io_unmap,
	hp300_isa_io_subregion,
	hp300_isa_io_vaddr,

	hp300_isa_io_bsr1,
	hp300_isa_io_bsr2,
	hp300_isa_io_bsr4,

	hp300_isa_io_bsrm1,
	hp300_isa_io_bsrm2,
	hp300_isa_io_bsrm4,

	hp300_isa_io_bsrrm2,
	hp300_isa_io_bsrrm4,

	hp300_isa_io_bsrr1,
	hp300_isa_io_bsrr2,
	hp300_isa_io_bsrr4,

	hp300_isa_io_bsrrr2,
	hp300_isa_io_bsrrr4,

	hp300_isa_io_bsw1,
	hp300_isa_io_bsw2,
	hp300_isa_io_bsw4,

	hp300_isa_io_bswm1,
	hp300_isa_io_bswm2,
	hp300_isa_io_bswm4,

	hp300_isa_io_bswrm2,
	hp300_isa_io_bswrm4,

	hp300_isa_io_bswr1,
	hp300_isa_io_bswr2,
	hp300_isa_io_bswr4,

	hp300_isa_io_bswrr2,
	hp300_isa_io_bswrr4,

	hp300_isa_io_bssm1,
	hp300_isa_io_bssm2,
	hp300_isa_io_bssm4,

	hp300_isa_io_bssr1,
	hp300_isa_io_bssr2,
	hp300_isa_io_bssr4
};

int
hp300_isa_io_map(bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	int error;
	u_int iobase, iosize, npg;
	vaddr_t va;
	paddr_t pa;

#ifdef DEBUG
	printf("isa_io_map(%04x, %04x)", bpa, size);
#endif

	/*
	 * Reserve the range in the allocation extent.
	 */
	if (bpa < IO_ISABEGIN || bpa + size > IO_ISAEND + 1) {
#ifdef DEBUG
		printf(" outside available range\n");
#endif
		return (ERANGE);
	}
	error = extent_alloc_region(isabr->sc_io_extent, bpa, size,
	    EX_MALLOCOK);
	if (error != 0) {
#ifdef DEBUG
		printf(" overlaps extent\n");
#endif
		return (error);
	}

	/*
	 * Round the allocation to a multiple of 8 bytes, to end up
	 * with entire pages.
	 */
	iobase = bpa & ~(ISABR_IOPORT_LINE - 1);
	iosize = ((bpa + size + (ISABR_IOPORT_LINE - 1)) &
	    ~(ISABR_IOPORT_LINE - 1)) - iobase;

	/*
	 * Compute how many pages will be necessary to map this range.
	 */
	npg = iosize / ISABR_IOPORT_LINE;
#ifdef DEBUG
	printf("->(%04x, %04x)=%d@", iobase, iosize, npg);
#endif

	/*
	 * Allocate virtual address space to map this space in.
	 */
	va = uvm_km_valloc(kernel_map, ptoa(npg));
	if (va == 0) {
#ifdef DEBUG
		printf("NULL\n");
#endif
		extent_free(isabr->sc_io_extent, bpa, size, EX_MALLOCOK);
		return (ENOMEM);
	}

	*bshp = (bus_space_handle_t)(va + (bpa - iobase));

	pa = ISABR_IOPORT_BASE + ISAADDR(iobase);
#ifdef DEBUG
	printf("%08x (ret %08x) pa %08x\n", va, *bshp, pa);
#endif

	while (npg != 0) {
		pmap_kenter_cache(va, pa, PG_RW | PG_CI);
		npg--;
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());

	return (0);
}

void
hp300_isa_io_unmap(bus_space_handle_t bsh, bus_size_t size)
{
	u_int iobase, iosize, npg;
	vaddr_t va;
	paddr_t pa;

#ifdef DEBUG
	printf("isa_io_unmap(%08x, %04x)", bsh, size);
#endif

	/*
	 * Find the pa matching this allocation, and the I/O port base
	 * from it.
	 */
	va = (vaddr_t)bsh;
	if (pmap_extract(pmap_kernel(), va, &pa) == FALSE) {
#ifdef DEBUG
		printf("-> no pa\n");
#endif
		return;	/* XXX be vocal? */
	}

#ifdef DEBUG
	printf("-> pa %08x ", pa);
#endif
	pa -= ISABR_IOPORT_BASE;
	iobase = ISAPORT(pa);
	if (iobase < IO_ISABEGIN || iobase > IO_ISAEND) {
#ifdef DEBUG
		printf("iobase %08x???\n", iobase);
#endif
		return;	/* XXX be vocal? */
	}

	iosize = size + (iobase & (ISABR_IOPORT_LINE - 1)) +
	    (ISABR_IOPORT_LINE - 1);
	npg = iosize / ISABR_IOPORT_LINE;
#ifdef DEBUG
	printf(" range %04x-%04x: %d\n", iobase, size, npg);
#endif

	pmap_kremove(va, ptoa(npg));
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, va, ptoa(npg));

	(void)extent_free(isabr->sc_io_extent, (u_long)iobase, size,
	    EX_MALLOCOK);
}

/*
 * Round down an I/O space bus_space_handle, so that it points to the
 * beginning of a page.
 * This is gonna be hell when we support ports above 0x400.
 */
#define	REALIGN_IO_HANDLE(h, o) \
do { \
	u_int tmp; \
	tmp = (h) & (ISABR_IOPORT_LINE - 1); \
	(h) -= tmp; \
	(o) += tmp; \
} while (0)

/* ARGSUSED */
int
hp300_isa_io_subregion(bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t size, bus_space_handle_t *nbshp)
{
	REALIGN_IO_HANDLE(bsh, offset);
	bsh += ISAADDR(offset);
	*nbshp = bsh;
	return (0);
}

void *
hp300_isa_io_vaddr(bus_space_handle_t h)
{
	return (NULL);
}

/* bus_space_read_X */

u_int8_t
hp300_isa_io_bsr1(bus_space_handle_t bsh, bus_size_t offset)
{
	vaddr_t va;
	u_int8_t rc;

	REALIGN_IO_HANDLE(bsh, offset);
	va = (vaddr_t)bsh + ISAADDR(offset);
	rc = *(volatile u_int8_t *)va;
#ifdef DEBUG
	printf("R%03x(%x):%02x\t", offset, va, rc);
#endif
	return (rc);
}

u_int16_t
__hp300_isa_io_bsr2(bus_space_handle_t bsh, bus_size_t offset)
{
	u_int16_t rc;
	vaddr_t va;

	if (offset & 1) {
		rc = hp300_isa_io_bsr1(bsh, offset + 1) << 8;
		rc |= hp300_isa_io_bsr1(bsh, offset);
	} else {
		REALIGN_IO_HANDLE(bsh, offset);
		va = (vaddr_t)bsh + ISAADDR(offset);
		rc = *(volatile u_int16_t *)va;
	}
#ifdef DEBUG
	printf("R%03x:%04x\t", offset, rc);
#endif
	return (rc);
}

u_int16_t
hp300_isa_io_bsr2(bus_space_handle_t bsh, bus_size_t offset)
{
	u_int16_t rc;

	rc = __hp300_isa_io_bsr2(bsh, offset);
	return (letoh16(rc));
}

u_int32_t
__hp300_isa_io_bsr4(bus_space_handle_t bsh, bus_size_t offset)
{
	u_int32_t rc;
	vaddr_t va;

	if (offset & 3) {
		rc = hp300_isa_io_bsr1(bsh, offset + 3) << 24;
		rc |= hp300_isa_io_bsr1(bsh, offset + 2) << 16;
		rc |= hp300_isa_io_bsr1(bsh, offset + 1) << 8;
		rc |= hp300_isa_io_bsr1(bsh, offset);
	} else {
		REALIGN_IO_HANDLE(bsh, offset);
		va = (vaddr_t)bsh + ISAADDR(offset);
		rc = *(volatile u_int32_t *)va;
	}
#ifdef DEBUG
	printf("R%03x:%08x\t", offset, rc);
#endif
	return (rc);
}

u_int32_t
hp300_isa_io_bsr4(bus_space_handle_t bsh, bus_size_t offset)
{
	u_int32_t rc;

	rc = __hp300_isa_io_bsr4(bsh, offset);
	return (letoh32(rc));
}

/* bus_space_read_multi_X */

void
hp300_isa_io_bsrm1(bus_space_handle_t h, bus_size_t offset,
	     u_int8_t *a, size_t c)
{
	while ((int)--c >= 0)
		*a++ = hp300_isa_io_bsr1(h, offset);
}

void
hp300_isa_io_bsrm2(bus_space_handle_t h, bus_size_t offset,
	     u_int16_t *a, size_t c)
{
	while ((int)--c >= 0)
		*a++ = hp300_isa_io_bsr2(h, offset);
}

void
hp300_isa_io_bsrm4(bus_space_handle_t h, bus_size_t offset,
	     u_int32_t *a, size_t c)
{
	while ((int)--c >= 0)
		*a++ = hp300_isa_io_bsr4(h, offset);
}

/* bus_space_read_raw_multi_X */

void
hp300_isa_io_bsrrm2(bus_space_handle_t h, bus_size_t offset,
	     u_int8_t *a, size_t c)
{
	while ((int)--c >= 0) {
		*(u_int16_t *)a = __hp300_isa_io_bsr2(h, offset);
		a += 2;
	}
}

void
hp300_isa_io_bsrrm4(bus_space_handle_t h, bus_size_t offset,
	     u_int8_t *a, size_t c)
{
	while ((int)--c >= 0) {
		*(u_int32_t *)a = __hp300_isa_io_bsr4(h, offset);
		a += 4;
	}
}

/* bus_space_read_region_X */

void
hp300_isa_io_bsrr1(bus_space_handle_t h, bus_size_t offset,
	     u_int8_t *a, size_t c)
{
	while ((int)--c >= 0)
		*a++ = hp300_isa_io_bsr1(h, offset++);
}

void
hp300_isa_io_bsrr2(bus_space_handle_t h, bus_size_t offset,
	     u_int16_t *a, size_t c)
{
	while ((int)--c >= 0) {
		*a++ = hp300_isa_io_bsr2(h, offset);
		offset += 2;
	}
}

void
hp300_isa_io_bsrr4(bus_space_handle_t h, bus_size_t offset,
	     u_int32_t *a, size_t c)
{
	while ((int)--c >= 0) {
		*a++ = hp300_isa_io_bsr4(h, offset);
		offset += 4;
	}
}

/* bus_space_read_raw_region_X */

void
hp300_isa_io_bsrrr2(bus_space_handle_t h, bus_size_t offset,
	     u_int8_t *a, size_t c)
{
	c >>= 1;
	while ((int)--c >= 0) {
		*(u_int16_t *)a = __hp300_isa_io_bsr2(h, offset);
		offset += 2;
		a += 2;
	}
}

void
hp300_isa_io_bsrrr4(bus_space_handle_t h, bus_size_t offset,
	     u_int8_t *a, size_t c)
{
	c >>= 2;
	while ((int)--c >= 0) {
		*(u_int32_t *)a = __hp300_isa_io_bsr4(h, offset);
		offset += 4;
		a += 4;
	}
}

/* bus_space_write_X */

void
hp300_isa_io_bsw1(bus_space_handle_t h, bus_size_t offset, u_int8_t v)
{
	vaddr_t va;

	REALIGN_IO_HANDLE(h, offset);
	va = (vaddr_t)h + ISAADDR(offset);
	*(volatile u_int8_t *)va = v;
#ifdef DEBUG
	printf("W%03x:%02x\t", offset, v);
#endif
}

void
__hp300_isa_io_bsw2(bus_space_handle_t h, bus_size_t offset, u_int16_t v)
{
	vaddr_t va;

	if (offset & 1) {
		hp300_isa_io_bsw1(h, offset + 1, v >> 8);
		hp300_isa_io_bsw1(h, offset, v);
	} else {
		REALIGN_IO_HANDLE(h, offset);
		va = (vaddr_t)h + ISAADDR(offset);
		*(volatile u_int16_t *)va = v;
	}
#ifdef DEBUG
	printf("W%03x:%04x\t", offset, v);
#endif
}

void
hp300_isa_io_bsw2(bus_space_handle_t h, bus_size_t offset, u_int16_t v)
{
	__hp300_isa_io_bsw2(h, offset, htole16(v));
}

void
__hp300_isa_io_bsw4(bus_space_handle_t h, bus_size_t offset, u_int32_t v)
{
	vaddr_t va;

	if (offset & 3) {
		hp300_isa_io_bsw1(h, offset + 3, v >> 24);
		hp300_isa_io_bsw1(h, offset + 2, v >> 16);
		hp300_isa_io_bsw1(h, offset + 1, v >> 8);
		hp300_isa_io_bsw1(h, offset, v);
	} else {
		REALIGN_IO_HANDLE(h, offset);
		va = (vaddr_t)h + ISAADDR(offset);
		*(volatile u_int32_t *)va = v;
	}
#ifdef DEBUG
	printf("W%03x:%08x\t", offset, v);
#endif
}

void
hp300_isa_io_bsw4(bus_space_handle_t h, bus_size_t offset, u_int32_t v)
{
	__hp300_isa_io_bsw4(h, offset, htole32(v));
}

/* bus_space_write_multi_X */

void
hp300_isa_io_bswm1(bus_space_handle_t h, bus_size_t offset,
	     const u_int8_t *a, size_t c)
{
	while ((int)--c >= 0)
		hp300_isa_io_bsw1(h, offset, *a++);
}

void
hp300_isa_io_bswm2(bus_space_handle_t h, bus_size_t offset,
	     const u_int16_t *a, size_t c)
{
	while ((int)--c >= 0)
		hp300_isa_io_bsw2(h, offset, *a++);
}

void
hp300_isa_io_bswm4(bus_space_handle_t h, bus_size_t offset,
	     const u_int32_t *a, size_t c)
{
	while ((int)--c >= 0)
		hp300_isa_io_bsw4(h, offset, *a++);
}

/* bus_space_write_raw_multi_X */

void
hp300_isa_io_bswrm2(bus_space_handle_t h, bus_size_t offset,
	     const u_int8_t *a, size_t c)
{
	while ((int)--c >= 0) {
		__hp300_isa_io_bsw2(h, offset, *(u_int16_t *)a);
		a += 2;
	}
}

void
hp300_isa_io_bswrm4(bus_space_handle_t h, bus_size_t offset,
	     const u_int8_t *a, size_t c)
{
	while ((int)--c >= 0) {
		__hp300_isa_io_bsw4(h, offset, *(u_int32_t *)a);
		a += 4;
	}
}

/* bus_space_write_region_X */

void
hp300_isa_io_bswr1(bus_space_handle_t h, bus_size_t offset,
	     const u_int8_t *a, size_t c)
{
	while ((int)--c >= 0)
		hp300_isa_io_bsw1(h, offset++, *a++);
}

void
hp300_isa_io_bswr2(bus_space_handle_t h, bus_size_t offset,
	     const u_int16_t *a, size_t c)
{
	while ((int)--c >= 0) {
		hp300_isa_io_bsw2(h, offset, *a++);
		offset += 2;
	}
}

void
hp300_isa_io_bswr4(bus_space_handle_t h, bus_size_t offset,
	     const u_int32_t *a, size_t c)
{
	while ((int)--c >= 0) {
		hp300_isa_io_bsw4(h, offset, *a++);
		offset += 4;
	}
}

/* bus_space_write_raw_region_X */

void
hp300_isa_io_bswrr2(bus_space_handle_t h, bus_size_t offset,
	     const u_int8_t *a, size_t c)
{
	c >>= 1;
	while ((int)--c >= 0) {
		__hp300_isa_io_bsw2(h, offset, *(u_int16_t *)a);
		offset += 2;
		a += 2;
	}
}

void
hp300_isa_io_bswrr4(bus_space_handle_t h, bus_size_t offset,
	     const u_int8_t *a, size_t c)
{
	c >>= 2;
	while ((int)--c >= 0) {
		__hp300_isa_io_bsw4(h, offset, *(u_int32_t *)a);
		offset += 4;
		a += 4;
	}
}

/* bus_space_set_multi_X */

void
hp300_isa_io_bssm1(bus_space_handle_t h, bus_size_t offset,
	     u_int8_t v, size_t c)
{
	while ((int)--c >= 0)
		hp300_isa_io_bsw1(h, offset, v);
}

void
hp300_isa_io_bssm2(bus_space_handle_t h, bus_size_t offset,
	     u_int16_t v, size_t c)
{
	while ((int)--c >= 0)
		hp300_isa_io_bsw2(h, offset, v);
}

void
hp300_isa_io_bssm4(bus_space_handle_t h, bus_size_t offset,
	     u_int32_t v, size_t c)
{
	while ((int)--c >= 0)
		hp300_isa_io_bsw4(h, offset, v);
}

/* bus_space_set_region_X */

void
hp300_isa_io_bssr1(bus_space_handle_t h, bus_size_t offset,
	     u_int8_t v, size_t c)
{
	while ((int)--c >= 0)
		hp300_isa_io_bsw1(h, offset++, v);
}

void
hp300_isa_io_bssr2(bus_space_handle_t h, bus_size_t offset,
	     u_int16_t v, size_t c)
{
	while ((int)--c >= 0) {
		hp300_isa_io_bsw2(h, offset, v);
		offset += 2;
	}
}

void
hp300_isa_io_bssr4(bus_space_handle_t h, bus_size_t offset,
	     u_int32_t v, size_t c)
{
	while ((int)--c >= 0) {
		hp300_isa_io_bsw4(h, offset, v);
		offset += 4;
	}
}

/*
 * ISA memory space
 */

int	hp300_isa_mem_map(bus_addr_t, bus_size_t, int, bus_space_handle_t *);
void	hp300_isa_mem_unmap(bus_space_handle_t, bus_size_t);

int
hp300_isa_mem_map(bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	int error;
	bus_addr_t membase;
	bus_size_t rsize;
	vaddr_t va;
	paddr_t pa;
	pt_entry_t template;

	/*
	 * Reserve the range in the allocation extent.
	 */
	if (bpa < IOM_BEGIN || bpa + size > IOM_END)
		return (ERANGE);
	error = extent_alloc_region(isabr->sc_mem_extent, bpa, size,
	    EX_MALLOCOK);
	if (error != 0)
		return (error);

	/*
	 * Allocate virtual address space to map this space in.
	 */
	membase = trunc_page(bpa);
	rsize = round_page(bpa + size) - membase;
	va = uvm_km_valloc(kernel_map, rsize);
	if (va == 0) {
		extent_free(isabr->sc_mem_extent, bpa, size, EX_MALLOCOK);
		return (ENOMEM);
	}

	*bshp = (bus_space_handle_t)(va + (bpa - membase));

	pa = membase + (ISABR_IOMEM_BASE - IOM_BEGIN);

	if (flags & BUS_SPACE_MAP_CACHEABLE)
		template = PG_RW;
	else
		template = PG_RW | PG_CI;

	while (rsize != 0) {
		pmap_kenter_cache(va, pa, template);
		rsize -= PAGE_SIZE;
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());

	return (0);
}

void
hp300_isa_mem_unmap(bus_space_handle_t bsh, bus_size_t size)
{
	vaddr_t va;
	vsize_t rsize;

	va = trunc_page((vaddr_t)bsh);
	rsize = round_page((vaddr_t)bsh + size) - va;

	pmap_kremove(va, rsize);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, va, rsize);

	(void)extent_free(isabr->sc_mem_extent, (u_long)bsh, size, EX_MALLOCOK);
}

struct hp300_bus_space_tag hp300_isa_mem_tag;	/* will be filled in */

/*
 * ISA bus_space initialization.
 * This creates the necessary accounting elements and initializes the
 * memory space bus_space_tag.
 */

int
isabr_bus_space_setup(struct isabr_softc *sc, struct frodo_attach_args *faa,
    struct isabus_attach_args *iba)
{
	/*
	 * Create the space extents.
	 * We only use them to prevent multiple allocations of the same areas.
	 */

	sc->sc_io_extent = extent_create("isa_io", IO_ISABEGIN,
	    IO_ISAEND + 1, M_DEVBUF, NULL, 0, EX_NOWAIT | EX_MALLOCOK);
	if (sc->sc_io_extent == NULL)
		return (ENOMEM);

	sc->sc_mem_extent = extent_create("isa_mem", IOM_BEGIN,
	    IOM_END, M_DEVBUF, NULL, 0, EX_NOWAIT | EX_MALLOCOK);
	if (sc->sc_mem_extent == NULL) {
		extent_destroy(sc->sc_io_extent);
		return (ENOMEM);
	}

	iba->iba_iot = &hp300_isa_io_tag;
	bcopy(faa->fa_tag, &hp300_isa_mem_tag,
	    sizeof(struct hp300_bus_space_tag));
	hp300_isa_mem_tag.bs_map = hp300_isa_mem_map;
	hp300_isa_mem_tag.bs_unmap = hp300_isa_mem_unmap;
	iba->iba_memt = &hp300_isa_mem_tag;

	isabr = sc;

	return (0);
}
