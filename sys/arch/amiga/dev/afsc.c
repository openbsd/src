/*	$NetBSD: afsc.c,v 1.10 1996/01/28 19:23:24 chopps Exp $	*/

/*
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
#include <amiga/dev/zbusvar.h>

int afscprint __P((void *auxp, char *));
void afscattach __P((struct device *, struct device *, void *));
int afscmatch __P((struct device *, struct cfdata *, void *));
int siopintr __P((struct siop_softc *));
int afsc_dmaintr __P((struct siop_softc *));

struct scsi_adapter afsc_scsiswitch = {
	siop_scsicmd,
	siop_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
};

struct scsi_device afsc_scsidev = {
	NULL,		/* use default error handler */
	NULL,		/* do not have a start functio */
	NULL,		/* have no async handler */
	NULL,		/* Use default done routine */
};


#ifdef DEBUG
#endif

struct cfdriver afsccd = {
	NULL, "afsc", (cfmatch_t)afscmatch, afscattach, 
	DV_DULL, sizeof(struct siop_softc), NULL, 0 };
struct cfdriver aftsccd = {
	NULL, "afsc", (cfmatch_t)afscmatch, afscattach, 
	DV_DULL, sizeof(struct siop_softc), NULL, 0 };

/*
 * if we are a Commodore Amiga A4091 or possibly an A4000T
 */
int
afscmatch(pdp, cdp, auxp)
	struct device *pdp;
	struct cfdata *cdp;
	void *auxp;
{
	struct zbus_args *zap;
	siop_regmap_p rp;
	u_long temp, scratch;

	zap = auxp;
	if (zap->manid == 514 && zap->prodid == 84)
		return(1);		/* It's an A4091 SCSI card */
	if (!is_a4000() || !matchname(auxp, "afsc"))
		return(0);		/* Not on an A4000 or not A4000T SCSI */
#ifdef DEBUG
	printf("afscmatch: probing for A4000T\n");
#endif
	rp = ztwomap(0xdd0040);
	if (badaddr(&rp->siop_scratch) || badaddr(&rp->siop_temp)) {
#ifdef DEBUG
		printf("afscmatch: A4000T probed bad address\n");
#endif
		return(0);
	}
	scratch = rp->siop_scratch;
	temp = rp->siop_temp;
	rp->siop_scratch = 0xdeadbeef;
	rp->siop_temp = 0xaaaa5555;
#ifdef DEBUG
	printf("afscmatch: probe %x %x %x %x\n", scratch, temp,
	    rp->siop_scratch, rp->siop_temp);
#endif
	if (rp->siop_scratch != 0xdeadbeef || rp->siop_temp != 0xaaaa5555)
		return(0);
	rp->siop_scratch = scratch;
	rp->siop_temp = temp;
	if (rp->siop_scratch != scratch || rp->siop_temp != temp)
		return(0);
	return(1);
}

void
afscattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	struct siop_softc *sc;
	struct zbus_args *zap;
	siop_regmap_p rp;

	printf("\n");

	zap = auxp;

	sc = (struct siop_softc *)dp;
	if (zap->manid == 514 && zap->prodid == 84)
		sc->sc_siopp = rp = zap->va + 0x00800000;
	else
		sc->sc_siopp = rp = ztwomap(0xdd0040);

	/*
	 * CTEST7 = 80 [disable burst]
	 */
	sc->sc_clock_freq = 50;		/* Clock = 50Mhz */
	sc->sc_ctest7 = 0x80;		/* CDIS */

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = 7;
	sc->sc_link.adapter = &afsc_scsiswitch;
	sc->sc_link.device = &afsc_scsidev;
	sc->sc_link.openings = 2;

	siopinitialize(sc);

	sc->sc_isr.isr_intr = afsc_dmaintr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = 2;
	add_isr (&sc->sc_isr);

	/*
	 * attach all scsi units on us
	 */
	config_found(dp, &sc->sc_link, afscprint);
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
		return(UNCONF);
	return(QUIET);
}

int
afsc_dmaintr(sc)
	struct siop_softc *sc;
{
	siop_regmap_p rp;
	u_char istat;

	if (sc->sc_flags & SIOP_INTSOFF)
		return (0);	/* interrupts are not active */
	rp = sc->sc_siopp;
	istat = rp->siop_istat;
	if ((istat & (SIOP_ISTAT_SIP | SIOP_ISTAT_DIP)) == 0)
		return(0);
	/*
	 * save interrupt status, DMA status, and SCSI status 0
	 * (may need to deal with stacked interrupts?)
	 */
	sc->sc_sstat0 = rp->siop_sstat0;
	sc->sc_istat = istat;
	sc->sc_dstat = rp->siop_dstat;
	siopintr(sc);
	return(1);
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
