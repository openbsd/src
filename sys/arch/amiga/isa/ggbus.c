/*	$OpenBSD: ggbus.c,v 1.13 1999/01/19 10:04:54 niklas Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Niklas Hallqvist
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

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>

#include <amiga/amiga/custom.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/zbusvar.h>
#include <amiga/isa/isa_machdep.h>
#include <amiga/isa/ggbusvar.h>
#include <amiga/isa/ggbusreg.h>

extern int cold;

int ggdebug = 0;
int ggstrayints = 0;

void	ggbusattach __P((struct device *, struct device *, void *));
int	ggbusmatch __P((struct device *, void *, void *));
int	ggbusprint __P((void *, const char *));

int	ggbus_io_map __P((bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *));
int	ggbus_mem_map __P((bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *));
int	ggbus_cannot_mem_map __P((bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *));
int	ggbus_unmap __P((bus_space_tag_t, bus_space_handle_t, bus_size_t));

int	ggbusintr __P((void *));

void	ggbus_attach_hook __P((struct device *, struct device *,
	    struct isabus_attach_args *));
void	*ggbus_intr_establish __P((void *, int, int, int, int (*)(void *),
	    void *, char *));
void	ggbus_intr_disestablish __P((void *, void *));
int	ggbus_intr_check __P((void *, int, int));

struct cfattach ggbus_ca = {
	sizeof(struct ggbus_softc), ggbusmatch, ggbusattach
};

struct cfdriver ggbus_cd = {
	NULL, "ggbus", DV_DULL, 0
};

int
ggbusmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct zbus_args *zap = aux;

	/*
	 * Check manufacturer and product id.
	 */
	if (zap->manid == 2150 && zap->prodid == 1)
		return (1);
	return (0);
}

void
ggbusattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ggbus_softc *sc = (struct ggbus_softc *)self;
	struct zbus_args *zap = aux;
	struct isabus_attach_args iba;

	bcopy(zap, &sc->sc_zargs, sizeof(struct zbus_args));
	sc->sc_iot.bs_data = sc;
	sc->sc_iot.bs_map = ggbus_io_map;
	sc->sc_iot.bs_unmap = ggbus_unmap;
	sc->sc_iot.bs_swapped = 0;
	sc->sc_iot.bs_shift = 1;
	if (sc->sc_zargs.serno >= 2) {
		sc->sc_memt.bs_data = sc;
		sc->sc_memt.bs_map = ggbus_mem_map;
		sc->sc_memt.bs_unmap = ggbus_unmap;
		sc->sc_memt.bs_swapped = 0;
		sc->sc_memt.bs_shift = 1;
		sc->sc_status = GG2_STATUS_ADDR(sc->sc_zargs.va);

		/* XXX turn on wait states unconditionally for now. */
		GG2_ENABLE_WAIT(zap->va);
		GG2_ENABLE_INTS(zap->va);
	} else
		sc->sc_memt.bs_map = ggbus_cannot_mem_map;

	printf(": pa 0x%08x va 0x%08x size 0x%x\n", zap->pa, zap->va,
	    zap->size);

	sc->sc_ic.ic_data = sc;
	sc->sc_ic.ic_attach_hook = ggbus_attach_hook;
	sc->sc_ic.ic_intr_establish = ggbus_intr_establish;
	sc->sc_ic.ic_intr_disestablish = ggbus_intr_disestablish;
	sc->sc_ic.ic_intr_check = ggbus_intr_check;

	iba.iba_busname = "isa";
	iba.iba_iot = &sc->sc_iot;
	iba.iba_memt = &sc->sc_memt;
	iba.iba_ic = &sc->sc_ic;
	config_found(self, &iba, ggbusprint);
}

int
ggbusprint(auxp, pnp)
	void *auxp;
	const char *pnp;
{
	if (pnp == NULL)
		return (QUIET);
	return (UNCONF);
}

int
ggbus_io_map(bst, addr, sz, cacheable, handle)
	bus_space_tag_t bst;
	bus_addr_t addr;
	bus_size_t sz;
	int cacheable;
	bus_space_handle_t *handle;
{
	*handle = (bus_space_handle_t)
	    ((struct ggbus_softc *)bst->bs_data)->sc_zargs.va + 2 * addr + 1;
	return (0);
}

int
ggbus_mem_map(bst, addr, sz, cacheable, handle)
	bus_space_tag_t bst;
	bus_addr_t addr;
	bus_size_t sz;
	int cacheable;
	bus_space_handle_t *handle;
{
	*handle = (bus_space_handle_t)
	    ((struct ggbus_softc *)bst->bs_data)->sc_zargs.va + 2 * addr +
	    GG2_MEMORY_OFFSET;
	return (0);
}

int
ggbus_cannot_mem_map(bst, addr, sz, cacheable, handle)
	bus_space_tag_t bst;
	bus_addr_t addr;
	bus_size_t sz;
	int cacheable;
	bus_space_handle_t *handle;
{
	static int have_warned = 0;

	if (!have_warned++)
		printf("The Golden Gate 1 cannot map ISA memory.\n");
	return (1);
}

int
ggbus_unmap(bst, handle, sz)
	bus_space_tag_t bst;
	bus_space_handle_t handle;
	bus_size_t sz;
{
	return (0);
}

static int ggbus_int_map[] = {
    0, 0, 0, 0, GG2_IRQ3, GG2_IRQ4, GG2_IRQ5, GG2_IRQ6, GG2_IRQ7, 0,
    GG2_IRQ9, GG2_IRQ10, GG2_IRQ11, GG2_IRQ12, 0, GG2_IRQ14, GG2_IRQ15
};

int
ggbusintr(v)
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
ggbus_attach_hook(parent, self, iba)
	struct device *parent, *self;
	struct isabus_attach_args *iba;
{
}

void *
ggbus_intr_establish(ic, irq, type, level, ih_fun, ih_arg, ih_what)
	void *ic;
	int irq;
	int type;
	int level;
	int (*ih_fun)(void *);
	void *ih_arg;
	char *ih_what;
{
	struct intrhand **p, *c, *ih;
	struct ggbus_softc *sc = (struct ggbus_softc *)ic;

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL) {
		printf("ggbus_intr_establish: can't malloc handler info");
		return (NULL);
	}

	if (irq > ICU_LEN || type == IST_NONE) {
		printf("ggbus_intr_establish: bogus irq or type");
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
			printf("ggbus_intr_establish: can't share %s with %s",
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
	ih->ih_mask = 1 << ggbus_int_map[irq + 1];
	ih->ih_status = sc->sc_status;
	ih->ih_isr.isr_intr = ggbusintr;
	ih->ih_isr.isr_arg = ih;
	ih->ih_isr.isr_ipl = 6;
	ih->ih_isr.isr_mapped_ipl = level;
	*p = ih;

	add_isr(&ih->ih_isr);
	return (ih);
}

void
ggbus_intr_disestablish(ic, arg)
	void *ic;
	void *arg;
{
	struct intrhand *ih = arg;
	struct ggbus_softc *sc = (struct ggbus_softc *)ic;
	int irq = ih->ih_irq;
	struct intrhand **p, *q;

	if (irq > ICU_LEN)
		panic("ggbus_intr_establish: bogus irq");

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
		panic("ggbus_intr_disestablish: handler not registered");
	free(ih, M_DEVBUF);

	if (sc->sc_intrsharetype[irq] == NULL)
		sc->sc_intrsharetype[irq] = IST_NONE;
}

int
ggbus_intr_check(ic, irq, type)
	void *ic;
	int irq;
	int type;
{
	struct ggbus_softc *sc = (struct ggbus_softc *)ic;

	return (__isa_intr_check(irq, type, sc->sc_intrsharetype));
}
