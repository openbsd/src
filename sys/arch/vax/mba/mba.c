/*	$OpenBSD: mba.c,v 1.10 2006/01/20 23:27:25 miod Exp $ */
/*	$NetBSD: mba.c,v 1.18 2000/01/24 02:40:36 matt Exp $ */
/*
 * Copyright (c) 1994, 1996 Ludd, University of Lule}, Sweden.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Simple massbus drive routine.
 * TODO:
 *  Autoconfig new devices 'on the fly'.
 *  More intelligent way to handle different interrupts.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#include <machine/trap.h>
#include <machine/scb.h>
#include <machine/nexus.h>
#include <machine/pte.h>
#include <machine/pcb.h>
#include <machine/sid.h>
#include <machine/cpu.h>

#include <vax/mba/mbareg.h>
#include <vax/mba/mbavar.h>

struct	mbaunit mbaunit[] = {
	{MBADT_RP04,	"rp04", MB_RP},
	{MBADT_RP05,	"rp05", MB_RP},
	{MBADT_RP06,	"rp06", MB_RP},
	{MBADT_RP07,	"rp07", MB_RP},
	{MBADT_RM02,	"rm02", MB_RP},
	{MBADT_RM03,	"rm03", MB_RP},
	{MBADT_RM05,	"rm05", MB_RP},
	{MBADT_RM80,	"rm80", MB_RP},
	{0,		0,	0}
};

int	mbamatch(struct device *, struct cfdata *, void *);
void	mbaattach(struct device *, struct device *, void *);
void	mbaintr(void *);
int	mbaprint(void *, const char *);
void	mbaqueue(struct mba_device *);
void	mbastart(struct mba_softc *);
void	mbamapregs(struct mba_softc *);

struct	cfattach mba_cmi_ca = {
	sizeof(struct mba_softc), mbamatch, mbaattach
};

struct	cfattach mba_sbi_ca = {
	sizeof(struct mba_softc), mbamatch, mbaattach
};

extern	struct cfdriver mba_cd;

/*
 * Look if this is a massbuss adapter.
 */
int
mbamatch(parent, cf, aux)
	struct	device *parent;
	struct  cfdata *cf;
	void	*aux;
{
	struct	sbi_attach_args *sa = (struct sbi_attach_args *)aux;

	if ((cf->cf_loc[0] != sa->nexnum) && (cf->cf_loc[0] > -1 ))
		return 0;

	if (sa->type == NEX_MBA)
		return 1;

	return 0;
}

/*
 * Attach the found massbuss adapter. Setup its interrupt vectors,
 * reset it and go searching for drives on it.
 */
void
mbaattach(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	struct	mba_softc *sc = (void *)self;
	struct	sbi_attach_args *sa = (struct sbi_attach_args *)aux;
	volatile struct	mba_regs *mbar = (struct mba_regs *)sa->nexaddr;
	struct	mba_attach_args ma;
	int	i, j;

	printf("\n");
	/*
	 * Set up interrupt vectors for this MBA.
	 */
	sc->sc_dsp = idsptch;
	sc->sc_dsp.pushlarg = sc;
	sc->sc_dsp.hoppaddr = mbaintr;
	scb->scb_nexvec[0][sa->nexnum] = scb->scb_nexvec[1][sa->nexnum] =
	    scb->scb_nexvec[2][sa->nexnum] = scb->scb_nexvec[3][sa->nexnum] =
	    &sc->sc_dsp;

	sc->sc_physnr = sa->nexnum - 8; /* MBA's have TR between 8 - 11... */
#if VAX750
	if (vax_cputype == VAX_750)
		sc->sc_physnr += 4;	/* ...but not on 11/750 */
#endif
	sc->sc_first = 0;
	sc->sc_last = (void *)&sc->sc_first;
	sc->sc_mbareg = (struct mba_regs *)mbar;
	mbar->mba_cr = MBACR_INIT;	/* Reset adapter */
	mbar->mba_cr = MBACR_IE;	/* Enable interrupts */

	for (i = 0; i < MAXMBADEV; i++) {
		sc->sc_state = SC_AUTOCONF;
		if ((mbar->mba_md[i].md_ds & MBADS_DPR) == 0) 
			continue;
		/* We have a drive, ok. */
		ma.unit = i;
		ma.type = mbar->mba_md[i].md_dt & 0777;
		j = 0;
		while (mbaunit[j++].nr)
			if (mbaunit[j].nr == ma.type)
				break;
		ma.devtyp = mbaunit[j].devtyp;
		ma.name = mbaunit[j].name;
		config_found(&sc->sc_dev, (void *)&ma, mbaprint);
	}
}

/*
 * We got an interrupt. Check type of interrupt and call the specific
 * device interrupt handling routine.
 */
void
mbaintr(mba)
	void	*mba;
{
	struct	mba_softc *sc = mba;
	volatile struct	mba_regs *mr = sc->sc_mbareg;
	struct	mba_device *md;
	struct	buf *bp;
	int	itype, attn, anr;

	itype = mr->mba_sr;
	mr->mba_sr = itype;	/* Write back to clear bits */

	attn = mr->mba_md[0].md_as & 0xff;
	mr->mba_md[0].md_as = attn;

	if (sc->sc_state == SC_AUTOCONF)
		return;	/* During autoconfig */

	md = sc->sc_first;
	bp = BUFQ_FIRST(&md->md_q);
	/*
	 * A data-transfer interrupt. Current operation is finished,
	 * call that device's finish routine to see what to do next.
	 */
	if (sc->sc_state == SC_ACTIVE) {

		sc->sc_state = SC_IDLE;
		switch ((*md->md_finish)(md, itype, &attn)) {

		case XFER_FINISH:
			/*
			 * Transfer is finished. Take buffer of drive
			 * queue, and take drive of adapter queue.
			 * If more to transfer, start the adapter again
			 * by calling mbastart().
			 */
			BUFQ_REMOVE(&md->md_q, bp);
			sc->sc_first = md->md_back;
			md->md_back = 0;
			if (sc->sc_first == 0)
				sc->sc_last = (void *)&sc->sc_first;

			if (BUFQ_FIRST(&md->md_q) != NULL) {
				sc->sc_last->md_back = md;
				sc->sc_last = md;
			}
	
			bp->b_resid = 0;
			biodone(bp);
			if (sc->sc_first)
				mbastart(sc);
			break;

		case XFER_RESTART:
			/*
			 * Something went wrong with the transfer. Try again.
			 */
			mbastart(sc);
			break;
		}
	}

	while (attn) {
		anr = ffs(attn) - 1;
		attn &= ~(1 << anr);
		if (sc->sc_md[anr]->md_attn == 0)
			panic("Should check for new MBA device %d", anr);
		(*sc->sc_md[anr]->md_attn)(sc->sc_md[anr]);
	}
}

int
mbaprint(aux, mbaname)
	void	*aux;
	const char	*mbaname;
{
	struct  mba_attach_args *ma = aux;

	if (mbaname) {
		if (ma->name)
			printf("%s", ma->name);
		else
			printf("device type %o", ma->type);
		printf(" at %s", mbaname);
	}
	printf(" drive %d", ma->unit);
	return (ma->name ? UNCONF : UNSUPP);
}

/*
 * A device calls mbaqueue() when it wants to get on the adapter queue.
 * Called at splbio(). If the adapter is inactive, start it. 
 */
void
mbaqueue(md)
	struct	mba_device *md;
{
	struct	mba_softc *sc = md->md_mba;
	int	i = (int)sc->sc_first;

	sc->sc_last->md_back = md;
	sc->sc_last = md;

	if (i == 0)
		mbastart(sc);
}

/*
 * Start activity on (idling) adapter. Calls mbamapregs() to setup
 * for dma transfer, then the unit-specific start routine.
 */
void
mbastart(sc)
	struct	mba_softc *sc;
{
	struct	mba_device *md = sc->sc_first;
	volatile struct	mba_regs *mr = sc->sc_mbareg;
	struct	buf *bp = BUFQ_FIRST(&md->md_q);

	disk_reallymapin(BUFQ_FIRST(&md->md_q), sc->sc_mbareg->mba_map,
	    0, PG_V);

	sc->sc_state = SC_ACTIVE;
	mr->mba_var = ((u_int)bp->b_data & VAX_PGOFSET);
	mr->mba_bc = (~bp->b_bcount) + 1;
	(*md->md_start)(md);		/* machine-dependent start */
}
