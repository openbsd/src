/*	$OpenBSD: cross.c,v 1.13 1999/01/19 10:04:54 niklas Exp $	*/

/*
 * Copyright (c) 1994, 1996 Niklas Hallqvist, Carsten Hammer
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niklas Hallqvist.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>

#include <amiga/amiga/custom.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/zbusvar.h>
#include <amiga/isa/isa_machdep.h>
#include <amiga/isa/crossreg.h>
#include <amiga/isa/crossvar.h>

extern int cold;

int crossdebug = 0;

void	crossattach __P((struct device *, struct device *, void *));
int	crossmatch __P((struct device *, void *, void *));
int	crossprint __P((void *, const char *));

int	cross_io_map __P((bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *));
int	cross_mem_map __P((bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *));
int	cross_io_unmap __P((bus_space_tag_t, bus_space_handle_t, bus_size_t));
int	cross_mem_unmap __P((bus_space_tag_t, bus_space_handle_t, bus_size_t));

int	crossintr __P((void *));

void	cross_attach_hook __P((struct device *, struct device *,
	    struct isabus_attach_args *));
void	*cross_intr_establish __P((void *, int, int, int, int (*)(void *),
	    void *, char *));
void	cross_intr_disestablish __P((void *, void *));
int	cross_intr_check __P((void *, int, int));

int	cross_pager_get_pages __P((vm_pager_t, vm_page_t *, int, boolean_t));

struct cfattach cross_ca = {
	sizeof(struct cross_softc), crossmatch, crossattach
};

struct cfdriver cross_cd = {
	NULL, "cross", DV_DULL, 0
};

struct pagerops crosspagerops = {
	NULL,
	NULL,
	NULL,
	cross_pager_get_pages,
	NULL,
	NULL,
	vm_pager_clusternull,
	NULL,
	NULL,
	NULL
};

struct pager_struct cross_pager;

int
crossmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct zbus_args *zap = aux;

	/*
	 * Check manufacturer and product id.
	 */
	if (zap->manid == 2011 && zap->prodid == 3)
		return(1);
	return(0);
}

void
crossattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct cross_softc *sc = (struct cross_softc *)self;
	struct zbus_args *zap = aux;
	struct isabus_attach_args iba;
	int i;

	bcopy(zap, &sc->sc_zargs, sizeof(struct zbus_args));
	sc->sc_status = CROSS_STATUS_ADDR(zap->va);
	sc->sc_imask = 1 << CROSS_MASTER;

	sc->sc_iot.bs_data = sc;
	sc->sc_iot.bs_map = cross_io_map;
	sc->sc_iot.bs_unmap = cross_io_unmap;
	sc->sc_iot.bs_swapped = 1;
	sc->sc_iot.bs_shift = 1;

	sc->sc_memt.bs_data = sc;
	sc->sc_memt.bs_map = cross_mem_map;
	sc->sc_memt.bs_unmap = cross_mem_unmap;
	sc->sc_memt.bs_shift = 1;

	sc->sc_ic.ic_data = sc;
	sc->sc_ic.ic_attach_hook = cross_attach_hook;
	sc->sc_ic.ic_intr_establish = cross_intr_establish;
	sc->sc_ic.ic_intr_disestablish = cross_intr_disestablish;
	sc->sc_ic.ic_intr_check = cross_intr_check;

	sc->sc_pager.pg_ops = &crosspagerops;
	sc->sc_pager.pg_type = PG_DFLT;
	sc->sc_pager.pg_flags = 0;
	sc->sc_pager.pg_data = sc;

	/* Allocate a bunch of pages used for the bank-switching logic.  */
	for (i = 0; i < CROSS_BANK_SIZE / NBPG; i++) {
		VM_PAGE_INIT(&sc->sc_page[i], NULL, 0);
		sc->sc_page[i].phys_addr = (vm_offset_t)zap->pa +
		    CROSS_XL_MEM + i * NBPG;
		sc->sc_page[i].flags |= PG_FICTITIOUS;
		vm_page_free(&sc->sc_page[i]);
	}

	/* Enable interrupts lazily in cross_intr_establish.  */
	CROSS_ENABLE_INTS(zap->va, 0);

	/* Default 16 bit tranfer  */
	*CROSS_HANDLE_TO_XLP_LATCH((bus_space_handle_t)zap->va) = CROSS_SBHE; 
	printf(": pa 0x%08x va 0x%08x size 0x%x\n", zap->pa, zap->va,
	    zap->size);

	iba.iba_busname = "isa";
	iba.iba_iot = &sc->sc_iot;
	iba.iba_memt = &sc->sc_memt;
	iba.iba_ic = &sc->sc_ic;
	config_found(self, &iba, crossprint);
}

int
crossprint(auxp, pnp)
	void *auxp;
	const char *pnp;
{
	if (pnp == NULL)
		return (QUIET);
	return (UNCONF);
}


int
cross_io_map(bst, addr, sz, cacheable, handle)
	bus_space_tag_t bst;
	bus_addr_t addr;
	bus_size_t sz;
	int cacheable;
	bus_space_handle_t *handle;
{
	*handle = (bus_space_handle_t)
	    ((struct cross_softc *)bst->bs_data)->sc_zargs.va + 2 * addr;
	return (0);
}

int
cross_mem_map(bst, addr, sz, cacheable, handle)
	bus_space_tag_t bst;
	bus_addr_t addr;
	bus_size_t sz;
	int cacheable;
	bus_space_handle_t *handle;
{
	struct cross_softc *sc = (struct cross_softc *)bst->bs_data;
	bus_addr_t banked_start;
	bus_size_t banked_size;
	vm_object_t object;
	vm_offset_t kva;
	int error;

	/*
	 * XXX Do extent checking here.
	 */

	/*
	 * Compute the bank range.  Note that we need to shift bus-addresses
	 * and sizes left one bit.
	 */
	banked_start = (addr << 1) & ~(CROSS_BANK_SIZE - 1);
        banked_size = ((sz << 1) + CROSS_BANK_SIZE - 1) &
	    ~(CROSS_BANK_SIZE - 1);

	/* Create the object that will take care of the bankswitching.  */
	object = vm_object_allocate(banked_size);
	if (object == NULL)
		goto fail_obj;
	vm_object_enter(object, &sc->sc_pager);
	vm_object_setpager(object, &sc->sc_pager, banked_start, FALSE);

	/*
	 * When done like this double mappings will be possible, thus
	 * wasting a little mapping space.  This happens when several
	 * bus_space_map maps stuff from the same bank.  But I don't care.
	 */
	kva = kmem_alloc_pageable(kernel_map, banked_size);
	if (kva == 0)
		goto fail_alloc;
	vm_map_lock(kernel_map);
	error = vm_map_insert(kernel_map, object, 0, kva, kva + banked_size);
	vm_map_unlock(kernel_map);
	if (error != KERN_SUCCESS)
		goto fail_insert;

	/* Tell caller where to find his data.  */
	*handle = (bus_space_handle_t)(kva + (addr << 1) - banked_start);
	return (0);

fail_insert:
	kmem_free(kernel_map, kva, banked_size);
fail_alloc:
	vm_object_deallocate(object);
fail_obj:
	return (1);
}

int
cross_io_unmap(bst, handle, sz)
	bus_space_tag_t bst;
	bus_space_handle_t handle;
	bus_size_t sz;
{
	return (0);
}

int
cross_mem_unmap(bst, handle, sz)
	bus_space_tag_t bst;
	bus_space_handle_t handle;
	bus_size_t sz;
{
#if 0
	struct cross_softc *sc = (struct cross_softc *)bst->bs_data;
#endif

	/* Remove the object handling this mapping.  */
	return (0);
}

static int cross_int_map[] = {
    0, 0, 0, 0, CROSS_IRQ3, CROSS_IRQ4, CROSS_IRQ5, CROSS_IRQ6, CROSS_IRQ7, 0,
    CROSS_IRQ9, CROSS_IRQ10, CROSS_IRQ11, CROSS_IRQ12, 0, CROSS_IRQ14,
    CROSS_IRQ15
};

int
crossintr(v)
	void *v;
{
	struct intrhand *ih = (struct intrhand *)v;
	int handled;

	if (!(*ih->ih_status & ih->ih_mask))
		return (0);
	for (handled = 0; ih; ih = ih->ih_next)
		if ((*ih->ih_fun)(ih->ih_arg))
			handled = 1;
	return (handled);
}

void
cross_attach_hook(parent, self, iba)
	struct device *parent, *self;
	struct isabus_attach_args *iba;
{
}

void *
cross_intr_establish(ic, irq, type, level, ih_fun, ih_arg, ih_what)
	void *ic;
	int irq;
	int type;
	int level;
	int (*ih_fun)(void *);
	void *ih_arg;
	char *ih_what;
{
	struct intrhand **p, *c, *ih;
	struct cross_softc *sc = (struct cross_softc *)ic;

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL) {
		printf("cross_intr_establish: can't malloc handler info");
		return (NULL);
	}

	if (irq > ICU_LEN || type == IST_NONE) {
		printf("cross_intr_establish: bogus irq or type");
		return (NULL);
	}

	switch (sc->sc_intrsharetype[irq]) {
	case IST_NONE:
		sc->sc_intrsharetype[irq] = type;
		break;
	case IST_EDGE:
	case IST_LEVEL:
		if (type == sc->sc_intrsharetype[irq])
			break;
	case IST_PULSE:
		if (type != IST_NONE)
			printf("cross_intr_establish: can't share %s with %s",
			    isa_intr_typename(sc->sc_intrsharetype[irq]),
			    isa_intr_typename(type));
		break;
        }

	/*
	 * Figure out where to put the handler.
	 * This is O(N^2), but we want to preserve the order, and N is
	 * generally small.
	 */
	for (p = &sc->sc_ih[irq]; (c = *p) != NULL; p = &c->ih_next)
		;

	/*
	 * Poke the real handler in now.
	 */
	ih->ih_fun = ih_fun;
	ih->ih_arg = ih_arg;
	ih->ih_count = 0;
	ih->ih_next = NULL;
	ih->ih_irq = irq;
	ih->ih_what = ih_what;
	ih->ih_mask = 1 << cross_int_map[irq + 1];
	ih->ih_status = sc->sc_status;

	ih->ih_isr.isr_intr = crossintr;
	ih->ih_isr.isr_arg = ih;
	ih->ih_isr.isr_ipl = 6;
	ih->ih_isr.isr_mapped_ipl = level;

	*p = ih;
	add_isr(&ih->ih_isr);

	sc->sc_imask |= 1 << cross_int_map[irq + 1];
	CROSS_ENABLE_INTS(sc->sc_zargs.va, sc->sc_imask);

	return (ih);
}

void
cross_intr_disestablish(ic, arg)
	void *ic;
	void *arg;
{
	struct intrhand *ih = arg;
	struct cross_softc *sc = (struct cross_softc *)ic;
	int irq = ih->ih_irq;
	struct intrhand **p, *q;

	if (irq > ICU_LEN)
		panic("cross_intr_establish: bogus irq");

	sc->sc_imask &= ~ih->ih_mask;
	CROSS_ENABLE_INTS (sc->sc_zargs.va, sc->sc_imask);
	remove_isr(&ih->ih_isr);

	/*
	 * Remove the handler from the chain.
	 * This is O(n^2), too.
	 */
	for (p = &sc->sc_ih[irq]; (q = *p) != NULL && q != ih; p = &q->ih_next)
		;
	if (q)
		*p = q->ih_next;
	else
		panic("cross_intr_disestablish: handler not registered");
	free(ih, M_DEVBUF);

	if (sc->sc_intrsharetype[irq] == NULL)
		sc->sc_intrsharetype[irq] = IST_NONE;
}

int
cross_pager_get_pages(pager, mlist, npages, sync)
	vm_pager_t	pager;
	vm_page_t	*mlist;
	int		npages;
	boolean_t	sync;
{
	struct cross_softc *sc = (struct cross_softc *)pager->pg_data;
	int i;
	vm_object_t object, old_object;
	vm_offset_t offset;

	while(npages--) {
		i = ((*mlist)->offset & (CROSS_BANK_SIZE - 1)) / NBPG;
		object = (*mlist)->object;
		old_object = sc->sc_page[i].object;
		offset = (*mlist)->offset;
		vm_page_lock_queues();
		vm_object_lock(object);
		if (old_object)
			vm_object_lock(old_object);
		vm_page_free(*mlist);

		/* generate A13-A19 for correct page */
		*CROSS_HANDLE_TO_XLP_LATCH(
		    (bus_space_handle_t)sc->sc_zargs.va) =
		    object->paging_offset >> 13 | CROSS_SBHE;

		vm_page_rename(&sc->sc_page[i], object, offset);
		if (old_object)
			vm_object_unlock(old_object);
		vm_object_unlock(object);
		vm_page_unlock_queues();
		mlist++;
	}
	return (VM_PAGER_OK);
}

int
cross_intr_check(ic, irq, type)
	void *ic;
	int irq;
	int type;
{
	struct cross_softc *sc = (struct cross_softc *)ic;

	return (__isa_intr_check(irq, type, sc->sc_intrsharetype));
}
