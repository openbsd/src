/*	$NetBSD: drsc.c,v 1.2 1996/05/19 19:03:01 is Exp $	*/

/*
 * Copyright (c) 1996 Ignatios Souvatzis
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
#include <amiga/amiga/custom.h>
#include <amiga/amiga/cc.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/siopreg.h>
#include <amiga/dev/siopvar.h>
#include <amiga/amiga/drcustom.h>

int drscprint __P((void *auxp, const char *));
void drscattach __P((struct device *, struct device *, void *));
int drscmatch __P((struct device *, void *, void *));
int drsc_dmaintr __P((struct siop_softc *));
#ifdef DEBUG
void drsc_dump __P((void));
#endif

struct scsi_adapter drsc_scsiswitch = {
	siop_scsicmd,
	siop_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
};

struct scsi_device drsc_scsidev = {
	NULL,		/* use default error handler */
	NULL,		/* do not have a start functio */
	NULL,		/* have no async handler */
	NULL,		/* Use default done routine */
};


#ifdef DEBUG
#endif

struct cfattach drsc_ca = {
	sizeof(struct siop_softc),
	drscmatch,
	drscattach
};

struct cfdriver drsc_cd = {
	NULL, "drsc", DV_DULL, NULL, 0 
};

static struct siop_softc *drsc_softc;

/*
 * One of us is on every DraCo motherboard, 
 */
int
drscmatch(pdp, match, auxp)
	struct device *pdp;
	void *match, *auxp;
{
	struct cfdata *cdp = (struct cfdata *)match;
	if (is_draco() && (cdp->cf_unit == 0))
		return(1);
	return(0);
}

void
drscattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	struct siop_softc *sc;
	struct zbus_args *zap;
	siop_regmap_p rp;

	printf("\n");

	zap = auxp;

	sc = (struct siop_softc *)dp;
	sc->sc_siopp = rp = (siop_regmap_p)(DRCCADDR+NBPG*DRSCSIPG);

	/*
	 * CTEST7 = TT1
	 */
	sc->sc_clock_freq = 50;		/* Clock = 50MHz */
	sc->sc_ctest7 = 0x02;

	alloc_sicallback();

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = 7;
	sc->sc_link.adapter = &drsc_scsiswitch;
	sc->sc_link.device = &drsc_scsidev;
	sc->sc_link.openings = 2;

	siopinitialize(sc);

#if 0
	sc->sc_isr.isr_intr = drsc_dmaintr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = 4;
	add_isr(&sc->sc_isr);
#else
	drsc_softc = sc;
	*draco_intpen &= ~DRIRQ_SCSI;
	*draco_intena |= DRIRQ_SCSI;
#endif
	/*
	 * attach all scsi units on us
	 */
	config_found(dp, &sc->sc_link, drscprint);
}

/*
 * print diag if pnp is NULL else just extra
 */
int
drscprint(auxp, pnp)
	void *auxp;
	const char *pnp;
{
	if (pnp == NULL)
		return(UNCONF);
	return(QUIET);
}


/*
 * Level 4 interrupt processing for the MacroSystem DraCo mainboard
 * SCSI.  Because the level 4 interrupt is above splbio, the
 * interrupt status is saved and an sicallback to the level 2 interrupt
 * handler scheduled.  This way, the actual processing of the interrupt
 * can be deferred until splbio is unblocked.
 */

void
drsc_handler()
{
	struct siop_softc *sc = drsc_softc;

	siop_regmap_p rp;
	int istat;

	if (sc->sc_flags & SIOP_INTSOFF)
		return;		/* interrupts are not active */

	rp = sc->sc_siopp;
	istat = rp->siop_istat;

	if ((istat & (SIOP_ISTAT_SIP | SIOP_ISTAT_DIP)) == 0)
		return;

	/*
	 * save interrupt status, DMA status, and SCSI status 0
	 * (may need to deal with stacked interrupts?)
	 */
	sc->sc_sstat0 = rp->siop_sstat0;
	sc->sc_istat = istat;
	sc->sc_dstat = rp->siop_dstat;
	/*
	 * disable interrupts until the callback can process this
	 * interrupt.
	 */
#ifdef DRSC_NOCALLBACK
	(void)spl1();
	siopintr(sc);
#else
	rp->siop_sien = 0;
	rp->siop_dien = 0;
	sc->sc_flags |= SIOP_INTDEFER | SIOP_INTSOFF;
	*draco_intpen &= ~DRIRQ_SCSI;
#ifdef DEBUG
	if (*draco_intpen & DRIRQ_SCSI)
		printf("%s: intpen still 0x%x\n", sc->sc_dev.dv_xname,
		    *draco_intpen);
#endif
	add_sicallback((sifunc_t)siopintr, sc, NULL);
#endif
	return;
}

#ifdef DEBUG
void
drsc_dump()
{
	int i;

	for (i = 0; i < drsc_cd.cd_ndevs; ++i)
		if (drsc_cd.cd_devs[i])
			siop_dump(drsc_cd.cd_devs[i]);
}
#endif
