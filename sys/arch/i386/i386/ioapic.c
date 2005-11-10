/*	$OpenBSD: ioapic.c,v 1.6 2005/11/10 14:35:13 mickey Exp $	*/
/* 	$NetBSD: ioapic.c,v 1.7 2003/07/14 22:32:40 lukem Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks Inc.
 *
 * Author: Bill Sommerfeld
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Copyright (c) 1999 Stefan Grefen
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR AND CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/psl.h>

#include <uvm/uvm_extern.h>

#include <machine/i82093reg.h>
#include <machine/i82093var.h>

#include <machine/i82489reg.h>
#include <machine/i82489var.h>

#include <machine/pmap.h>

#include <machine/mpbiosvar.h>

#include "isa.h"

/*
 * XXX locking
 */

int     ioapic_match(struct device *, void *, void *);
void    ioapic_attach(struct device *, struct device *, void *);

/* XXX */
extern int bus_mem_add_mapping(bus_addr_t, bus_size_t, int,
    bus_space_handle_t *);

void	apic_set_redir(struct ioapic_softc *, int);
void	apic_vectorset(struct ioapic_softc *, int, int, int);

int apic_verbose = 0;

int ioapic_bsp_id = 0;
int ioapic_cold = 1;

struct ioapic_softc *ioapics;	 /* head of linked list */
int nioapics = 0;	   	 /* number attached */

void ioapic_set_id(struct ioapic_softc *);

/*
 * A bitmap telling what APIC IDs usable for I/O APICs are free.
 * The size must be at least IOAPIC_ID_MAX bits (16).
 */
u_int16_t ioapic_id_map = (1 << IOAPIC_ID_MAX) - 1;

/*
 * When we renumber I/O APICs we provide a mapping vector giving us the new
 * ID out of the old BIOS supplied one.  Each item must be able to hold IDs
 * in [0, IOAPIC_ID_MAX << 1), since we use an extra bit to tell if the ID
 * has actually been remapped.  
 */
u_int8_t ioapic_id_remap[IOAPIC_ID_MAX];

/*
 * Register read/write routines.
 */
static __inline u_int32_t
ioapic_read(struct ioapic_softc *sc, int regid)
{
	u_int32_t val;

	/*
	 * XXX lock apic
	 */
	*(sc->sc_reg) = regid;
	val = *sc->sc_data;

	return (val);

}

static __inline void
ioapic_write(struct ioapic_softc *sc, int regid, int val)
{
	/*
	 * XXX lock apic
	 */
	*(sc->sc_reg) = regid;
	*(sc->sc_data) = val;
}

struct ioapic_softc *
ioapic_find(int apicid)
{
	struct ioapic_softc *sc;

	if (apicid == MPS_ALL_APICS) {	/* XXX mpbios-specific */
		/*
		 * XXX kludge for all-ioapics interrupt support
		 * on single ioapic systems
		 */
		if (nioapics <= 1)
			return (ioapics);
		panic("unsupported: all-ioapics interrupt with >1 ioapic");
	}

	for (sc = ioapics; sc != NULL; sc = sc->sc_next)
		if (sc->sc_apicid == apicid)
			return (sc);

	return (NULL);
}

static __inline void
ioapic_add(struct ioapic_softc *sc)
{
	sc->sc_next = ioapics;
	ioapics = sc;
	nioapics++;
}

void
ioapic_print_redir(struct ioapic_softc *sc, char *why, int pin)
{
	u_int32_t redirlo = ioapic_read(sc, IOAPIC_REDLO(pin));
	u_int32_t redirhi = ioapic_read(sc, IOAPIC_REDHI(pin));

	apic_format_redir(sc->sc_dev.dv_xname, why, pin, redirhi, redirlo);
}

struct cfattach ioapic_ca = {
	sizeof(struct ioapic_softc), ioapic_match, ioapic_attach
};

struct cfdriver ioapic_cd = {
	NULL, "ioapic", DV_DULL /* XXX DV_CPU ? */
};

int
ioapic_match(struct device *parent, void *matchv, void *aux)
{
        struct cfdata *match = (struct cfdata *)matchv;
	struct apic_attach_args * aaa = (struct apic_attach_args *)aux;

	if (strcmp(aaa->aaa_name, match->cf_driver->cd_name) == 0)
		return (1);
	return (0);
}

/* Reprogram the APIC ID, and check that it actually got set. */
void
ioapic_set_id(struct ioapic_softc *sc) {
	u_int8_t apic_id;

	ioapic_write(sc, IOAPIC_ID,
	    (ioapic_read(sc, IOAPIC_ID) & ~IOAPIC_ID_MASK) |
	    (sc->sc_apicid << IOAPIC_ID_SHIFT));

	apic_id = (ioapic_read(sc, IOAPIC_ID) & IOAPIC_ID_MASK) >>
	    IOAPIC_ID_SHIFT;

	if (apic_id != sc->sc_apicid)
		printf(", can't remap to apid %d\n", sc->sc_apicid);
	else
		printf(", remapped to apic %d\n", sc->sc_apicid);
}

/*
 * can't use bus_space_xxx as we don't have a bus handle ...
 */
void
ioapic_attach(struct device *parent, struct device *self, void *aux)
{
	struct ioapic_softc *sc = (struct ioapic_softc *)self;
	struct apic_attach_args  *aaa = (struct apic_attach_args *)aux;
	int apic_id;
	int8_t new_id;
	bus_space_handle_t bh;
	u_int32_t ver_sz;
	int i, ioapic_found;

	sc->sc_flags = aaa->flags;
	sc->sc_apicid = aaa->apic_id;

	printf(": apid %d pa 0x%lx", aaa->apic_id, aaa->apic_address);

	if (bus_mem_add_mapping(aaa->apic_address, PAGE_SIZE, 0, &bh) != 0) {
		printf(", map failed\n");
		return;
	}
	sc->sc_reg = (volatile u_int32_t *)(bh + IOAPIC_REG);
	sc->sc_data = (volatile u_int32_t *)(bh + IOAPIC_DATA);

	ver_sz = ioapic_read(sc, IOAPIC_VER);
	sc->sc_apic_vers = (ver_sz & IOAPIC_VER_MASK) >> IOAPIC_VER_SHIFT;
	sc->sc_apic_sz = (ver_sz & IOAPIC_MAX_MASK) >> IOAPIC_MAX_SHIFT;
	sc->sc_apic_sz++;

	if (mp_verbose) {
		printf(", %s mode",
		    aaa->flags & IOAPIC_PICMODE ? "PIC" : "virtual wire");
	}

	printf(", version %x, %d pins\n", sc->sc_apic_vers, sc->sc_apic_sz);

	/*
	 * If either a LAPIC or an I/O APIC is already at the ID the BIOS
	 * setup for this I/O APIC, try to find a free ID to use and reprogram
	 * the chip.  Record this remapping since all references done by the
	 * MP BIOS will be through the old ID.
	 */
	ioapic_found = ioapic_find(sc->sc_apicid) != NULL;
	if (cpu_info[sc->sc_apicid] != NULL || ioapic_found) {
		printf("%s: duplicate apic id", sc->sc_dev.dv_xname);
		new_id = ffs(ioapic_id_map) - 1;
		if (new_id == -1) {
			printf(" (and none free, ignoring)\n");
			return;
		}

		/*
		 * If there were many I/O APICs at the same ID, we choose
		 * to let later references to that ID (in the MP BIOS) refer
		 * to the first found.
		 */
		if (!ioapic_found && !IOAPIC_REMAPPED(sc->sc_apicid))
			IOAPIC_REMAP(sc->sc_apicid, new_id);
		sc->sc_apicid = new_id;
		ioapic_set_id(sc);
	}
	ioapic_id_map &= ~(1 << sc->sc_apicid);

	ioapic_add(sc);

	apic_id = (ioapic_read(sc, IOAPIC_ID) & IOAPIC_ID_MASK) >>
	    IOAPIC_ID_SHIFT;

	sc->sc_pins = malloc(sizeof(struct ioapic_pin) * sc->sc_apic_sz,
	    M_DEVBUF, M_WAITOK);

	for (i=0; i<sc->sc_apic_sz; i++) {
		sc->sc_pins[i].ip_handler = NULL;
		sc->sc_pins[i].ip_next = NULL;
		sc->sc_pins[i].ip_map = NULL;
		sc->sc_pins[i].ip_vector = 0;
		sc->sc_pins[i].ip_type = 0;
		sc->sc_pins[i].ip_minlevel = 0xff; /* XXX magic*/
		sc->sc_pins[i].ip_maxlevel = 0;	/* XXX magic */
	}

	/*
	 * In case the APIC is not initialized to the correct ID
	 * do it now.
	 */
	if (apic_id != sc->sc_apicid) {
		printf("%s: misconfigured as apic %d", sc->sc_dev.dv_xname,
		    apic_id);
		ioapic_set_id(sc);
	}
#if 0
	/* output of this was boring. */
	if (mp_verbose)
		for (i=0; i<sc->sc_apic_sz; i++)
			ioapic_print_redir(sc, "boot", i);
#endif
}

/*
 * Interrupt mapping.
 *
 * Multiple handlers may exist for each pin, so there's an
 * intrhand chain for each pin.
 *
 * Ideally, each pin maps to a single vector at the priority of the
 * highest level interrupt for that pin.
 *
 * XXX in the event that there are more than 16 interrupt sources at a
 * single level, some doubling-up may be needed.  This is not yet
 * implemented.
 *
 * XXX we are wasting some space here because we only use a limited
 * range of the vectors here.  (0x30..0xef)
 */

struct intrhand *apic_intrhand[256];
int	apic_intrcount[256];
int	apic_maxlevel[256];


/* XXX should check vs. softc max int number */
#define	LEGAL_IRQ(x)	((x) >= 0 && (x) < APIC_ICU_LEN && (x) != 2)

void
apic_set_redir(struct ioapic_softc *sc, int pin)
{
	u_int32_t redlo;
	u_int32_t redhi = 0;
	int delmode;

	struct ioapic_pin *pp;
	struct mp_intr_map *map;

	pp = &sc->sc_pins[pin];
	map = pp->ip_map;
	if (map == NULL) {
		redlo = IOAPIC_REDLO_MASK;
	} else {
		redlo = map->redir;
	}
	delmode = (redlo & IOAPIC_REDLO_DEL_MASK) >> IOAPIC_REDLO_DEL_SHIFT;

	/* XXX magic numbers */
	if ((delmode != 0) && (delmode != 1))
		;
	else if (pp->ip_handler == NULL) {
		redlo |= IOAPIC_REDLO_MASK;
	} else {
		redlo |= (pp->ip_vector & 0xff);
		redlo &= ~IOAPIC_REDLO_DEL_MASK;
		redlo |= (IOAPIC_REDLO_DEL_FIXED << IOAPIC_REDLO_DEL_SHIFT);
		redlo &= ~IOAPIC_REDLO_DSTMOD;

		/*
		 * Destination: BSP CPU
		 *
		 * XXX will want to distribute interrupts across cpu's
		 * eventually.  most likely, we'll want to vector each
		 * interrupt to a specific CPU and load-balance across
		 * cpu's.  but there's no point in doing that until after 
		 * most interrupts run without the kernel lock.  
		 */
		redhi |= (ioapic_bsp_id << IOAPIC_REDHI_DEST_SHIFT);

		/* XXX derive this bit from BIOS info */
		if (pp->ip_type == IST_LEVEL)
			redlo |= IOAPIC_REDLO_LEVEL;
		else
			redlo &= ~IOAPIC_REDLO_LEVEL;
		if (map != NULL && ((map->flags & 3) == MPS_INTPO_DEF)) {
			if (pp->ip_type == IST_LEVEL)
				redlo |= IOAPIC_REDLO_ACTLO;
			else
				redlo &= ~IOAPIC_REDLO_ACTLO;
		}
	}
	/* Do atomic write */
	ioapic_write(sc, IOAPIC_REDLO(pin), IOAPIC_REDLO_MASK);
	ioapic_write(sc, IOAPIC_REDHI(pin), redhi);
	ioapic_write(sc, IOAPIC_REDLO(pin), redlo);
	if (mp_verbose)
		ioapic_print_redir(sc, "int", pin);
}

/*
 * XXX To be really correct an NISA > 0 condition should check for these.
 * However, the i386 port pretty much assumes isa is there anyway.
 * For example, pci_intr_establish calls isa_intr_establish unconditionally.
 */
extern int fakeintr(void *); 	/* XXX headerify */
extern char *isa_intr_typename(int); 	/* XXX headerify */

/*
 * apic_vectorset: allocate a vector for the given pin, based on
 * the levels of the interrupts on that pin.
 *
 * XXX if the level of the pin changes while the pin is
 * masked, need to do something special to prevent pending
 * interrupts from being lost.
 * (the answer may be to hang the interrupt chain off of both vectors
 * until any interrupts from the old source have been handled.  the trouble
 * is that we don't have a global view of what interrupts are pending.
 *
 * Deferring for now since MP systems are more likely servers rather
 * than laptops or desktops, and thus will have relatively static
 * interrupt configuration.
 */

void
apic_vectorset(struct ioapic_softc *sc, int pin, int minlevel, int maxlevel)
{
	struct ioapic_pin *pp = &sc->sc_pins[pin];
	int ovector = 0;
	int nvector = 0;

	ovector = pp->ip_vector;
	
	if (maxlevel == 0) {
		/* no vector needed. */
		pp->ip_minlevel = 0xff; /* XXX magic */
		pp->ip_maxlevel = 0; /* XXX magic */
		pp->ip_vector = 0;
	} else if (maxlevel != pp->ip_maxlevel) {
		if (minlevel != maxlevel)
			printf("%s: pin %d shares different IPL interrupts "
			    "(%x..%x), degraded performance\n",
			    sc->sc_dev.dv_xname, pin, minlevel, maxlevel);

		/*
		 * Allocate interrupt vector at the *lowest* priority level
		 * of any of the handlers invoked by this pin.
		 *
		 * The interrupt handler will raise ipl higher than this
		 * as appropriate.
		 */
		nvector = idt_vec_alloc(minlevel, minlevel+15);

		if (nvector == 0) {
			/*
			 * XXX XXX we should be able to deal here..
			 * need to double-up an existing vector
			 * and install a slightly different handler.
			 */
			panic("%s: can't alloc vector for pin %d at level %x",
			    sc->sc_dev.dv_xname, pin, maxlevel);
		}
		apic_maxlevel[nvector] = maxlevel;
		/* 
		 * XXX want special handler for the maxlevel != minlevel
		 * case here!
		 */
		idt_vec_set(nvector, apichandler[nvector & 0xf]);
		pp->ip_vector = nvector;
		pp->ip_minlevel = minlevel;
		pp->ip_maxlevel = maxlevel;
	}
	apic_intrhand[pp->ip_vector] = pp->ip_handler;

	if (ovector) {
		/*
		 * XXX should defer this until we're sure the old vector
		 * doesn't have a pending interrupt on any processor.
		 * do this by setting a counter equal to the number of CPU's,
		 * and firing off a low-priority broadcast IPI to all cpu's.
		 * each cpu then decrements the counter; when it
		 * goes to zero, free the vector..
		 * i.e., defer until all processors have run with a CPL
		 * less than the level of the interrupt..
		 *
		 * this is only an issue for dynamic interrupt configuration
		 * (e.g., cardbus or pcmcia).
		 */
		apic_intrhand[ovector] = NULL;
		idt_vec_free(ovector);
		printf("freed vector %x\n", ovector);
	}

	apic_set_redir(sc, pin);
}

/*
 * Throw the switch and enable interrupts..
 */

void
ioapic_enable(void)
{
	int p, maxlevel, minlevel;
	struct ioapic_softc *sc;
	struct intrhand *q;
	extern void intr_calculatemasks(void); /* XXX */

	intr_calculatemasks();	/* for softints, AST's */

	ioapic_cold = 0;

	if (ioapics == NULL)
		return;

#if 1 /* XXX Will probably get removed */
	lapic_set_softvectors();
	lapic_set_lvt();
#endif

	if (ioapics->sc_flags & IOAPIC_PICMODE) {
		printf("%s: writing to IMCR to disable pics\n",
		    ioapics->sc_dev.dv_xname);
		outb(IMCR_ADDR, IMCR_REGISTER);
		outb(IMCR_DATA, IMCR_APIC);
	}

#if 0 /* XXX Will be removed when we have intrsource. */
	isa_nodefaultirq();
#endif
			
	for (sc = ioapics; sc != NULL; sc = sc->sc_next) {
		if (mp_verbose)
			printf("%s: enabling\n", sc->sc_dev.dv_xname);

		for (p=0; p<sc->sc_apic_sz; p++) {
			maxlevel = 0;	 /* magic */
			minlevel = 0xff; /* magic */
				
			for (q = sc->sc_pins[p].ip_handler; q != NULL;
			     q = q->ih_next) {
				if (q->ih_level > maxlevel)
					maxlevel = q->ih_level;
				if (q->ih_level < minlevel)
					minlevel = q->ih_level;
			}
			apic_vectorset(sc, p, minlevel, maxlevel);
		}
	}
}

/*
 * Interrupt handler management with the apic is radically different from the
 * good old 8259.
 *
 * The APIC adds an additional level of indirection between interrupt
 * signals and interrupt vectors in the IDT.
 * It also encodes a priority into the high-order 4 bits of the IDT vector
 * number.
 *
 *
 * interrupt establishment:
 *	-> locate interrupt pin.
 *	-> locate or allocate vector for pin.
 *	-> locate or allocate handler chain for vector.
 *	-> chain interrupt into handler chain.
 * 	#ifdef notyet
 *	-> if level of handler chain increases, reallocate vector, move chain.
 *	#endif
 */

void *
apic_intr_establish(int irq, int type, int level, int (*ih_fun)(void *),
    void *ih_arg, char *ih_what)
{
	unsigned int ioapic = APIC_IRQ_APIC(irq);
	unsigned int intr = APIC_IRQ_PIN(irq);
	struct ioapic_softc *sc = ioapic_find(ioapic);
	struct ioapic_pin *pin;
	struct intrhand **p, *q, *ih;
	static struct intrhand fakehand = {fakeintr};
	extern int cold;
	int minlevel, maxlevel;

	if (sc == NULL)
		panic("apic_intr_establish: unknown ioapic %d", ioapic);

	if ((irq & APIC_INT_VIA_APIC) == 0)
		panic("apic_intr_establish of non-apic interrupt 0x%x", irq);

	if (intr >= sc->sc_apic_sz || type == IST_NONE)
		panic("apic_intr_establish: bogus intr or type");

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("apic_intr_establish: can't malloc handler info");

	pin = &sc->sc_pins[intr];
	switch (pin->ip_type) {
	case IST_NONE:
		pin->ip_type = type;
		break;
	case IST_EDGE:
	case IST_LEVEL:
		if (type == pin->ip_type)
			break;
	case IST_PULSE:
		if (type != IST_NONE)
			/* XXX should not panic here! */
			panic("apic_intr_establish: "
			      "intr %d can't share %s with %s",
			      intr,
			      isa_intr_typename(sc->sc_pins[intr].ip_type),
			      isa_intr_typename(type));
		break;
	}

	/*
	 * Figure out where to put the handler.
	 * This is O(N^2) to establish N interrupts, but we want to
	 * preserve the order, and N is generally small.
	 */
	maxlevel = level;
	minlevel = level;
	for (p = &pin->ip_handler; (q = *p) != NULL; p = &q->ih_next) {
		if (q->ih_level > maxlevel)
			maxlevel = q->ih_level;
		if (q->ih_level < minlevel)
			minlevel = q->ih_level;
	}

	/*
	 * Actually install a fake handler momentarily, since we might be doing
	 * this with interrupts enabled and don't want the real routine called
	 * until masking is set up.
	 */
	fakehand.ih_level = level;
	*p = &fakehand;

	/*
	 * Fix up the vector for this pin.
	 * (if cold, defer this until most interrupts have been established,
	 * to avoid too much thrashing of the idt..)
	 */

	if (!ioapic_cold)
		apic_vectorset(sc, intr, minlevel, maxlevel);

#if 0
	apic_calculatemasks();
#endif

	/*
	 * Poke the real handler in now.
	 */
	ih->ih_fun = ih_fun;
	ih->ih_arg = ih_arg;
	ih->ih_next = NULL;
	ih->ih_level = level;
	ih->ih_irq = irq;
	evcount_attach(&ih->ih_count, ih_what, (void *)&pin->ip_vector,
	    &evcount_intr);
	*p = ih;

	return (ih);
}

/*
 * apic disestablish:
 *	locate handler chain.
 * 	dechain intrhand from handler chain
 *	if chain empty {
 *		reprogram apic for "safe" vector.
 *		free vector (point at stray handler).
 *	}
 *	#ifdef notyet
 *	else {
 *		recompute level for current chain.
 *		if changed, reallocate vector, move chain.
 *	}
 *	#endif
 */

void
apic_intr_disestablish(void *arg)
{
	struct intrhand *ih = arg;
	int irq = ih->ih_irq;
	unsigned int ioapic = APIC_IRQ_APIC(irq);
	unsigned int intr = APIC_IRQ_PIN(irq);
	struct ioapic_softc *sc = ioapic_find(ioapic);
	struct ioapic_pin *pin = &sc->sc_pins[intr];
	struct intrhand **p, *q;
	int minlevel, maxlevel;

	if (sc == NULL)
		panic("apic_intr_disestablish: unknown ioapic %d", ioapic);

	if (intr >= sc->sc_apic_sz)
		panic("apic_intr_disestablish: bogus irq");

	/*
	 * Remove the handler from the chain.
	 * This is O(n^2), too.
	 */
	maxlevel = 0;
	minlevel = 0xff;
	for (p = &pin->ip_handler; (q = *p) != NULL && q != ih;
	     p = &q->ih_next) {
		if (q->ih_level > maxlevel)
			maxlevel = q->ih_level;
		if (q->ih_level < minlevel)
			minlevel = q->ih_level;
	}

	if (q)
		*p = q->ih_next;
	else
		panic("intr_disestablish: handler not registered");
	for (; q != NULL; q = q->ih_next) {
		if (q->ih_level > maxlevel)
			maxlevel = q->ih_level;
		if (q->ih_level < minlevel)
			minlevel = q->ih_level;
	}

	if (!ioapic_cold)
		apic_vectorset(sc, intr, minlevel, maxlevel);

	evcount_detach(&ih->ih_count);
	free(ih, M_DEVBUF);
}

#ifdef DDB
void ioapic_dump(void);

void
ioapic_dump(void)
{
	struct ioapic_softc *sc;
	struct ioapic_pin *ip;
	int p;

	for (sc = ioapics; sc != NULL; sc = sc->sc_next) {
		for (p = 0; p < sc->sc_apic_sz; p++) {
			ip = &sc->sc_pins[p];
			if (ip->ip_type != IST_NONE)
				ioapic_print_redir(sc, "dump", p);
		}
	}
}
#endif
