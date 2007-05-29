/*	$OpenBSD: stp_sbus.c,v 1.8 2007/05/29 09:54:19 sobrado Exp $	*/
/*	$NetBSD: stp4020.c,v 1.23 2002/06/01 23:51:03 lukem Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * STP4020: SBus/PCMCIA bridge supporting one Type-3 PCMCIA card, or up to
 * two Type-1 and Type-2 PCMCIA cards..
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/extent.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/device.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciachip.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/sbus/sbusvar.h>
#include <dev/sbus/stp4020reg.h>
#include <dev/sbus/stp4020var.h>

struct stp4020_sbus_softc {
	struct stp4020_softc stp;
};

int	stpmatch(struct device *, void *, void *);
void	stpattach(struct device *, struct device *, void *);

struct cfattach stp_sbus_ca = {
	sizeof(struct stp4020_sbus_softc), stpmatch, stpattach
};

int
stpmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct sbus_attach_args *sa = aux;

	return (strcmp("SUNW,pcmcia", sa->sa_name) == 0);
}

/*
 * Attach all the sub-devices we can find
 */
void
stpattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sbus_attach_args *sa = aux;
	struct stp4020_softc *sc = (void *)self;
	int node;
	int i;
	bus_space_handle_t bh;

	node = sa->sa_node;

	/* Transfer bus tags */
	sc->sc_bustag = sa->sa_bustag;

	/* Set up per-socket static initialization */
	sc->sc_socks[0].sc = sc->sc_socks[1].sc = sc;
	sc->sc_socks[0].tag = sc->sc_socks[1].tag = sa->sa_bustag;

	if (sa->sa_nreg < 8) {
		printf(": only %d register sets\n", sa->sa_nreg);
		return;
	}

	if (sa->sa_nintr != 2) {
		printf(": expect 2 interrupt SBus levels; got %d\n",
		    sa->sa_nintr);
		return;
	}

#define STP4020_BANK_PROM	0
#define STP4020_BANK_CTRL	4
	for (i = 0; i < 8; i++) {

		/*
		 * STP4020 Register address map:
		 *	bank  0:   Forth PROM
		 *	banks 1-3: socket 0, windows 0-2
		 *	bank  4:   control registers
		 *	banks 5-7: socket 1, windows 0-2
		 */

		if (i == STP4020_BANK_PROM)
			/* Skip the PROM */
			continue;

		if (sbus_bus_map(sa->sa_bustag, sa->sa_reg[i].sbr_slot,
		    sa->sa_reg[i].sbr_offset, sa->sa_reg[i].sbr_size, 0, 0,
		    &bh) != 0) {
			printf(": attach: cannot map registers\n");
			return;
		}

		if (i == STP4020_BANK_CTRL) {
			/*
			 * Copy tag and handle to both socket structures
			 * for easy access in control/status IO functions.
			 */
			sc->sc_socks[0].regs = sc->sc_socks[1].regs = bh;
		} else if (i < STP4020_BANK_CTRL) {
			/* banks 1-3 */
			sc->sc_socks[0].windows[i-1].winaddr = bh;
			sc->sc_socks[0].wintag = sc->sc_bustag;
		} else {
			/* banks 5-7 */
			sc->sc_socks[1].windows[i-5].winaddr = bh;
			sc->sc_socks[1].wintag = sc->sc_bustag;
		}
	}

	/*
	 * We get to use two SBus interrupt levels.
	 * The higher level we use for status change interrupts;
	 * the lower level for PC card I/O.
	 */
	bus_intr_establish(sa->sa_bustag, sa->sa_intr[1].sbi_pri,
	    IPL_NONE, 0, stp4020_statintr, sc, self->dv_xname);

	bus_intr_establish(sa->sa_bustag, sa->sa_intr[0].sbi_pri,
	    IPL_NONE, 0, stp4020_iointr, sc, self->dv_xname);

	stpattach_common(sc, sa->sa_frequency);
}
