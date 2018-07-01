/*	$OpenBSD: acpi_machdep.c,v 1.1 2018/07/01 19:30:37 kettenis Exp $	*/
/*
 * Copyright (c) 2018 Mark Kettenis
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#include <dev/acpi/acpivar.h>

int	lid_action;

int	acpi_fdt_match(struct device *, void *, void *);
void	acpi_fdt_attach(struct device *, struct device *, void *);
void	acpi_attach(struct device *, struct device *, void *);

struct cfattach acpi_fdt_ca = {
	sizeof(struct acpi_softc), acpi_fdt_match, acpi_fdt_attach
};

int
acpi_fdt_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "openbsd,acpi-5.0");
}

void
acpi_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpi_softc *sc = (struct acpi_softc *)self;
	struct fdt_attach_args *faa = aux;
	bus_dma_tag_t dmat;

	/* Create coherent DMA tag. */
	dmat = malloc(sizeof(*sc->sc_dmat), M_DEVBUF, M_WAITOK | M_ZERO);
	memcpy(dmat, faa->fa_dmat, sizeof(*dmat));
	dmat->_flags |= BUS_DMA_COHERENT;
	
	sc->sc_memt = faa->fa_iot;
	sc->sc_dmat = dmat;

	acpi_attach_common(sc, faa->fa_reg[0].addr);
}

int
acpi_map(paddr_t pa, size_t len, struct acpi_mem_map *handle)
{
	paddr_t pgpa = trunc_page(pa);
	paddr_t endpa = round_page(pa + len);
	vaddr_t va = uvm_km_valloc(kernel_map, endpa - pgpa);

	if (va == 0)
		return (ENOMEM);

	handle->baseva = va;
	handle->va = (u_int8_t *)(va + (pa & PGOFSET));
	handle->vsize = endpa - pgpa;
	handle->pa = pa;

	do {
		pmap_kenter_pa(va, pgpa, PROT_READ | PROT_WRITE);
		va += NBPG;
		pgpa += NBPG;
	} while (pgpa < endpa);

	return 0;
}

void
acpi_unmap(struct acpi_mem_map *handle)
{
	pmap_kremove(handle->baseva, handle->vsize);
	uvm_km_free(kernel_map, handle->baseva, handle->vsize);
}

int
acpi_bus_space_map(bus_space_tag_t t, bus_addr_t addr, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	return bus_space_map(t, addr, size, flags, bshp);
}

void
acpi_bus_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t size)
{
	bus_space_unmap(t, bsh, size);
}

int
acpi_acquire_glk(uint32_t *lock)
{
	/* No global lock. */
	return 1;
}

int
acpi_release_glk(uint32_t *lock)
{
	/* No global lock. */
	return 0;
}

void
acpi_attach_machdep(struct acpi_softc *sc)
{
	/* Nothing to do. */
}

void *
acpi_intr_establish(int irq, int flags, int level,
    int (*func)(void *), void *arg, const char *name)
{
	struct interrupt_controller *ic;
	uint32_t interrupt[3];

	extern LIST_HEAD(, interrupt_controller) interrupt_controllers;
	LIST_FOREACH(ic, &interrupt_controllers, ic_list) {
		if (ic->ic_phandle == 1)
			break;
	}
	if (ic == NULL)
		return NULL;

	interrupt[0] = 0;
	interrupt[1] = irq - 32;
	interrupt[2] = 0x4;

	return ic->ic_establish(ic->ic_cookie, interrupt, level,
				func, arg, (char *)name);
}

void
acpi_sleep_clocks(struct acpi_softc *sc, int state)
{
}

void
acpi_resume_clocks(struct acpi_softc *sc)
{
}

int
acpi_sleep_cpu(struct acpi_softc *sc, int state)
{
	return 0;
}

void
acpi_resume_cpu(struct acpi_softc *sc)
{
}

#ifdef MULTIPROCESSOR

void
acpi_sleep_mp(void)
{
}

void
acpi_resume_mp(void)
{
}

#endif
