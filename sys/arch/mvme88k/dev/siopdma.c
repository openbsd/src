/*	$OpenBSD: siopdma.c,v 1.2 1998/12/15 05:52:31 smurph Exp $ */

/*
 * Copyright (c) 1996 Nivas Madhur
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
#if defined(MVME187)
#include <machine/board.h>
#endif /* MVME187 */

#if defined(MVME187)
#include <mvme88k/dev/siopreg.h>
#include <mvme88k/dev/siopvar.h>
#else
#include <mvme68k/dev/siopreg.h>
#include <mvme68k/dev/siopvar.h>
#endif /* defined(MVME187) */

#if !defined(MVME187)
#include "mc.h"
#endif /* MVME187 */
#include "pcctwo.h"

#if NMC > 0
#include <mvme68k/dev/mcreg.h>
#endif
#if NPCCTWO > 0
#if defined(MVME187)
#include <mvme88k/dev/pcctworeg.h>
#else
#include <mvme68k/dev/pcctworeg.h>
#endif /* defined(MVME187) */
#endif

#if defined(MVME187)
#include "machine/mmu.h"
#endif /* defined(MVME187) */

int	afscmatch	__P((struct device *, void *, void *));
void	afscattach	__P((struct device *, struct device *, void *));

int	afscprint	__P((void *auxp, char *));
int	siopintr	__P((struct siop_softc *));
int	afsc_dmaintr	__P((struct siop_softc *));

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
	int ret;

	if ((ret = badvaddr(IIOV(ca->ca_vaddr), 4)) <=0){
	    printf("==> siop: failed address check returning %ld.\n", ret);
	    return(0);
	}

	return (1);
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
	/* XXX this is a guess! */
	if (cputyp == CPU_177)
		sc->sc_clock_freq = cpuspeed;
#endif
	sc->sc_dcntl = SIOP_DCNTL_EA;
/*X*/	if (sc->sc_clock_freq <= 25)
/*X*/		sc->sc_dcntl |= (2 << 6);
/*X*/	else if (sc->sc_clock_freq <= 37)
/*X*/		sc->sc_dcntl |= (1 << 6);
/*X*/	else if (sc->sc_clock_freq <= 50)
/*X*/		sc->sc_dcntl |= (0 << 6);
/*X*/	else
/*X*/		sc->sc_dcntl |= (3 << 6);

	sc->sc_ctest0 = SIOP_CTEST0_BTD | SIOP_CTEST0_EAN;

#ifdef MVME187
	/*
	 * MVME187 doesn't implement snooping...
	 */
	sc->sc_ctest7 = SIOP_CTEST7_TT1;
#else
	sc->sc_ctest7 = SIOP_CTEST7_SNOOP | SIOP_CTEST7_TT1 | SIOP_CTEST7_STD;
#endif /* MVME187 */

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
#if defined(MVME187)
		/*
		 * Disable caching for the softc. Actually, I want
		 * to disable cache for acb structures, but they are
		 * part of softc, and I am disabling the entire softc
		 * just in case.
		 */

		struct pcctworeg *pcc2 = (struct pcctworeg *)ca->ca_master;
		
		pmap_cache_ctrl(pmap_kernel(), M88K_TRUNC_PAGE((vm_offset_t)sc),
			M88K_ROUND_PAGE((vm_offset_t)sc + sizeof(*sc)),
			CACHE_INH);
#endif
		pcctwointr_establish(PCC2V_NCR, &sc->sc_ih);
/*		intr_establish(PCC2_VECT + SCSIIRQ, &sc->sc_ih);*/
		/* enable interrupts at ca_ipl */
		pcc2->pcc2_ncrirq = ca->ca_ipl | PCC2_IRQ_IEN;
/*		pcc2->pcc2_scsiirq = ca->ca_ipl | PCC2_SCSIIRQ_IEN;*/
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
		bootpart = -1;          /* invalid flag to dk_establish */
	config_found(self, &sc->sc_link, scsiprint);
	bootpart = tmp;             /* restore old value */
}
/*
 * print diag if pnp is NULL else just extra
 */
int
afscprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	if (pnp == NULL)
		return (UNCONF);
	return (QUIET);
}

int
afsc_dmaintr(sc)
	struct siop_softc *sc;
{
	siop_regmap_p rp;
	u_char	istat;

	rp = sc->sc_siopp;
	istat = rp->siop_istat;
	if ((istat & (SIOP_ISTAT_SIP | SIOP_ISTAT_DIP)) == 0)
		return (0);
	if ((rp->siop_sien | rp->siop_dien) == 0)
		return (0);	/* no interrupts enabled */

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
	if (istat & SIOP_ISTAT_SIP) {
		sc->sc_sstat0 = rp->siop_sstat0;
	}
	if (istat & SIOP_ISTAT_DIP) {
		delay(3);
		sc->sc_dstat = rp->siop_dstat;
	}
	siopintr(sc);
	sc->sc_intrcnt.ev_count++;
	return (1);
}

#ifdef XXX_CD_DEBUG /* XXXsmurph What is afsccd ?? */
void
afsc_dump()
{
	int i;

	for (i = 0; i < afsccd.cd_ndevs; ++i)
		if (afsccd.cd_devs[i])
			siop_dump(afsccd.cd_devs[i]);
}
#endif
