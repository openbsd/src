/*	$OpenBSD: siopdma.c,v 1.7 2000/01/06 03:21:42 smurph Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
 * Copyright (c) 1994 Michael L. Hitch
 * Copyright (c) 1982, 1990 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)dma.c
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <machine/autoconf.h>
#include <mvme68k/dev/siopreg.h>
#include <mvme68k/dev/siopvar.h>

#include "mc.h"
#include "pcctwo.h"

#if NMC > 0
#include <mvme68k/dev/mcreg.h>
#endif
#if NPCCTWO > 0
#include <mvme68k/dev/pcctworeg.h>
#endif

int   afscmatch   __P((struct device *, void *, void *));
void  afscattach  __P((struct device *, struct device *, void *));

int   siopintr __P((struct siop_softc *));
int   afsc_dmaintr   __P((struct siop_softc *));

struct scsi_adapter afsc_scsiswitch = {
	siop_scsicmd,
	siop_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
};

struct scsi_device afsc_scsidev = {
	NULL,		/* use default error handler */
	NULL,		/* do not have a start function */
	NULL,		/* have no async handler */
	NULL,		/* Use default done routine */
};

struct cfattach siop_ca = {
	sizeof(struct siop_softc), afscmatch, afscattach,
};

struct cfdriver siop_cd = {
	NULL, "siop", DV_DULL, 0
};

int
afscmatch(pdp, vcf, args)
struct device *pdp;
void *vcf, *args;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = args;

	return (!badvaddr(ca->ca_vaddr, 4));
}

void
afscattach(parent, self, auxp)
struct device *parent, *self;
void *auxp;
{
	struct siop_softc *sc = (struct siop_softc *)self;
	struct confargs *ca = auxp;
	siop_regmap_p rp;
	int tmp;
	extern int cpuspeed;

	sc->sc_siopp = rp = ca->ca_vaddr;
	/*
	 * siop uses sc_clock_freq to define the dcntl & ctest7 reg values
	 * (was 0x0221, but i added SIOP_CTEST7_SC0 for snooping control)
	 * XXX does the clock frequency change for the 33MHz processors?
	 */
	sc->sc_clock_freq = cpuspeed * 2;
#ifdef MVME177
	/* MVME177 siop clock documented as fixed 50Mhz in VME177A/HX */
	if (cputyp == CPU_177)
		sc->sc_clock_freq = 50;
#endif
#ifdef MVME172
	/* XXX this is a guess! Same as MVME177?*/
	if (cputyp == CPU_172)
		sc->sc_clock_freq = 50;
#endif
	sc->sc_dcntl = SIOP_DCNTL_EA; 
/*XXX*/	if (sc->sc_clock_freq <= 25)
/*XXX*/		sc->sc_dcntl |= (2 << 6);
/*XXX*/	else if (sc->sc_clock_freq <= 37)
/*XXX*/		sc->sc_dcntl |= (1 << 6);
/*XXX*/	else if (sc->sc_clock_freq <= 50)
/*XXX*/		sc->sc_dcntl |= (0 << 6);
/*XXX*/	else
/*XXX*/		sc->sc_dcntl |= (3 << 6);

#if defined(MVME172) || defined(MVME177)  /* No Select timouts on MC68060 */
	if (cputyp == CPU_172 || cputyp == CPU_172)
		sc->sc_ctest7 = SIOP_CTEST7_SNOOP | SIOP_CTEST7_TT1 | SIOP_CTEST7_STD;
	else
#endif 
		sc->sc_ctest7 = SIOP_CTEST7_SNOOP | SIOP_CTEST7_TT1;

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = 7;		/* XXXX should ask ROM */
	sc->sc_link.adapter = &afsc_scsiswitch;
	sc->sc_link.device = &afsc_scsidev;
	sc->sc_link.openings = 1;

	sc->sc_ih.ih_fn = afsc_dmaintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_ipl = ca->ca_ipl;

	siopinitialize(sc);

	switch (ca->ca_bustype) {
#if NMC > 0
		case BUS_MC:
			{
				struct mcreg *mc = (struct mcreg *)ca->ca_master;

				mcintr_establish(MCV_NCR, &sc->sc_ih);
				mc->mc_ncrirq = ca->ca_ipl | MC_IRQ_IEN;
				break;
			}
#endif
#if NPCCTWO > 0
		case BUS_PCCTWO:
			{
				struct pcctworeg *pcc2 = (struct pcctworeg *)ca->ca_master;

				pcctwointr_establish(PCC2V_NCR, &sc->sc_ih);
				pcc2->pcc2_ncrirq = ca->ca_ipl | PCC2_IRQ_IEN;
				break;
			}
#endif
	}

	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt);

	/*
	 * attach all scsi units on us, watching for boot device
	 * (see dk_establish).
	 */
	tmp = bootpart;
	if (ca->ca_paddr != bootaddr)
		bootpart = -1;				/* invalid flag to dk_establish */
	config_found(self, &sc->sc_link, scsiprint);
	bootpart = tmp;				 /* restore old value */

}

int
afsc_dmaintr(sc)
struct siop_softc *sc;
{
	siop_regmap_p rp;
	u_char   istat;

	rp = sc->sc_siopp;
	istat = rp->siop_istat;
	if ((istat & (SIOP_ISTAT_SIP | SIOP_ISTAT_DIP)) == 0)
		return (0);
	if ((rp->siop_sien | rp->siop_dien) == 0)
		return (0);	/* no interrupts enabled */

	/*
	 * save interrupt status, DMA status, and SCSI status 0
	 * (may need to deal with stacked interrupts?)
	 */
	sc->sc_istat = istat;
	sc->sc_dstat = rp->siop_dstat;
	sc->sc_sstat0 = rp->siop_sstat0;
	siopintr(sc);
	sc->sc_intrcnt.ev_count++;
	return (1);
}

#ifdef DEBUG
void
afsc_dump()
{
	int i;

	for (i = 0; i < afsccd.cd_ndevs; ++i)
		if (afsccd.cd_devs[i])
			siop_dump(afsccd.cd_devs[i]);
}
#endif
