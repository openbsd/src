/*	$OpenBSD: empsc.c,v 1.3 1996/05/29 10:14:51 niklas Exp $ */
/*	$NetBSD: empsc.c,v 1.7 1996/05/10 13:02:33 is Exp $	*/

/*

 * Copyright (c) 1995 Sean Riddle, Bo Najdrovsky
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
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/scireg.h>
#include <amiga/dev/scivar.h>
#include <amiga/dev/zbusvar.h>

int empscprint __P((void *auxp, char *));
void empscattach __P((struct device *, struct device *, void *));
int empscmatch __P((struct device *, void *, void *));
int empsc_intr __P((void *));

struct scsi_adapter empsc_scsiswitch = {
	sci_scsicmd,
	sci_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
};

struct scsi_device empsc_scsidev = {
	NULL,		/* use default error handler */
	NULL,		/* do not have a start functio */
	NULL,		/* have no async handler */
	NULL,		/* Use default done routine */
};

#ifdef DEBUG
extern int sci_debug;
#endif

extern int sci_data_wait;

struct cfattach empsc_ca = {
	sizeof(struct sci_softc), empscmatch, empscattach
};

struct cfdriver empsc_cd = {
	NULL, "empsc", DV_DULL, NULL, 0
};

/*
 * if this is an EMPLANT board
 */
int
empscmatch(pdp, match, auxp)
	struct device *pdp;
	void *match, *auxp;
{
	struct zbus_args *zap;

	zap = auxp;

	/*
	 * Check manufacturer and product id.
	 */
	if (zap->manid == 2171 && ((zap->prodid == 21)||(zap->prodid==32)))
		return(1);
	else
		return(0);
}

void
empscattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	volatile u_char *rp;
	struct sci_softc *sc;
	struct zbus_args *zap;

	printf("\n");

	zap = auxp;
	
	sc = (struct sci_softc *)dp;
	rp = zap->va + 0x5000;

	sc->sci_data = rp;
	sc->sci_odata = rp;
	sc->sci_icmd = rp + 0x10;
	sc->sci_mode = rp + 0x20;
	sc->sci_tcmd = rp + 0x30;
	sc->sci_bus_csr = rp + 0x40;
	sc->sci_sel_enb = rp + 0x40;
	sc->sci_csr = rp + 0x50;
	sc->sci_dma_send = rp + 0x50;
	sc->sci_idata = rp + 0x60;
	sc->sci_trecv = rp + 0x60;
	sc->sci_iack = rp + 0x70;
	sc->sci_irecv = rp + 0x70;
	sc->sc_isr.isr_intr = empsc_intr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = 2;
	add_isr(&sc->sc_isr);

	scireset(sc);

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = 7;
	sc->sc_link.adapter = &empsc_scsiswitch;
	sc->sc_link.device = &empsc_scsidev;
	sc->sc_link.openings = 1;
	TAILQ_INIT(&sc->sc_xslist);

	/*
	 * attach all scsi units on us
	 */
	config_found(dp, &sc->sc_link, empscprint);
}

/*
 * print diag if pnp is NULL else just extra
 */
int
empscprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	if (pnp == NULL)
		return(UNCONF);
	return(QUIET);
}

int
empsc_intr(arg)
	void *arg;
{
	struct sci_softc *dev = arg;
	u_char stat;

	if ((*dev->sci_csr & SCI_CSR_INT) == 0)
		return(0);
	stat = *dev->sci_iack;
	/* XXXX is: something is missing here, at least a: */
	return(1);
}
