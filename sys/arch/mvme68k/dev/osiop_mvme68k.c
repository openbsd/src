/*	$OpenBSD: osiop_mvme68k.c,v 1.4 2010/11/18 21:13:19 miod Exp $	*/
/*
 * Copyright (c) 2004, Miodrag Vallat.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/disklabel.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <machine/prom.h>

#include <dev/ic/osiopreg.h>
#include <dev/ic/osiopvar.h>

#include "mc.h"
#include "pcctwo.h"

#if NMC > 0
#include <mvme68k/dev/mcreg.h>
#endif
#if NPCCTWO > 0
#include <mvme68k/dev/pcctworeg.h>
#endif

void	osiop_mvme68k_attach(struct device *, struct device *, void *);
int	osiop_mvme68k_intr(void *);
int	osiop_match(struct device *, void *, void *);

struct osiop_mvme68k_softc {
	struct osiop_softc sc_osiop;
	struct intrhand sc_ih;
};

#if NMC > 0
struct cfattach osiop_mc_ca = {
	sizeof(struct osiop_mvme68k_softc),
	    osiop_match, osiop_mvme68k_attach
};
#endif

#if NPCCTWO > 0
struct cfattach osiop_pcctwo_ca = {
	sizeof(struct osiop_mvme68k_softc),
	    osiop_match, osiop_mvme68k_attach
};
#endif

int
osiop_match(struct device *parent, void *cf, void *aux)
{
	struct confargs *ca = aux;

	return (!badvaddr((vaddr_t)ca->ca_vaddr, 4));
}

void
osiop_mvme68k_attach(struct device *parent, struct device *self, void *aux)
{
	struct osiop_softc *sc = (struct osiop_softc *)self;
	struct osiop_mvme68k_softc *psc = (struct osiop_mvme68k_softc *)self;
	struct confargs *ca = aux;
	int tmp;
	extern int cpuspeed;

	sc->sc_bst = ca->ca_iot;
	sc->sc_dmat = ca->ca_dmat;

	if (bus_space_map(sc->sc_bst, (bus_addr_t)ca->ca_paddr, OSIOP_NREGS, 0,
	    &sc->sc_reg) != 0) {
		printf(": couldn't map I/O ports\n");
		return;
	}

	switch (cputyp) {
#ifdef MVME172
	case CPU_172:	/* XXX this is a guess! Same as MVME177? */
#endif
#ifdef MVME177
	case CPU_176:
	case CPU_177:
#endif
#if defined(MVME172) || defined(MVME177)
		/* MVME177 clock documented as fixed 50MHz in VME177A/HX */
		sc->sc_clock_freq = 50;
#if 0
		/* No select timeouts on MC68060 */
		/* XXX 177 works better with them! */
		sc->sc_ctest7 = OSIOP_CTEST7_SC0 | OSIOP_CTEST7_TT1 |
		    OSIOP_CTEST7_STD;
#else
		sc->sc_ctest7 = OSIOP_CTEST7_SC0 | OSIOP_CTEST7_TT1;
#endif
		break;
#endif
	default:
		/* XXX does the clock frequency change for 33MHz processors? */
		sc->sc_clock_freq = cpuspeed * 2;
		sc->sc_ctest7 = OSIOP_CTEST7_SC0 | OSIOP_CTEST7_TT1;
		break;
	}

	sc->sc_dcntl = OSIOP_DCNTL_EA;
	sc->sc_dmode = OSIOP_DMODE_BL4;	/* burst length = 4 */
	sc->sc_flags = 0;
	sc->sc_id = 7;	/* XXX should read from CNFG block in nvram */

	tmp = bootpart;
	if (ca->ca_paddr != bootaddr)
		bootpart = -1;	/* never match */

	osiop_attach(sc);

	bootpart = tmp;

	psc->sc_ih.ih_fn = osiop_mvme68k_intr;
	psc->sc_ih.ih_arg = sc;
	psc->sc_ih.ih_wantframe = 0;
	psc->sc_ih.ih_ipl = ca->ca_ipl;

	/* enable device interrupts */
	switch (ca->ca_bustype) {
#if NMC > 0
	case BUS_MC:
		mcintr_establish(MCV_NCR, &psc->sc_ih, self->dv_xname);
		sys_mc->mc_ncrirq = ca->ca_ipl | MC_IRQ_IEN;
		break;
#endif
#if NPCCTWO > 0
	case BUS_PCCTWO:
		pcctwointr_establish(PCC2V_NCR, &psc->sc_ih, self->dv_xname);
		sys_pcc2->pcc2_ncrirq = ca->ca_ipl | PCC2_IRQ_IEN;
		break;
#endif
	}
}

int
osiop_mvme68k_intr(void *arg)
{
	struct osiop_softc *sc = arg;
	u_int8_t istat;

	if (sc->sc_flags & OSIOP_INTSOFF)
		return 0;

	istat = osiop_read_1(sc, OSIOP_ISTAT);
	if ((istat & (OSIOP_ISTAT_SIP | OSIOP_ISTAT_DIP)) == 0)
		return 0;

	/*
	 * 53c710 manual recommends reading dstat and sstat0 at least
	 * 12 clk cycles apart if reading as bytes (which is what
	 * pcc2 permits). Stick in a 1us delay between accessing dstat and
	 * sstat0 below.
	 *
	 * save interrupt status, DMA status, and SCSI status 0
	 * (may need to deal with stacked interrupts?)
	 */
	sc->sc_istat = istat;
	sc->sc_sstat0 = osiop_read_1(sc, OSIOP_SSTAT0);
	DELAY(25);
	sc->sc_dstat = osiop_read_1(sc, OSIOP_DSTAT);

	osiop_intr(sc);

	return 1;
}
