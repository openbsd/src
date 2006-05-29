/*	$NetBSD: com_obio.c,v 1.9 2005/12/11 12:17:09 christos Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com>.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/termios.h>

#include <machine/bus.h>

#include <arm/xscale/i80321var.h>
#include <armish/dev/obiovar.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

struct com_obio_softc {
	struct com_softc sc_com;

	void *sc_ih;
};

int	com_obio_match(struct device *, void *, void *);
void	com_obio_attach(struct device *, struct device *, void *);

struct cfattach com_obio_ca = {
	sizeof(struct com_obio_softc), com_obio_match, com_obio_attach
};

struct cfdriver com_obio_cd = {
	NULL, "com_obio", DV_DULL
};

int
com_obio_match(struct device *parent, void *cf, void *aux)
{

	/* We take it on faith that the device is there. */
	return (1);
}

int comintr0(void *a);
int comintr1(void *a);
int comintr2(void *a);
int comintr3(void *a);
int
comintr0(void *a)
{
	return comintr(a);
}
int
comintr1(void *a)
{
	return comintr(a);
}
int
comintr2(void *a)
{
	return comintr(a);
}
int
comintr3(void *a)
{
	return comintr(a);
}

uint32_t get_pending_irq(void);

struct com_softc *console_comsc;
int poll_console(void);
int poll_console()
{
	int ret;
#if 0
	uint32_t pending0, pending1;
	pending0 = get_pending_irq();
#endif
	ret = comintr(console_comsc);
#if 0
	if (ret != 0) {
		pending1 = get_pending_irq();
		printf("serviced com irq, opending %x npending %x\n",
		    pending0, pending1);
	}
#endif
	return ret;
}
void
com_obio_attach(struct device *parent, struct device *self, void *aux)
{
	struct obio_attach_args *oba = aux;
	struct com_obio_softc *osc = (void *) self;
	struct com_softc *sc = &osc->sc_com;
	int error;

	console_comsc = sc;
	sc->sc_iot = oba->oba_st;
	sc->sc_iobase = oba->oba_addr;
	sc->sc_frequency = COM_FREQ;
/* 	sc->sc_hwflags = COM_HW_NO_TXPRELOAD; */
	sc->sc_hwflags = 0;
	error = bus_space_map(sc->sc_iot, oba->oba_addr, 8, 0, &sc->sc_ioh);

	if (error) {
		printf(": failed to map registers: %d\n", error);
		return;
	}

	com_attach_subr(sc);
	oba->oba_irq = 0x1c;
#if 1
	osc->sc_ih = i80321_intr_establish(oba->oba_irq, IPL_TTY,
	    comintr, sc, sc->sc_dev.dv_xname);
#else
#define	ICU_INT_XINT0 27
#define	ICU_INT_XINT(x)	((x) + ICU_INT_XINT0)
#if 1
	osc->sc_ih = i80321_intr_establish(ICU_INT_XINT(0), IPL_TTY,
	    comintr0, sc, sc->sc_dev.dv_xname);
#endif
	osc->sc_ih = i80321_intr_establish(ICU_INT_XINT(1), IPL_TTY,
	    comintr1, sc, sc->sc_dev.dv_xname);
	osc->sc_ih = i80321_intr_establish(ICU_INT_XINT(2), IPL_TTY,
	    comintr2, sc, sc->sc_dev.dv_xname);
	osc->sc_ih = i80321_intr_establish(ICU_INT_XINT(3), IPL_TTY,
	    comintr3, sc, sc->sc_dev.dv_xname);
#endif
	if (osc->sc_ih == NULL)
		printf("%s: unable to establish interrupt at CPLD irq %d\n",
		    sc->sc_dev.dv_xname, oba->oba_irq);
}
