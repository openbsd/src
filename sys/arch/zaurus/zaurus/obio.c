/*	$NetBSD: obio.c,v 1.2 2003/07/15 00:25:06 lukem Exp $ */

/*
 * Copyright (c) 2002, 2003  Genetec Corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec Corporation may not be used to endorse or 
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * TODO: dispatch interrupt to SOFTSERIAL or SOFTNET according to requested
 *       interrupt level.
 */

#include <sys/cdefs.h>
/*
__KERNEL_RCSID(0, "$NetBSD: obio.c,v 1.2 2003/07/15 00:25:06 lukem Exp $");
*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/reboot.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <arm/cpufunc.h>

#include <arm/mainbus/mainbus.h>
#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_gpio.h>
#include <arm/sa11x0/sa11x0_var.h>
#include <machine/zaurus_reg.h>
#include <machine/zaurus_var.h>

/* prototypes */
static int	obio_match(struct device *, void *, void *);
static void	obio_attach(struct device *, struct device *, void *);
static int 	obio_search(struct device *, struct cfdata *, void *);
static int	obio_print(void *, const char *);

/* attach structures */
struct cfattach obio_ca = {
	sizeof (struct obio_softc), obio_match, obio_attach
};

struct cfdriver obio_cd = {
	NULL, "obio", DV_DULL
};


uint32_t obio_intr_mask;

static int
obio_spurious(void *arg)
{
	int irqno = (int)arg;

	printf("Spurious interrupt %d on On-board peripheral", irqno);
	return 1;
}


/*
 * interrupt handler for GPIO0 (on-board peripherals)
 *
 * On Lubbock, 8 interrupts are ORed through on-board logic,
 * and routed to GPIO0 of PXA250 processor.
 */
static int	
obio_intr(void *arg)
{
	int irqno, pending, mask;
	struct obio_softc *sc = (struct obio_softc *)arg;
	int psw;

	mask = sc->sc_obio_intr_mask; /* real irq mask for obio */

	psw = disable_interrupts(I32_bit|F32_bit);
	pending = bus_space_read_2(sc->sc_iot, sc->sc_obioreg_ioh,
	    LUBBOCK_INTRCTL);
	/* Here is a chance to lose some interrupts.
	 * You need to modify FPGA program to avoid it 
	 */
	bus_space_write_2(sc->sc_iot, sc->sc_obioreg_ioh, LUBBOCK_INTRCTL, 0);
	restore_interrupts(psw);


	pending &= mask;
	while (pending) {
		irqno = 0;

		for ( ;pending; ++irqno) {
			if (0 == (pending & (1U<<irqno)))
				continue;
			pending &= ~(1U<<irqno);

#ifdef notyet
			/* if ipl of this irq is higher than current spl level,
			   call the handler directly instead of dispatching it to
			   software interrupt. */
			if (sc->sc_handler[irqno].level > current_spl_level) {
				(* sc->sc_handler[irqno].func)(
					sc->sc_handler[irqno].arg );
			}
			else 
#endif
			{
				/* mask this interrupt until software
				   interrupt is handled. */
				sc->sc_obio_intr_pending |= (1U<<irqno);
				mask &= ~(1U<<irqno);
				bus_space_write_4(sc->sc_iot, sc->sc_obioreg_ioh, 
				    LUBBOCK_INTRMASK, mask);

				/* handle it later */
				softintr_schedule(sc->sc_si);
			}
		}

		psw = disable_interrupts(I32_bit|F32_bit);
		pending = bus_space_read_2(sc->sc_iot, sc->sc_obioreg_ioh,
		    LUBBOCK_INTRCTL);
		bus_space_write_2(sc->sc_iot, sc->sc_obioreg_ioh, 
		    LUBBOCK_INTRCTL,0);
		restore_interrupts(psw);
		pending &= mask;
	}

	/* GPIO interrupt is edge triggered.  make a pulse
	   to let Cotulla notice when other interrupts are
	   still pending */
	bus_space_write_2(sc->sc_iot, sc->sc_obioreg_ioh, LUBBOCK_INTRMASK, 0);
	bus_space_write_2(sc->sc_iot, sc->sc_obioreg_ioh, LUBBOCK_INTRMASK, mask);
	return 1;
}

static void
obio_softintr(void *arg)
{
	struct obio_softc *sc = (struct obio_softc *)arg;
	int irqno;
	int psw;
	int spl_save = current_spl_level;

	psw = disable_interrupts(I32_bit);
	while ((irqno = find_first_bit(sc->sc_obio_intr_pending)) >= 0) {
		sc->sc_obio_intr_pending &= ~(1U<<irqno);

		restore_interrupts(psw);

		_splraise(sc->sc_handler[irqno].level);
		(* sc->sc_handler[irqno].func)(
			sc->sc_handler[irqno].arg);
		splx(spl_save);
		
		psw = disable_interrupts(I32_bit);
	}

	bus_space_write_4(sc->sc_iot, sc->sc_obioreg_ioh, 
	    LUBBOCK_INTRMASK, sc->sc_obio_intr_mask);

	restore_interrupts(psw);
}

/*
 * int obio_print(void *aux, const char *name)
 * print configuration info for children
 */

static int
obio_print(void *aux, const char *name)
{
	struct obio_attach_args *oba = (struct obio_attach_args*)aux;

	if (oba->oba_addr != -1)
                printf(" addr 0x%lx", oba->oba_addr);
        if (oba->oba_intr > 0)
                printf(" intr %d", oba->oba_intr);
        return (UNCONF);
}

int
obio_match(struct device *parent, void *match, void *aux)
{
	return 1;
}

void
obio_attach(struct device *parent, struct device *self, void *aux)
{
	struct obio_softc *sc = (struct obio_softc*)self;
	int system_id, baseboard_id, expansion_id, processor_card_id;
	struct pxaip_attach_args *sa = (struct pxaip_attach_args *)aux;
	char *processor_card_name;
	int i;
	

	/* Map on-board FPGA registers */
	sc->sc_iot = &pxa2x0_bs_tag;
	if (bus_space_map(sc->sc_iot, LUBBOCK_OBIO_PBASE, LUBBOCK_OBIO_SIZE,
	    0, &(sc->sc_obioreg_ioh))) {
		printf("%s: can't map FPGA registers\n", self->dv_xname);
	}

	system_id = bus_space_read_4(sc->sc_iot, sc->sc_obioreg_ioh,
	    LUBBOCK_SYSTEMID);

	baseboard_id = (system_id>>8) & 0x0f;
	expansion_id = (system_id>>4) & 0x0f;
	processor_card_id = system_id & 0x0f;

	switch (processor_card_id) {
	case 0: processor_card_name = "Cotulla"; break;
	case 1: processor_card_name = "Sabinal"; break;
	default: processor_card_name = "(unknown)";
	}

	printf(" : baseboard=%d (%s), expansion card=%d, processor card=%d (%s)\n",
	       baseboard_id,
	       baseboard_id==8 ? "DBPXA250(lubbock)" : "(unknown)",
	       expansion_id,
	       processor_card_id, processor_card_name );

	/*
	 *  Mask all interrupts.
	 *  They are later unmasked at each device's attach routine.
	 */
	bus_space_write_4(sc->sc_iot, sc->sc_obioreg_ioh,
	    LUBBOCK_INTRMASK,0);

	sc->sc_intr = sa->pxa_intr;	/* irq no. on ICU. */
	sc->sc_obio_intr_mask = 0;	/* No interrupt used */
	sc->sc_obio_intr_pending = 0;
	sc->sc_ipl = IPL_BIO;

	for (i=0; i < N_OBIO_IRQ; ++i) {
		sc->sc_handler[i].func = obio_spurious;
		sc->sc_handler[i].arg = (void *)i;
	}


	/*
	 * establish interrupt handler.
	 */
#if 0
	/*
	 * level is lowest at first, and changed when
	 * sub-interrupt handlers are established
	 */
	sc->sc_ipl = IPL_BIO;
#else
	/*
	 * level is very high to allow high priority sub-interrupts.
	 */
	sc->sc_ipl = IPL_AUDIO;
#endif
	sc->sc_ih = pxa2x0_gpio_intr_establish(0, IST_EDGE_FALLING, sc->sc_ipl,
	    obio_intr, sc, sc->sc_dev.dv_xname);
	sc->sc_si = softintr_establish(IPL_SOFTNET, obio_softintr, sc);


	/*
	 *  Attach each devices
	 */
	obio_search(self, NULL, NULL);
}

int
obio_search(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct obio_softc *sc = (struct obio_softc *)parent;
	struct obio_attach_args oba;

	oba.oba_sc = sc;
        oba.oba_iot = sc->sc_iot;
        oba.oba_addr = -1;
        oba.oba_intr = -1;

	config_found(parent, &oba, obio_print);

        return 0;
}

void *
obio_intr_establish(struct obio_softc *sc,
		    int irq, int ipl, int (*func)(void *), void *arg)
{
	int psw;

	if (irq < 0 || N_OBIO_IRQ <= irq)
		panic("Bad irq no for obio");

	psw = disable_interrupts(I32_bit);

	sc->sc_handler[irq].func = func;
	sc->sc_handler[irq].arg = arg;
	sc->sc_handler[irq].level = ipl;

#ifdef notyet
	if (ipl > sc->sc_ipl) {
		pxa2x0_update_intr_masks(sc->sc_intr, ipl);
		sc->sc_ipl = ipl;
	}
#endif

	sc->sc_obio_intr_mask |= (1U << irq);
	bus_space_write_4(sc->sc_iot, sc->sc_obioreg_ioh,
	    LUBBOCK_INTRMASK, sc->sc_obio_intr_mask);

	enable_interrupts(psw);
	return &sc->sc_handler[irq];
}
