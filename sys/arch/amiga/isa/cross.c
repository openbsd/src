/*	$OpenBSD: cross.c,v 1.3 1996/04/27 18:38:55 niklas Exp $	*/

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

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>

#include <amiga/amiga/custom.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/zbusvar.h>
#include <amiga/isa/isa_machdep.h>
#include <amiga/isa/crossvar.h>
#include <amiga/isa/crossreg.h>

extern int cold;

int crossdebug = 0;

void	crossattach __P((struct device *, struct device *, void *));
int	crossmatch __P((struct device *, void *, void *));
int	crossprint __P((void *, char *));

int	cross_io_map(bus_chipset_tag_t, bus_io_addr_t, bus_io_size_t,
	    bus_io_handle_t *);
int	cross_mem_map(bus_chipset_tag_t, bus_mem_addr_t, bus_mem_size_t, int,
	    bus_mem_handle_t *);

void	cross_io_read_multi_1(bus_io_handle_t, bus_io_size_t, u_int8_t *,
	    bus_io_size_t);
void	cross_io_read_multi_2(bus_io_handle_t, bus_io_size_t, u_int16_t *,
	    bus_io_size_t);

void	cross_io_write_multi_1(bus_io_handle_t, bus_io_size_t,
	    const u_int8_t *, bus_io_size_t);
void	cross_io_write_multi_2(bus_io_handle_t, bus_io_size_t,
	    const u_int16_t *, bus_io_size_t);

/*
 * Note that the following unified access functions are prototyped for the
 * I/O access case.  We use casts to get type correctness.
 */
int	cross_unmap(bus_io_handle_t, bus_io_size_t);

__inline u_int8_t cross_read_1(bus_io_handle_t, bus_io_size_t);
__inline u_int16_t cross_read_2(bus_io_handle_t, bus_io_size_t);

__inline void cross_write_1(bus_io_handle_t, bus_io_size_t, u_int8_t);
__inline void cross_write_2(bus_io_handle_t, bus_io_size_t, u_int16_t);

/*
 * In order to share the access function implementations for I/O and memory
 * access we cast the functions for the memory access case.  These typedefs
 * make that casting look nicer.
 */
typedef int (*bus_mem_unmap_t)(bus_mem_handle_t, bus_mem_size_t);
typedef u_int8_t (*bus_mem_read_1_t)(bus_mem_handle_t, bus_mem_size_t);
typedef u_int16_t (*bus_mem_read_2_t)(bus_mem_handle_t, bus_mem_size_t);
typedef void (*bus_mem_write_1_t)(bus_mem_handle_t, bus_mem_size_t, u_int8_t);
typedef void (*bus_mem_write_2_t)(bus_mem_handle_t, bus_mem_size_t, u_int16_t);

void	cross_attach_hook(struct device *, struct device *,
	    struct isabus_attach_args *);
void	*cross_intr_establish __P((void *, int, int, int, int (*)(void *),
	    void *, char *));
void	cross_intr_disestablish __P((void *, void *));

static u_int16_t swap __P((u_int16_t));

struct amiga_bus_chipset cross_chipset = {
	0 /* bc_data */,

	cross_io_map, cross_unmap,
	cross_read_1, cross_read_2,
	0 /* bc_io_read_4 */, 0 /* bc_io_read_8 */,
	cross_io_read_multi_1, cross_io_read_multi_2,
	0 /* bc_io_multi_4 */, 0 /* bc_io_multi_8 */,
	cross_write_1, cross_write_2,
	0 /* bc_io_write_4 */, 0 /* bc_io_write_8 */,
	cross_io_write_multi_1, cross_io_write_multi_2,
	0 /* bc_io_write_multi_4 */, 0 /* bc_io_write_multi_8 */,

	cross_mem_map, (bus_mem_unmap_t)cross_unmap,
	(bus_mem_read_1_t)cross_read_1, (bus_mem_read_2_t)cross_read_2,
	0 /* bc_mem_read_4 */, 0 /* bc_mem_read_8 */,
	(bus_mem_write_1_t)cross_write_1, (bus_mem_write_2_t)cross_write_2,
	0 /* bc_mem_write_4 */, 0 /* bc_mem_write_8 */,

	/* These are extensions to the general NetBSD bus interface.  */
	swap, 0 /* bc_to_host_4 */, 0 /* bc_to_host_8 */,
	swap, 0 /* bc_from_host_4 */, 0 /* bc_from_host_8 */,
};

struct cfattach cross_ca = {
	sizeof(struct cross_softc), crossmatch, crossattach
};

struct cfdriver cross_cd = {
	NULL, "cross", DV_DULL, 0
};

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

	bcopy(zap, &sc->sc_zargs, sizeof(struct zbus_args));
	bcopy(&cross_chipset, &sc->sc_bc, sizeof(struct amiga_bus_chipset));
	sc->sc_bc.bc_data = sc;
	sc->sc_status = CROSS_STATUS_ADDR(zap->va);
	sc->sc_imask = 1 << CROSS_MASTER;

	/* Enable interrupts lazily in cross_intr_establish.  */
	CROSS_ENABLE_INTS(zap->va, 0);
	/* Default 16 bit tranfer  */
	*CROSS_HANDLE_TO_XLP_LATCH((bus_io_handle_t)zap->va) = CROSS_SBHE; 
	printf(": pa 0x%08x va 0x%08x size 0x%x\n", zap->pa, zap->va,
	    zap->size);

	sc->sc_ic.ic_data = sc;
	sc->sc_ic.ic_attach_hook = cross_attach_hook;
	sc->sc_ic.ic_intr_establish = cross_intr_establish;
	sc->sc_ic.ic_intr_disestablish = cross_intr_disestablish;

	iba.iba_busname = "isa";
	iba.iba_bc = &sc->sc_bc;
	iba.iba_ic = &sc->sc_ic;
	config_found(self, &iba, crossprint);
}

int
crossprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	if (pnp == NULL)
		return(QUIET);
	return(UNCONF);
}


int
cross_io_map(bct, addr, sz, handle)
	bus_chipset_tag_t bct;
	bus_io_addr_t addr;
	bus_io_size_t sz;
	bus_io_handle_t *handle;
{
	*handle = (bus_io_handle_t)
	    ((struct cross_softc *)bct->bc_data)->sc_zargs.va + 2 * addr;
#if 0
	printf("io_map %x %d -> %x\n", addr, sz, *handle);
#endif
	return 0;
}

int
cross_mem_map(bct, addr, sz, cacheable, handle)
	bus_chipset_tag_t bct;
	bus_mem_addr_t addr;
	bus_mem_size_t sz;
	int cacheable;
	bus_mem_handle_t *handle;
{
	*handle = (bus_mem_handle_t)
	    ((struct cross_softc *)bct->bc_data)->sc_zargs.va + 2 * addr +
	    CROSS_MEMORY_OFFSET;
#if 0
	printf("mem_map %x %d -> %x\n", addr, sz, *handle);
#endif
	return 0;
}

int
cross_unmap(handle, sz)
	bus_io_handle_t handle;
	bus_io_size_t sz;
{
	return 0;
}

__inline u_int8_t
cross_read_1(handle, addr)
	bus_io_handle_t handle;
	bus_io_size_t addr;
{
	u_int8_t val;

	/* generate A13-A19 for correct page */
	*CROSS_HANDLE_TO_XLP_LATCH(handle) = addr >> 13 | CROSS_SBHE;
	val = *(volatile u_int8_t *)(handle + 2 * addr);

#if 0
	printf("read_1 @%x handle %x -> %d\n", addr, handle, val);
#endif
	return val;
}

__inline u_int16_t
cross_read_2(handle, addr)
	bus_io_handle_t handle;
	bus_io_size_t addr;
{
	/* generate A13-A19 for correct page */
	*CROSS_HANDLE_TO_XLP_LATCH(handle) = addr >> 13 | CROSS_SBHE;
	return *(volatile u_int16_t *)(handle + 2 * addr);
}

void
cross_io_read_multi_1(handle, addr, buf, cnt)
	bus_io_handle_t handle;
	bus_io_size_t addr;
	u_int8_t *buf;
	bus_io_size_t cnt;
{
	while (cnt--)
		*buf++ = cross_read_1(handle, addr);
}

void
cross_io_read_multi_2(handle, addr, buf, cnt)
	bus_io_handle_t handle;
	bus_io_size_t addr;
	u_int16_t *buf;
	bus_io_size_t cnt;
{
	while (cnt--)
		*buf++ = cross_read_2(handle, addr);
}

__inline void
cross_write_1(handle, addr, val)
	bus_io_handle_t handle;
	bus_io_size_t addr;
	u_int8_t val;
{
	/* generate A13-A19 for correct page */
	*CROSS_HANDLE_TO_XLP_LATCH(handle) = addr >> 13 | CROSS_SBHE;
#if 0
	printf("write_1 @%x handle %x: %d\n", addr, handle, val);
#endif
	*(volatile u_int8_t *)(handle + 2 * addr + 1) = val;
}

__inline void
cross_write_2(handle, addr, val)
	bus_io_handle_t handle;
	bus_io_size_t addr;
	u_int16_t val;
{
	/* generate A13-A19 for correct page */
	*CROSS_HANDLE_TO_XLP_LATCH(handle) = addr >> 13 | CROSS_SBHE;
	*(volatile u_int16_t *)(handle + 2 * addr) = val;
}

void
cross_io_write_multi_1(handle, addr, buf, cnt)
	bus_io_handle_t handle;
	bus_io_size_t addr;
	const u_int8_t *buf;
	bus_io_size_t cnt;
{
	while (cnt--)
		cross_write_1(handle, addr, *buf++);
}

void
cross_io_write_multi_2(handle, addr, buf, cnt)
	bus_io_handle_t handle;
	bus_io_size_t addr;
	const u_int16_t *buf;
	bus_io_size_t cnt;
{
	while (cnt--)
		cross_write_2(handle, addr, *buf++);
}

static cross_int_map[] = {
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
		return 0;
	for (handled = 0; ih; ih = ih->ih_next)
		if ((*ih->ih_fun)(ih->ih_arg))
			handled = 1;
	return handled;
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
	if (ih == NULL)
		panic("cross_intr_establish: can't malloc handler info");

	if (irq > ICU_LEN || type == IST_NONE)
		panic("cross_intr_establish: bogus irq or type");

	switch (sc->sc_intrsharetype[irq]) {
	case IST_EDGE:
	case IST_LEVEL:
		if (type == sc->sc_intrsharetype[irq])
			break;
	case IST_PULSE:
		if (type != IST_NONE)
			panic("cross_intr_establish: can't share %s with %s",
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

	return ih;
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

/* Swap bytes in a short word.  */
static u_short
swap(u_short x)
{
	__asm("rolw #8,%0" : "=r" (x) : "0" (x));
	return x;
}
