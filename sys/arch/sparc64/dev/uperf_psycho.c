/*	$OpenBSD: uperf_psycho.c,v 1.1 2002/01/30 23:58:02 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/autoconf.h>

#include <arch/sparc64/dev/iommureg.h>
#include <arch/sparc64/dev/psychoreg.h>
#include <arch/sparc64/dev/uperfvar.h>
#include <arch/sparc64/dev/uperf_psychovar.h>

int uperf_psycho_match __P((struct device *, void *, void *));
void uperf_psycho_attach __P((struct device *, struct device *, void *));

struct uperf_psycho_softc {
	struct uperf_softc	sc_usc;
	struct perfmon		*sc_pm;
};

struct cfattach uperf_psycho_ca = {
	sizeof(struct uperf_psycho_softc), uperf_psycho_match, uperf_psycho_attach
};

int uperf_psycho_getcnt __P((void *, int, u_int32_t *, u_int32_t *));
int uperf_psycho_clrcnt __P((void *, int));
int uperf_psycho_getcntsrc __P((void *, int, u_int *, u_int *));
int uperf_psycho_setcntsrc __P((void *, int, u_int, u_int));

struct uperf_src uperf_psycho_srcs[] = {
	{ UPERFSRC_SDVRA, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_SDVRA },
	{ UPERFSRC_SDVWA, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_SDVWA },
	{ UPERFSRC_CDVRA, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_CDVRA },
	{ UPERFSRC_CDVWA, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_CDVWA },
	{ UPERFSRC_SBMA, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_SBMA },
	{ UPERFSRC_DVA, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_DVA },
	{ UPERFSRC_DVWA, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_DVWA },
	{ UPERFSRC_PIOA, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_PIOA },
	{ UPERFSRC_SDVRB, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_SDVRB },
	{ UPERFSRC_SDVWB, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_SDVWB },
	{ UPERFSRC_CDVRB, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_CDVRB },
	{ UPERFSRC_CDVWB, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_CDVWB },
	{ UPERFSRC_SBMB, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_SBMB },
	{ UPERFSRC_DVB, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_DVB },
	{ UPERFSRC_DVWB, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_DVWB },
	{ UPERFSRC_PIOB, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_PIOB },
	{ UPERFSRC_TLBMISS, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_TLBMISS },
	{ UPERFSRC_NINTRS, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_NINTRS },
	{ UPERFSRC_INACK, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_INACK },
	{ UPERFSRC_PIOR, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_PIOR },
	{ UPERFSRC_PIOW, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_PIOW },
	{ UPERFSRC_MERGE, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_MERGE },
	{ UPERFSRC_TBLA, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_TBLA },
	{ UPERFSRC_STCA, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_STCA },
	{ UPERFSRC_TBLB, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_TBLB },
	{ UPERFSRC_STCB, UPERF_CNT0|UPERF_CNT1, PSY_PMCRSEL_STCB },
	{ -1, -1, 0 }
};

int
uperf_psycho_match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct uperf_psycho_attach_args *upaa = aux;

	return (strcmp(upaa->upaa_name, "uperf") == 0);
}

void
uperf_psycho_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct uperf_psycho_attach_args *upaa = aux;
	struct uperf_psycho_softc *sc = (struct uperf_psycho_softc *)self;

	sc->sc_pm = upaa->upaa_regs;
	sc->sc_usc.usc_cookie = sc;
	sc->sc_usc.usc_getcntsrc = uperf_psycho_getcntsrc;
	sc->sc_usc.usc_setcntsrc = uperf_psycho_setcntsrc;
	sc->sc_usc.usc_clrcnt = uperf_psycho_clrcnt;
	sc->sc_usc.usc_getcnt = uperf_psycho_getcnt;
	sc->sc_usc.usc_srcs = uperf_psycho_srcs;

	printf("\n");
}

int
uperf_psycho_clrcnt(vsc, flags)
	void *vsc;
	int flags;
{
	struct uperf_psycho_softc *sc = vsc;
	u_int64_t cr = sc->sc_pm->pm_cr;

	if (flags & UPERF_CNT0)
		cr |= PSY_PMCR_CLR0;
	if (flags & UPERF_CNT1)
		cr |= PSY_PMCR_CLR1;
	sc->sc_pm->pm_cr = cr;
	return (0);
}

int
uperf_psycho_setcntsrc(vsc, flags, src0, src1)
	void *vsc;
	int flags;
	u_int src0, src1;
{
	struct uperf_psycho_softc *sc = vsc;
	u_int64_t cr = sc->sc_pm->pm_cr;

	if (flags & UPERF_CNT0) {
		cr &= ~PSY_PMCR_SEL0;
		cr |= ((src0 << 0) & PSY_PMCR_SEL0) | PSY_PMCR_CLR0;
	}
	if (flags & UPERF_CNT1) {
		cr &= ~PSY_PMCR_SEL1;
		cr |= ((src1 << 8) & PSY_PMCR_SEL1) | PSY_PMCR_CLR1;
	}
	sc->sc_pm->pm_cr = cr;
	cr = sc->sc_pm->pm_cr;
	return (0);
}

int
uperf_psycho_getcntsrc(vsc, flags, srcp0, srcp1)
	void *vsc;
	int flags;
	u_int *srcp0, *srcp1;
{
	struct uperf_psycho_softc *sc = vsc;
	u_int64_t cr = sc->sc_pm->pm_cr;

	if (flags & UPERF_CNT0)
		*srcp0 = (cr & PSY_PMCR_SEL0) >> 0;
	if (flags & UPERF_CNT1)
		*srcp1 = (cr & PSY_PMCR_SEL1) >> 8;
	return (0);
}

int
uperf_psycho_getcnt(vsc, flags, cntp0, cntp1)
	void *vsc;
	int flags;
	u_int32_t *cntp0, *cntp1;
{
	struct uperf_psycho_softc *sc = vsc;
	u_int64_t cnt = sc->sc_pm->pm_count;

	if (flags & UPERF_CNT0)
		*cntp0 = (cnt >> 32) & 0xffffffff;
	if (flags & UPERF_CNT1)
		*cntp1 = (cnt >> 0) & 0xffffffff;
	return (0);
}
