/*	$NetBSD: siop.c,v 1.25.2.1 1995/11/24 07:51:23 chopps Exp $	*/

/*
 * Copyright (c) 1994 Michael L. Hitch
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 *	@(#)siop.c	7.5 (Berkeley) 5/4/91
 */

/*
 * AMIGA 53C710 scsi adaptor driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <machine/cpu.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/siopreg.h>
#include <amiga/dev/siopvar.h>

extern u_int	kvtop();

/*
 * SCSI delays
 * In u-seconds, primarily for state changes on the SPC.
 */
#define	SCSI_CMD_WAIT	500000	/* wait per step of 'immediate' cmds */
#define	SCSI_DATA_WAIT	500000	/* wait per data in/out step */
#define	SCSI_INIT_WAIT	500000	/* wait per step (both) during init */

void siop_select __P((struct siop_softc *));
void siopabort __P((struct siop_softc *, siop_regmap_p, char *));
void sioperror __P((struct siop_softc *, siop_regmap_p, u_char));
void siopstart __P((struct siop_softc *));
void siopreset __P((struct siop_softc *));
void siopsetdelay __P((int));
void siop_scsidone __P((struct siop_acb *, int));
void siop_sched __P((struct siop_softc *));
int  siop_poll __P((struct siop_softc *, struct siop_acb *));
int  siopintr __P((struct siop_softc *));

/* 53C710 script */
const
#include <amiga/dev/siop_script.out>

/* default to not inhibit sync negotiation on any drive */
u_char siop_inhibit_sync[8] = { 0, 0, 0, 0, 0, 0, 0 }; /* initialize, so patchable */
u_char siop_allow_disc[8] = {3, 3, 3, 3, 3, 3, 3, 3};
int siop_no_dma = 0;

int siop_reset_delay = 250;	/* delay after reset, in milleseconds */

int siop_cmd_wait = SCSI_CMD_WAIT;
int siop_data_wait = SCSI_DATA_WAIT;
int siop_init_wait = SCSI_INIT_WAIT;

#ifdef DEBUG
/*
 * sync period transfer lookup - only valid for 66Mhz clock
 */
static struct {
	unsigned char p;	/* period from sync request message */
	unsigned char r;	/* siop_period << 4 | sbcl */
} sync_tab[] = {
	{ 60/4, 0<<4 | 1},
	{ 76/4, 1<<4 | 1},
	{ 92/4, 2<<4 | 1},
	{ 92/4, 0<<4 | 2},
	{108/4, 3<<4 | 1},
	{116/4, 1<<4 | 2},
	{120/4, 4<<4 | 1},
	{120/4, 0<<4 | 3},
	{136/4, 5<<4 | 1},
	{140/4, 2<<4 | 2},
	{152/4, 6<<4 | 1},
	{152/4, 1<<4 | 3},
	{164/4, 3<<4 | 2},
	{168/4, 7<<4 | 1},
	{180/4, 2<<4 | 3},
	{184/4, 4<<4 | 2},
	{208/4, 5<<4 | 2},
	{212/4, 3<<4 | 3},
	{232/4, 6<<4 | 2},
	{240/4, 4<<4 | 3},
	{256/4, 7<<4 | 2},
	{272/4, 5<<4 | 3},
	{300/4, 6<<4 | 3},
	{332/4, 7<<4 | 3}
};
#endif

#ifdef DEBUG
/*
 *	0x01 - full debug
 *	0x02 - DMA chaining
 *	0x04 - siopintr
 *	0x08 - phase mismatch
 *	0x10 - <not used>
 *	0x20 - panic on unhandled exceptions
 *	0x100 - disconnect/reselect
 */
int	siop_debug = 0;
int	siopsync_debug = 0;
int	siopdma_hits = 0;
int	siopdma_misses = 0;
int	siopchain_ints = 0;
int	siopstarts = 0;
int	siopints = 0;
int	siopphmm = 0;
#define SIOP_TRACE_SIZE	128
#define SIOP_TRACE(a,b,c,d) \
	siop_trbuf[siop_trix] = (a); \
	siop_trbuf[siop_trix+1] = (b); \
	siop_trbuf[siop_trix+2] = (c); \
	siop_trbuf[siop_trix+3] = (d); \
	siop_trix = (siop_trix + 4) & (SIOP_TRACE_SIZE - 1);
u_char	siop_trbuf[SIOP_TRACE_SIZE];
int	siop_trix;
void siop_dump __P((struct siop_softc *));
void siop_dump_trace __P((void));
#else
#define SIOP_TRACE(a,b,c,d)
#endif


/*
 * default minphys routine for siop based controllers
 */
void
siop_minphys(bp)
	struct buf *bp;
{

	/*
	 * No max transfer at this level.
	 */
	minphys(bp);
}

/*
 * used by specific siop controller
 *
 */
int
siop_scsicmd(xs)
	struct scsi_xfer *xs;
{
	struct siop_acb *acb;
	struct siop_softc *sc;
	struct scsi_link *slp;
	int flags, s, i;

	slp = xs->sc_link;
	sc = slp->adapter_softc;
	flags = xs->flags;

	/* XXXX ?? */
	if (flags & SCSI_DATA_UIO)
		panic("siop: scsi data uio requested");

	/* XXXX ?? */
	if (sc->sc_nexus && flags & SCSI_POLL)
/*		panic("siop_scsicmd: busy");*/
		printf("siop_scsicmd: busy\n");

	s = splbio();
	acb = sc->free_list.tqh_first;
	if (acb) {
		TAILQ_REMOVE(&sc->free_list, acb, chain);
	}
	splx(s);

	if (acb == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return(TRY_AGAIN_LATER);
	}

	acb->flags = ACB_ACTIVE;
	acb->xs = xs;
	bcopy(xs->cmd, &acb->cmd, xs->cmdlen);
	acb->clen = xs->cmdlen;
	acb->daddr = xs->data;
	acb->dleft = xs->datalen;

	s = splbio();
	TAILQ_INSERT_TAIL(&sc->ready_list, acb, chain);

	if (sc->sc_nexus == NULL)
		siop_sched(sc);

	splx(s);

	if (flags & SCSI_POLL || siop_no_dma)
		return(siop_poll(sc, acb));
	return(SUCCESSFULLY_QUEUED);
}

int
siop_poll(sc, acb)
	struct siop_softc *sc;
	struct siop_acb *acb;
{
	siop_regmap_p rp = sc->sc_siopp;
	struct scsi_xfer *xs = acb->xs;
	int i;
	int status;
	u_char istat;
	u_char dstat;
	u_char sstat0;
	int s;
	int to;

	s = splbio();
	to = xs->timeout / 1000;
	if (sc->nexus_list.tqh_first)
		printf("%s: siop_poll called with disconnected device\n",
		    sc->sc_dev.dv_xname);
	for (;;) {
		/* use cmd_wait values? */
		i = 50000;
		spl0();
		while (((istat = rp->siop_istat) &
		    (SIOP_ISTAT_SIP | SIOP_ISTAT_DIP)) == 0) {
			if (--i <= 0) {
#ifdef DEBUG
				printf ("waiting: tgt %d cmd %02x sbcl %02x dsp %x (+%x) dcmd %x ds %x timeout %d\n",
				    xs->sc_link->target, acb->cmd.opcode,
				    rp->siop_sbcl, rp->siop_dsp,
				    rp->siop_dsp - sc->sc_scriptspa,
				    *((long *)&rp->siop_dcmd), &acb->ds, acb->xs->timeout);
#endif
				i = 50000;
				--to;
				if (to <= 0) {
					siopreset(sc);
					return(COMPLETE);
				}
			}
			delay(10);
		}
		sstat0 = rp->siop_sstat0;
		dstat = rp->siop_dstat;
		if (siop_checkintr(sc, istat, dstat, sstat0, &status)) {
			if (acb != sc->sc_nexus)
				printf("%s: siop_poll disconnected device completed\n",
				    sc->sc_dev.dv_xname);
			else if ((sc->sc_flags & SIOP_INTDEFER) == 0) {
				sc->sc_flags &= ~SIOP_INTSOFF;
				rp->siop_sien = sc->sc_sien;
				rp->siop_dien = sc->sc_dien;
			}
			siop_scsidone(sc->sc_nexus, status);
		}
		if (xs->flags & ITSDONE)
			break;
	}
	splx(s);
	return (COMPLETE);
}

/*
 * start next command that's ready
 */
void
siop_sched(sc)
	struct siop_softc *sc;
{
	struct scsi_link *slp;
	struct siop_acb *acb;
	int stat, i;

#ifdef DEBUG
	if (sc->sc_nexus) {
		printf("%s: siop_sched- nexus %x/%d ready %x/%d\n",
		    sc->sc_dev.dv_xname, sc->sc_nexus,
		    sc->sc_nexus->xs->sc_link->target,
		    sc->ready_list.tqh_first,
		    sc->ready_list.tqh_first->xs->sc_link->target);
		return;
	}
#endif
	for (acb = sc->ready_list.tqh_first; acb; acb = acb->chain.tqe_next) {
		slp = acb->xs->sc_link;
		i = slp->target;
		if(!(sc->sc_tinfo[i].lubusy & (1 << slp->lun))) {
			struct siop_tinfo *ti = &sc->sc_tinfo[i];

			TAILQ_REMOVE(&sc->ready_list, acb, chain);
			sc->sc_nexus = acb;
			slp = acb->xs->sc_link;
			ti = &sc->sc_tinfo[slp->target];
			ti->lubusy |= (1 << slp->lun);
			break;
		}
	}

	if (acb == NULL) {
#ifdef DEBUGXXX
		printf("%s: siop_sched didn't find ready command\n",
		    sc->sc_dev.dv_xname);
#endif
		return;
	}

	if (acb->xs->flags & SCSI_RESET)
		siopreset(sc);

#if 0
	acb->cmd.bytes[0] |= slp->lun << 5;	/* XXXX */
#endif
	++sc->sc_active;
	siop_select(sc);
}

void
siop_scsidone(acb, stat)
	struct siop_acb *acb;
	int stat;
{
	struct scsi_xfer *xs = acb->xs;
	struct scsi_link *slp = xs->sc_link;
	struct siop_softc *sc = slp->adapter_softc;
	int s, dosched = 0;

#ifdef DIAGNOSTIC
	if (acb == NULL || xs == NULL)
		panic("siop_scsidone");
#endif
	if (slp->device_softc && 
	    ((struct device *)(slp->device_softc))->dv_unit < dk_ndrive)
		++dk_xfer[((struct device *)(slp->device_softc))->dv_unit];
	/*
	 * is this right?
	 */
	xs->status = stat;

	if (xs->error == XS_NOERROR && !(acb->flags & ACB_CHKSENSE)) {
		if (stat == SCSI_CHECK) {
			struct scsi_sense *ss = (void *)&acb->cmd;
			bzero(ss, sizeof(*ss));
			ss->opcode = REQUEST_SENSE;
			ss->byte2 = slp->lun << 5;
			ss->length = sizeof(struct scsi_sense_data);
			acb->clen = sizeof(*ss);
			acb->daddr = (char *)&xs->sense;
			acb->dleft = sizeof(struct scsi_sense_data);
			acb->flags = ACB_ACTIVE | ACB_CHKSENSE;
			TAILQ_INSERT_HEAD(&sc->ready_list, acb, chain);
			--sc->sc_active;
			sc->sc_tinfo[slp->target].lubusy &=
			    ~(1 << slp->lun);
			sc->sc_tinfo[slp->target].senses++;
			if (sc->sc_nexus == acb) {
				sc->sc_nexus = NULL;
				siop_sched(sc);
			}
			SIOP_TRACE('d','s',0,0)
			return;
		}
	}
	if (xs->error == XS_NOERROR && (acb->flags & ACB_CHKSENSE)) {
		xs->error = XS_SENSE;
	} else {
		xs->resid = 0;		/* XXXX */
	}
#if whataboutthisone
		case SCSI_BUSY:
			xs->error = XS_BUSY;
			break;
#endif
	xs->flags |= ITSDONE;

	/*
	 * Remove the ACB from whatever queue it's on.  We have to do a bit of
	 * a hack to figure out which queue it's on.  Note that it is *not*
	 * necessary to cdr down the ready queue, but we must cdr down the
	 * nexus queue and see if it's there, so we can mark the unit as no
	 * longer busy.  This code is sickening, but it works.
	 */
	if (acb == sc->sc_nexus) {
		sc->sc_nexus = NULL;
		sc->sc_tinfo[slp->target].lubusy &= ~(1<<slp->lun);
		if (sc->ready_list.tqh_first)
			dosched = 1;	/* start next command */
		--sc->sc_active;
		SIOP_TRACE('d','a',stat,0)
	} else if (sc->ready_list.tqh_last == &acb->chain.tqe_next) {
		TAILQ_REMOVE(&sc->ready_list, acb, chain);
		SIOP_TRACE('d','r',stat,0)
	} else {
		register struct siop_acb *acb2;
		for (acb2 = sc->nexus_list.tqh_first; acb2;
		    acb2 = acb2->chain.tqe_next)
			if (acb2 == acb) {
				TAILQ_REMOVE(&sc->nexus_list, acb, chain);
				sc->sc_tinfo[slp->target].lubusy
					&= ~(1<<slp->lun);
				--sc->sc_active;
				break;
			}
		if (acb2)
			;
		else if (acb->chain.tqe_next) {
			TAILQ_REMOVE(&sc->ready_list, acb, chain);
			--sc->sc_active;
		} else {
			printf("%s: can't find matching acb\n",
			    sc->sc_dev.dv_xname);
#ifdef DDB
/*			Debugger(); */
#endif
		}
		SIOP_TRACE('d','n',stat,0);
	}
	/* Put it on the free list. */
	acb->flags = ACB_FREE;
	TAILQ_INSERT_HEAD(&sc->free_list, acb, chain);

	sc->sc_tinfo[slp->target].cmds++;

	scsi_done(xs);

	if (dosched && sc->sc_nexus == NULL)
		siop_sched(sc);
}

void
siopabort(sc, rp, where)
	register struct siop_softc *sc;
	siop_regmap_p rp;
	char *where;
{
	int i;

	printf ("%s: abort %s: dstat %02x, sstat0 %02x sbcl %02x\n",
	    sc->sc_dev.dv_xname,
	    where, rp->siop_dstat, rp->siop_sstat0, rp->siop_sbcl);

	if (sc->sc_active > 0) {
#ifdef TODO
      SET_SBIC_cmd (rp, SBIC_CMD_ABORT);
      WAIT_CIP (rp);

      GET_SBIC_asr (rp, asr);
      if (asr & (SBIC_ASR_BSY|SBIC_ASR_LCI))
        {
          /* ok, get more drastic.. */

	  SET_SBIC_cmd (rp, SBIC_CMD_RESET);
	  delay(25);
	  SBIC_WAIT(rp, SBIC_ASR_INT, 0);
	  GET_SBIC_csr (rp, csr);       /* clears interrupt also */

          return;
        }

      do
        {
          SBIC_WAIT (rp, SBIC_ASR_INT, 0);
          GET_SBIC_csr (rp, csr);
        }
      while ((csr != SBIC_CSR_DISC) && (csr != SBIC_CSR_DISC_1)
	      && (csr != SBIC_CSR_CMD_INVALID));
#endif

		/* lets just hope it worked.. */
#ifdef fix_this
		for (i = 0; i < 2; ++i) {
			if (sc->sc_iob[i].sc_xs && &sc->sc_iob[i] !=
			    sc->sc_cur) {
				printf ("siopabort: cleanup!\n");
				sc->sc_iob[i].sc_xs = NULL;
			}
		}
#endif	/* fix_this */
/*		sc->sc_active = 0; */
	}
}

void
siopinitialize(sc)
	struct siop_softc *sc;
{
	int i;
	u_int inhibit_sync;
	extern u_long scsi_nosync;
	extern int shift_nosync;

	/*
	 * Need to check that scripts is on a long word boundary
	 * and that DS is on a long word boundary.
	 * Also should verify that dev doesn't span non-contiguous
	 * physical pages.
	 */
	sc->sc_scriptspa = kvtop(scripts);
	sc->sc_tcp[1] = 1000 / sc->sc_clock_freq;
	sc->sc_tcp[2] = 1500 / sc->sc_clock_freq;
	sc->sc_tcp[3] = 2000 / sc->sc_clock_freq;
	sc->sc_minsync = sc->sc_tcp[1];		/* in 4ns units */
	if (sc->sc_minsync < 25)
		sc->sc_minsync = 25;
	if (sc->sc_clock_freq <= 25) {
		sc->sc_dcntl = 0x80;		/* SCLK/1 */
		sc->sc_tcp[0] = sc->sc_tcp[1];
	} else if (sc->sc_clock_freq <= 37) {
		sc->sc_dcntl = 0x40;		/* SCLK/1.5 */
		sc->sc_tcp[0] = sc->sc_tcp[2];
	} else if (sc->sc_clock_freq <= 50) {
		sc->sc_dcntl = 0x00;		/* SCLK/2 */
		sc->sc_tcp[0] = sc->sc_tcp[3];
	} else {
		sc->sc_dcntl = 0xc0;		/* SCLK/3 */
		sc->sc_tcp[0] = 3000 / sc->sc_clock_freq;
	}

	if (scsi_nosync) {
		inhibit_sync = (scsi_nosync >> shift_nosync) & 0xff;
		shift_nosync += 8;
#ifdef DEBUG
		if (inhibit_sync)
			printf("%s: Inhibiting synchronous transfer %02x\n",
				sc->sc_dev.dv_xname, inhibit_sync);
#endif
		for (i = 0; i < 8; ++i)
			if (inhibit_sync & (1 << i))
				siop_inhibit_sync[i] = 1;
	}

	siopreset (sc);
}

void
siopreset(sc)
	struct siop_softc *sc;
{
	siop_regmap_p rp;
	u_int i, s;
	u_char  dummy;
	struct siop_acb *acb;

	rp = sc->sc_siopp;

	if (sc->sc_flags & SIOP_ALIVE)
		siopabort(sc, rp, "reset");

	printf("%s: ", sc->sc_dev.dv_xname);		/* XXXX */

	s = splbio();

	/*
	 * Reset the chip
	 * XXX - is this really needed?
	 */
	rp->siop_istat |= SIOP_ISTAT_ABRT;	/* abort current script */
	rp->siop_istat |= SIOP_ISTAT_RST;		/* reset chip */
	rp->siop_istat &= ~SIOP_ISTAT_RST;
	/*
	 * Reset SCSI bus (do we really want this?)
	 */
	rp->siop_sien = 0;
	rp->siop_scntl1 |= SIOP_SCNTL1_RST;
	delay(1);
	rp->siop_scntl1 &= ~SIOP_SCNTL1_RST;

	/*
	 * Set up various chip parameters
	 */
	rp->siop_scntl0 = SIOP_ARB_FULL | SIOP_SCNTL0_EPC | SIOP_SCNTL0_EPG;
	rp->siop_scntl1 = SIOP_SCNTL1_ESR;
	rp->siop_dcntl = sc->sc_dcntl;
	rp->siop_dmode = 0x80;	/* burst length = 4 */
	rp->siop_sien = 0x00;	/* don't enable interrupts yet */
	rp->siop_dien = 0x00;	/* don't enable interrupts yet */
	rp->siop_scid = 1 << sc->sc_link.adapter_target;
	rp->siop_dwt = 0x00;
	rp->siop_ctest0 |= SIOP_CTEST0_BTD | SIOP_CTEST0_EAN;
	rp->siop_ctest7 |= sc->sc_ctest7;

	/* will need to re-negotiate sync xfers */
	bzero(&sc->sc_sync, sizeof (sc->sc_sync));

	i = rp->siop_istat;
	if (i & SIOP_ISTAT_SIP)
		dummy = rp->siop_sstat0;
	if (i & SIOP_ISTAT_DIP)
		dummy = rp->siop_dstat;

	splx (s);

	delay (siop_reset_delay * 1000);
	printf("siop id %d reset V%d\n", sc->sc_link.adapter_target,
	    rp->siop_ctest8 >> 4);

	if ((sc->sc_flags & SIOP_ALIVE) == 0) {
		TAILQ_INIT(&sc->ready_list);
		TAILQ_INIT(&sc->nexus_list);
		TAILQ_INIT(&sc->free_list);
		sc->sc_nexus = NULL;
		acb = sc->sc_acb;
		bzero(acb, sizeof(sc->sc_acb));
		for (i = 0; i < sizeof(sc->sc_acb) / sizeof(*acb); i++) {
			TAILQ_INSERT_TAIL(&sc->free_list, acb, chain);
			acb++;
		}
		bzero(sc->sc_tinfo, sizeof(sc->sc_tinfo));
	} else {
		if (sc->sc_nexus != NULL) {
			sc->sc_nexus->xs->error = XS_DRIVER_STUFFUP;
			siop_scsidone(sc->sc_nexus, sc->sc_nexus->stat[0]);
		}
		while (acb = sc->nexus_list.tqh_first) {
			acb->xs->error = XS_DRIVER_STUFFUP;
			siop_scsidone(acb, acb->stat[0]);
		}
	}

	sc->sc_flags |= SIOP_ALIVE;
	sc->sc_flags &= ~(SIOP_INTDEFER|SIOP_INTSOFF);
	/* enable SCSI and DMA interrupts */
	sc->sc_sien = SIOP_SIEN_M_A | SIOP_SIEN_STO | /*SIOP_SIEN_SEL |*/ SIOP_SIEN_SGE |
	    SIOP_SIEN_UDC | SIOP_SIEN_RST | SIOP_SIEN_PAR;
	sc->sc_dien = SIOP_DIEN_BF | SIOP_DIEN_ABRT | SIOP_DIEN_SIR |
	    /*SIOP_DIEN_WTD |*/ SIOP_DIEN_IID;
	rp->siop_sien = sc->sc_sien;
	rp->siop_dien = sc->sc_dien;
}

/*
 * Setup Data Storage for 53C710 and start SCRIPTS processing
 */

void
siop_start (sc, target, lun, cbuf, clen, buf, len)
	struct siop_softc *sc;
	int target;
	int lun;
	u_char *cbuf;
	int clen;
	u_char *buf;
	int len;
{
	siop_regmap_p rp = sc->sc_siopp;
	int i;
	int nchain;
	int count, tcount;
	char *addr, *dmaend;
	struct siop_acb *acb = sc->sc_nexus;

#ifdef DEBUG
	if (siop_debug & 0x100 && rp->siop_sbcl & SIOP_BSY) {
		printf ("ACK! siop was busy: rp %x script %x dsa %x active %d\n",
		    rp, &scripts, &acb->ds, sc->sc_active);
		printf ("istat %02x sfbr %02x lcrc %02x sien %02x dien %02x\n",
		    rp->siop_istat, rp->siop_sfbr, rp->siop_lcrc,
		    rp->siop_sien, rp->siop_dien);
#ifdef DDB
		/*Debugger();*/
#endif
	}
#endif
	acb->msgout[0] = MSG_IDENTIFY | lun;
	if (siop_allow_disc[target] & 2 ||
	    (siop_allow_disc[target] && len == 0))
		acb->msgout[0] = MSG_IDENTIFY_DR | lun;
	acb->status = 0;
	acb->stat[0] = -1;
	acb->msg[0] = -1;
	acb->ds.scsi_addr = (0x10000 << target) | (sc->sc_sync[target].sxfer << 8);
	acb->ds.idlen = 1;
	acb->ds.idbuf = (char *) kvtop(&acb->msgout[0]);
	acb->ds.cmdlen = clen;
	acb->ds.cmdbuf = (char *) kvtop(cbuf);
	acb->ds.stslen = 1;
	acb->ds.stsbuf = (char *) kvtop(&acb->stat[0]);
	acb->ds.msglen = 1;
	acb->ds.msgbuf = (char *) kvtop(&acb->msg[0]);
	acb->msg[1] = -1;
	acb->ds.msginlen = 1;
	acb->ds.extmsglen = 1;
	acb->ds.synmsglen = 3;
	acb->ds.msginbuf = (char *) kvtop(&acb->msg[1]);
	acb->ds.extmsgbuf = (char *) kvtop(&acb->msg[2]);
	acb->ds.synmsgbuf = (char *) kvtop(&acb->msg[3]);
	bzero(&acb->ds.chain, sizeof (acb->ds.chain));

	if (sc->sc_sync[target].state == SYNC_START) {
		if (siop_inhibit_sync[target]) {
			sc->sc_sync[target].state = SYNC_DONE;
			sc->sc_sync[target].sbcl = 0;
			sc->sc_sync[target].sxfer = 0;
#ifdef DEBUG
			if (siopsync_debug)
				printf ("Forcing target %d asynchronous\n", target);
#endif
		}
		else {
			acb->msg[2] = -1;
			acb->msgout[1] = MSG_EXT_MESSAGE;
			acb->msgout[2] = 3;
			acb->msgout[3] = MSG_SYNC_REQ;
#ifdef MAXTOR_SYNC_KLUDGE
			acb->msgout[4] = 50 / 4;	/* ask for ridiculous period */
#else
			acb->msgout[4] = sc->sc_minsync;
#endif
			acb->msgout[5] = SIOP_MAX_OFFSET;
			acb->ds.idlen = 6;
			sc->sc_sync[target].state = SYNC_SENT;
#ifdef DEBUG
			if (siopsync_debug)
				printf ("Sending sync request to target %d\n", target);
#endif
		}
	}

/*
 * Build physical DMA addresses for scatter/gather I/O
 */
	acb->iob_buf = buf;
	acb->iob_len = len;
	acb->iob_curbuf = acb->iob_curlen = 0;
	nchain = 0;
	count = len;
	addr = buf;
	dmaend = NULL;
	while (count > 0) {
		acb->ds.chain[nchain].databuf = (char *) kvtop (addr);
		if (count < (tcount = NBPG - ((int) addr & PGOFSET)))
			tcount = count;
		acb->ds.chain[nchain].datalen = tcount;
		addr += tcount;
		count -= tcount;
		if (acb->ds.chain[nchain].databuf == dmaend) {
			dmaend += acb->ds.chain[nchain].datalen;
			acb->ds.chain[nchain].datalen = 0;
			acb->ds.chain[--nchain].datalen += tcount;
#ifdef DEBUG
			++siopdma_hits;
#endif
		}
		else {
			dmaend = acb->ds.chain[nchain].databuf +
			    acb->ds.chain[nchain].datalen;
			acb->ds.chain[nchain].datalen = tcount;
#ifdef DEBUG
			if (nchain)	/* Don't count miss on first one */
				++siopdma_misses;
#endif
		}
		++nchain;
	}
#ifdef DEBUG
	if (nchain != 1 && len != 0 && siop_debug & 3) {
		printf ("DMA chaining set: %d\n", nchain);
		for (i = 0; i < nchain; ++i) {
			printf ("  [%d] %8x %4x\n", i, acb->ds.chain[i].databuf,
			    acb->ds.chain[i].datalen);
		}
	}
#endif

	/* push data cache for all data the 53c710 needs to access */
	dma_cachectl (sc, sizeof (struct siop_softc));
	dma_cachectl (cbuf, clen);
	if (buf != NULL && len != 0)
		dma_cachectl (buf, len);
#ifdef DEBUG
	if (siop_debug & 0x100 && rp->siop_sbcl & SIOP_BSY) {
		printf ("ACK! siop was busy at start: rp %x script %x dsa %x active %d\n",
		    rp, &scripts, &acb->ds, sc->sc_active);
#ifdef DDB
		/*Debugger();*/
#endif
	}
#endif
	if (sc->nexus_list.tqh_first == NULL) {
		if (rp->siop_istat & SIOP_ISTAT_CON)
			printf("%s: siop_select while connected?\n",
			    sc->sc_dev.dv_xname);
		rp->siop_temp = 0;
		rp->siop_sbcl = sc->sc_sync[target].sbcl;
		rp->siop_dsa = kvtop(&acb->ds);
		rp->siop_dsp = sc->sc_scriptspa;
		SIOP_TRACE('s',1,0,0)
	} else {
		if ((rp->siop_istat & SIOP_ISTAT_CON) == 0) {
			rp->siop_istat = SIOP_ISTAT_SIGP;
			SIOP_TRACE('s',2,0,0);
		}
		else {
			SIOP_TRACE('s',3,rp->siop_istat,0);
		}
	}
#ifdef DEBUG
	++siopstarts;
#endif
}

/*
 * Process a DMA or SCSI interrupt from the 53C710 SIOP
 */

int
siop_checkintr(sc, istat, dstat, sstat0, status)
	struct	siop_softc *sc;
	u_char	istat;
	u_char	dstat;
	u_char	sstat0;
	int	*status;
{
	siop_regmap_p rp = sc->sc_siopp;
	struct siop_acb *acb = sc->sc_nexus;
	int	target;
	int	dfifo, dbc, sstat1;

	dfifo = rp->siop_dfifo;
	dbc = rp->siop_dbc0;
	sstat1 = rp->siop_sstat1;
	rp->siop_ctest8 |= SIOP_CTEST8_CLF;
	while ((rp->siop_ctest1 & SIOP_CTEST1_FMT) != SIOP_CTEST1_FMT)
		;
	rp->siop_ctest8 &= ~SIOP_CTEST8_CLF;
#ifdef DEBUG
	++siopints;
#if 0
	if (siop_debug & 0x100) {
		DCIAS(&acb->stat[0]);	/* XXX */
		printf ("siopchkintr: istat %x dstat %x sstat0 %x dsps %x sbcl %x sts %x msg %x\n",
		    istat, dstat, sstat0, rp->siop_dsps, rp->siop_sbcl, acb->stat[0], acb->msg[0]);
		printf ("sync msg in: %02x %02x %02x %02x %02x %02x\n",
		    acb->msg[0], acb->msg[1], acb->msg[2],
		    acb->msg[3], acb->msg[4], acb->msg[5]);
	}
#endif
	if (rp->siop_dsp && (rp->siop_dsp < sc->sc_scriptspa ||
	    rp->siop_dsp >= sc->sc_scriptspa + sizeof(scripts))) {
		printf ("%s: dsp not within script dsp %x scripts %x:%x",
		    sc->sc_dev.dv_xname, rp->siop_dsp, sc->sc_scriptspa,
		    sc->sc_scriptspa + sizeof(scripts));
		printf(" istat %x dstat %x sstat0 %x\n",
		    istat, dstat, sstat0);
		Debugger();
	}
#endif
	SIOP_TRACE('i',dstat,istat,(istat&SIOP_ISTAT_DIP)?rp->siop_dsps&0xff:sstat0);
	if (dstat & SIOP_DSTAT_SIR && rp->siop_dsps == 0xff00) {
		/* Normal completion status, or check condition */
#ifdef DEBUG
		if (rp->siop_dsa != kvtop(&acb->ds)) {
			printf ("siop: invalid dsa: %x %x\n", rp->siop_dsa,
			    kvtop(&acb->ds));
			panic("*** siop DSA invalid ***");
		}
#endif
		target = acb->xs->sc_link->target;
		if (sc->sc_sync[target].state == SYNC_SENT) {
#ifdef DEBUG
			if (siopsync_debug)
				printf ("sync msg in: %02x %02x %02x %02x %02x %02x\n",
				    acb->msg[0], acb->msg[1], acb->msg[2],
				    acb->msg[3], acb->msg[4], acb->msg[5]);
#endif
			if (acb->msg[1] == 0xff)
				printf ("%s: target %d ignored sync request\n",
				    sc->sc_dev.dv_xname, target);
			else if (acb->msg[1] == MSG_REJECT)
				printf ("%s: target %d rejected sync request\n",
				    sc->sc_dev.dv_xname, target);
			sc->sc_sync[target].state = SYNC_DONE;
			sc->sc_sync[target].sxfer = 0;
			sc->sc_sync[target].sbcl = 0;
			if (acb->msg[2] == 3 &&
			    acb->msg[3] == MSG_SYNC_REQ &&
			    acb->msg[5] != 0) {
#ifdef MAXTOR_KLUDGE
				/*
				 * Kludge for my Maxtor XT8580S
				 * It accepts whatever we request, even
				 * though it won't work.  So we ask for
				 * a short period than we can handle.  If
				 * the device says it can do it, use 208ns.
				 * If the device says it can do less than
				 * 100ns, then we limit it to 100ns.
				 */
				if (acb->msg[4] && acb->msg[4] < 100 / 4) {
#ifdef DEBUG
					printf ("%d: target %d wanted %dns period\n",
					    sc->sc_dev.dv_xname, target,
					    acb->msg[4] * 4);
#endif
					if (acb->msg[4] == 50 / 4)
						acb->msg[4] = 208 / 4;
					else
						acb->msg[4] = 100 / 4;
				}
#endif /* MAXTOR_KLUDGE */
				printf ("%s: target %d now synchronous, period=%dns, offset=%d\n",
				    sc->sc_dev.dv_xname, target,
				    acb->msg[4] * 4, acb->msg[5]);
				scsi_period_to_siop (sc, target);
			}
		}
		dma_cachectl(&acb->stat[0], 1);
		*status = acb->stat[0];
#ifdef DEBUG
		if (rp->siop_sbcl & SIOP_BSY) {
			/*printf ("ACK! siop was busy at end: rp %x script %x dsa %x\n",
			    rp, &scripts, &acb->ds);*/
#ifdef DDB
			/*Debugger();*/
#endif
		}
		if (acb->msg[0] != 0x00)
			printf("%s: message was not COMMAND COMPLETE: %x\n",
			    sc->sc_dev.dv_xname, acb->msg[0]);
#endif
		if (sc->nexus_list.tqh_first)
			rp->siop_dcntl |= SIOP_DCNTL_STD;
		return 1;
	}
	if (sstat0 & SIOP_SSTAT0_M_A) {		/* Phase mismatch */
#ifdef DEBUG
		++siopphmm;
		if (acb == NULL)
			printf("%s: Phase mismatch with no active command?\n",
			    sc->sc_dev.dv_xname);
#endif
		if (acb->iob_len) {
			int adjust;
			adjust = ((dfifo - (dbc & 0x7f)) & 0x7f);
			if (sstat1 & SIOP_SSTAT1_ORF)
				++adjust;
			if (sstat1 & SIOP_SSTAT1_OLF)
				++adjust;
			acb->iob_curlen = *((long *)&rp->siop_dcmd) & 0xffffff;
			acb->iob_curlen += adjust;
			acb->iob_curbuf = *((long *)&rp->siop_dnad) - adjust;
#ifdef DEBUG
			if (siop_debug & 0x100) {
				int i;
				printf ("Phase mismatch: curbuf %x curlen %x dfifo %x dbc %x sstat1 %x adjust %x sbcl %x starts %d acb %x\n",
				    acb->iob_curbuf, acb->iob_curlen, dfifo,
				    dbc, sstat1, adjust, rp->siop_sbcl, siopstarts, acb);
				if (acb->ds.chain[1].datalen) {
					for (i = 0; acb->ds.chain[i].datalen; ++i)
						printf("chain[%d] addr %x len %x\n",
						    i, acb->ds.chain[i].databuf,
						    acb->ds.chain[i].datalen);
				}
			}
#endif
			dma_cachectl (acb, sizeof(*acb));
		}
#ifdef DEBUG
		SIOP_TRACE('m',rp->siop_sbcl,(rp->siop_dsp>>8),rp->siop_dsp);
		if (siop_debug & 9)
			printf ("Phase mismatch: %x dsp +%x dcmd %x\n",
			    rp->siop_sbcl,
			    rp->siop_dsp - sc->sc_scriptspa,
			    *((long *)&rp->siop_dcmd));
#endif
		if ((rp->siop_sbcl & SIOP_REQ) == 0) {
			printf ("Phase mismatch: REQ not asserted! %02x dsp %x\n",
			    rp->siop_sbcl, rp->siop_dsp);
#ifdef DEBUG
			Debugger();
#endif
		}
		switch (rp->siop_sbcl & 7) {
		case 0:		/* data out */
		case 1:		/* data in */
		case 2:		/* status */
		case 3:		/* command */
		case 6:		/* message in */
		case 7:		/* message out */
			rp->siop_dsp = sc->sc_scriptspa + Ent_switch;
			break;
		default:
			goto bad_phase;
		}
		return 0;
	}
	if (sstat0 & SIOP_SSTAT0_STO) {		/* Select timed out */
#ifdef DEBUG
		if (acb == NULL)
			printf("%s: Select timeout with no active command?\n",
			    sc->sc_dev.dv_xname);
		if (rp->siop_sbcl & SIOP_BSY) {
			printf ("ACK! siop was busy at timeout: rp %x script %x dsa %x\n",
			    rp, &scripts, &acb->ds);
			printf(" sbcl %x sdid %x istat %x dstat %x sstat0 %x\n",
			    rp->siop_sbcl, rp->siop_sdid, istat, dstat, sstat0);
			if (!(rp->siop_sbcl & SIOP_BSY)) {
				printf ("Yikes, it's not busy now!\n");
#if 0
				*status = -1;
				if (sc->nexus_list.tqh_first)
					rp->siop_dsp = sc->sc_scriptspa + Ent_wait_reselect;
				return 1;
#endif
			}
/*			rp->siop_dcntl |= SIOP_DCNTL_STD;*/
			return (0);
#ifdef DDB
			Debugger();
#endif
		}
#endif
		*status = -1;
		acb->xs->error = XS_SELTIMEOUT;
		if (sc->nexus_list.tqh_first)
			rp->siop_dsp = sc->sc_scriptspa + Ent_wait_reselect;
		return 1;
	}
	if (acb)
		target = acb->xs->sc_link->target;
	else
		target = 7;
	if (sstat0 & SIOP_SSTAT0_UDC) {
#ifdef DEBUG
		if (acb == NULL)
			printf("%s: Unexpected disconnect with no active command?\n",
			    sc->sc_dev.dv_xname);
		printf ("%s: target %d disconnected unexpectedly\n",
		   sc->sc_dev.dv_xname, target);
#endif
#if 0
		siopabort (sc, rp, "siopchkintr");
#endif
		*status = STS_BUSY;
		if (sc->nexus_list.tqh_first)
			rp->siop_dsp = sc->sc_scriptspa + Ent_wait_reselect;
		return 1;
	}
	if (dstat & SIOP_DSTAT_SIR && (rp->siop_dsps == 0xff01 ||
	    rp->siop_dsps == 0xff02)) {
#ifdef DEBUG
		if (siop_debug & 0x100)
			printf ("%s: ID %02x disconnected TEMP %x (+%x) curbuf %x curlen %x buf %x len %x dfifo %x dbc %x sstat1 %x starts %d acb %x\n",
			    sc->sc_dev.dv_xname, 1 << target, rp->siop_temp,
			    rp->siop_temp ? rp->siop_temp - sc->sc_scriptspa : 0,
			    acb->iob_curbuf, acb->iob_curlen,
			    acb->ds.chain[0].databuf, acb->ds.chain[0].datalen, dfifo, dbc, sstat1, siopstarts, acb);
#endif
		if (acb == NULL) {
			printf("%s: Disconnect with no active command?\n",
			    sc->sc_dev.dv_xname);
			return (0);
		}
		/*
		 * XXXX need to update iob_curbuf/iob_curlen to reflect
		 * current data transferred.  If device disconnected in
		 * the middle of a DMA block, they should already be set
		 * by the phase change interrupt.  If the disconnect
		 * occurs on a DMA block boundary, we have to figure out
		 * which DMA block it was.
		 */
		if (acb->iob_len && rp->siop_temp) {
			int n = rp->siop_temp - sc->sc_scriptspa;

			if (acb->iob_curlen && acb->iob_curlen != acb->ds.chain[0].datalen)
				printf("%s: iob_curbuf/len already set? n %x iob %x/%x chain[0] %x/%x\n",
				    sc->sc_dev.dv_xname, n, acb->iob_curbuf, acb->iob_curlen,
				    acb->ds.chain[0].databuf, acb->ds.chain[0].datalen);
			if (n < Ent_datain)
				n = (n - Ent_dataout) / 16;
			else
				n = (n - Ent_datain) / 16;
			if (n <= 0 && n > DMAMAXIO)
				printf("TEMP invalid %d\n", n);
			else {
				acb->iob_curbuf = (u_long)acb->ds.chain[n].databuf;
				acb->iob_curlen = acb->ds.chain[n].datalen;
			}
#ifdef DEBUG
			if (siop_debug & 0x100) {
				printf("%s: TEMP offset %d", sc->sc_dev.dv_xname, n);
				printf(" curbuf %x curlen %x\n", acb->iob_curbuf,
				    acb->iob_curlen);
			}
#endif
		}
		/*
		 * If data transfer was interrupted by disconnect, iob_curbuf
		 * and iob_curlen should reflect the point of interruption.
		 * Adjust the DMA chain so that the data transfer begins
		 * at the appropriate place upon reselection.
		 * XXX This should only be done on save data pointer message?
		 */
		if (acb->iob_curlen) {
			int i, j;

#ifdef DEBUG
			if (siop_debug & 0x100)
				printf ("%s: adjusting DMA chain\n",
				    sc->sc_dev.dv_xname);
			if (rp->siop_dsps == 0xff02)
				printf ("%s: ID %02x disconnected without Save Data Pointers\n",
				    sc->sc_dev.dv_xname, 1 << target);
#endif
			for (i = 0; i < DMAMAXIO; ++i) {
				if (acb->ds.chain[i].datalen == 0)
					break;
				if (acb->iob_curbuf >= (long)acb->ds.chain[i].databuf &&
				    acb->iob_curbuf < (long)(acb->ds.chain[i].databuf +
				    acb->ds.chain[i].datalen))
					break;
			}
			if (i >= DMAMAXIO || acb->ds.chain[i].datalen == 0)
				printf("couldn't find saved data pointer\n");
#ifdef DEBUG
			if (siop_debug & 0x100)
				printf("  chain[0]: %x/%x -> %x/%x\n",
				    acb->ds.chain[0].databuf,
				    acb->ds.chain[0].datalen,
				    acb->iob_curbuf,
				    acb->iob_curlen);
#endif
			acb->ds.chain[0].databuf = (char *)acb->iob_curbuf;
			acb->ds.chain[0].datalen = acb->iob_curlen;
			for (j = 1, ++i; i < DMAMAXIO && acb->ds.chain[i].datalen; ++i, ++j) {
#ifdef DEBUG
			if (siop_debug & 0x100)
				printf("  chain[%d]: %x/%x -> %x/%x\n", j,
				    acb->ds.chain[j].databuf,
				    acb->ds.chain[j].datalen,
				    acb->ds.chain[i].databuf,
				    acb->ds.chain[i].datalen);
#endif
				acb->ds.chain[j].databuf = acb->ds.chain[i].databuf;
				acb->ds.chain[j].datalen = acb->ds.chain[i].datalen;
			}
			if (j < DMAMAXIO)
				acb->ds.chain[j].datalen = 0;
			DCIAS(kvtop(&acb->ds.chain));
		}
		++sc->sc_tinfo[target].dconns;
		/*
		 * add nexus to waiting list
		 * clear nexus
		 * try to start another command for another target/lun
		 */
		acb->status = sc->sc_flags & SIOP_INTSOFF;
		TAILQ_INSERT_HEAD(&sc->nexus_list, acb, chain);
		sc->sc_nexus = NULL;		/* no current device */
		/* start script to wait for reselect */
		if (sc->sc_nexus == NULL)
			rp->siop_dsp = sc->sc_scriptspa + Ent_wait_reselect;
/* XXXX start another command ? */
		if (sc->ready_list.tqh_first)
			siop_sched(sc);
		return (0);
	}
	if (dstat & SIOP_DSTAT_SIR && rp->siop_dsps == 0xff03) {
		int reselid = rp->siop_scratch & 0x7f;
		int reselun = rp->siop_sfbr & 0x07;

		sc->sc_sstat1 = rp->siop_sbcl;	/* XXXX save current SBCL */
#ifdef DEBUG
		if (siop_debug & 0x100)
			printf ("%s: target ID %02x reselected dsps %x\n",
			     sc->sc_dev.dv_xname, reselid,
			     rp->siop_dsps);
		if ((rp->siop_sfbr & 0x80) == 0)
			printf("%s: Reselect message in was not identify: %x\n",
			    sc->sc_dev.dv_xname, rp->siop_sfbr);
#endif
		if (sc->sc_nexus) {
#ifdef DEBUG
			if (siop_debug & 0x100)
				printf ("%s: reselect ID %02x w/active\n",
				    sc->sc_dev.dv_xname, reselid);
#endif
			TAILQ_INSERT_HEAD(&sc->ready_list, sc->sc_nexus, chain);
			sc->sc_tinfo[sc->sc_nexus->xs->sc_link->target].lubusy
			    &= ~(1 << sc->sc_nexus->xs->sc_link->lun);
			--sc->sc_active;
		}
		/*
		 * locate acb of reselecting device
		 * set sc->sc_nexus to acb
		 */
		for (acb = sc->nexus_list.tqh_first; acb;
		    acb = acb->chain.tqe_next) {
			if (reselid != (acb->ds.scsi_addr >> 16) ||
			    reselun != (acb->msgout[0] & 0x07))
				continue;
			TAILQ_REMOVE(&sc->nexus_list, acb, chain);
			sc->sc_nexus = acb;
			sc->sc_flags |= acb->status;
			acb->status = 0;
			DCIAS(kvtop(&acb->stat[0]));
			rp->siop_dsa = kvtop(&acb->ds);
			rp->siop_sxfer = sc->sc_sync[acb->xs->sc_link->target].sxfer;
			rp->siop_sbcl = sc->sc_sync[acb->xs->sc_link->target].sbcl;
			break;
		}
		if (acb == NULL) {
			printf("%s: target ID %02x reselect nexus_list %x\n",
			    sc->sc_dev.dv_xname, reselid,
			    sc->nexus_list.tqh_first);
			panic("unable to find reselecting device");
		}
		dma_cachectl (acb, sizeof(*acb));
		rp->siop_temp = 0;
		rp->siop_dcntl |= SIOP_DCNTL_STD;
		return (0);
	}
	if (dstat & SIOP_DSTAT_SIR && rp->siop_dsps == 0xff04) {
		u_short ctest2 = rp->siop_ctest2;

		/* reselect was interrupted (by Sig_P or select) */
#ifdef DEBUG
		if (siop_debug & 0x100 ||
		    (ctest2 & SIOP_CTEST2_SIGP) == 0)
			printf ("%s: reselect interrupted (Sig_P?) scntl1 %x ctest2 %x sfbr %x istat %x/%x\n",
			    sc->sc_dev.dv_xname, rp->siop_scntl1,
			    ctest2, rp->siop_sfbr, istat, rp->siop_istat);
#endif
		/* XXX assumes it was not select */
		if (sc->sc_nexus == NULL) {
			printf("%s: reselect interrupted, sc_nexus == NULL\n",
			    sc->sc_dev.dv_xname);
#if 0
			siop_dump(sc);
#ifdef DDB
			Debugger();
#endif
#endif
			rp->siop_dcntl |= SIOP_DCNTL_STD;
			return(0);
		}
		target = sc->sc_nexus->xs->sc_link->target;
		rp->siop_temp = 0;
		rp->siop_dsa = kvtop(&sc->sc_nexus->ds);
		rp->siop_sxfer = sc->sc_sync[target].sxfer;
		rp->siop_sbcl = sc->sc_sync[target].sbcl;
		rp->siop_dsp = sc->sc_scriptspa;
		return (0);
	}
	if (dstat & SIOP_DSTAT_SIR && rp->siop_dsps == 0xff06) {
		if (acb == NULL)
			printf("%s: Bad message-in with no active command?\n",
			    sc->sc_dev.dv_xname);
		/* Unrecognized message in byte */
		dma_cachectl (&acb->msg[1],1);
		printf ("%s: Unrecognized message in data sfbr %x msg %x sbcl %x\n",
			sc->sc_dev.dv_xname, rp->siop_sfbr, acb->msg[1], rp->siop_sbcl);
		/* what should be done here? */
		DCIAS(kvtop(&acb->msg[1]));
		rp->siop_dsp = sc->sc_scriptspa + Ent_switch;
		return (0);
	}
	if (dstat & SIOP_DSTAT_SIR && rp->siop_dsps == 0xff0a) {
		/* Status phase wasn't followed by message in phase? */
		printf ("%s: Status phase not followed by message in phase? sbcl %x sbdl %x\n",
			sc->sc_dev.dv_xname, rp->siop_sbcl, rp->siop_sbdl);
		if (rp->siop_sbcl == 0xa7) {
			/* It is now, just continue the script? */
			rp->siop_dcntl |= SIOP_DCNTL_STD;
			return (0);
		}
	}
	if (sstat0 == 0 && dstat & SIOP_DSTAT_SIR) {
		dma_cachectl (&acb->stat[0], 1);
		dma_cachectl (&acb->msg[0], 1);
		printf ("SIOP interrupt: %x sts %x msg %x %x sbcl %x\n",
		    rp->siop_dsps, acb->stat[0], acb->msg[0], acb->msg[1],
		    rp->siop_sbcl);
		siopreset (sc);
		*status = -1;
		return 0;	/* siopreset has cleaned up */
	}
	if (sstat0 & SIOP_SSTAT0_SGE)
		printf ("SIOP: SCSI Gross Error\n");
	if (sstat0 & SIOP_SSTAT0_PAR)
		printf ("SIOP: Parity Error\n");
	if (dstat & SIOP_DSTAT_IID)
		printf ("SIOP: Invalid instruction detected\n");
bad_phase:
	/*
	 * temporary panic for unhandled conditions
	 * displays various things about the 53C710 status and registers
	 * then panics.
	 * XXXX need to clean this up to print out the info, reset, and continue
	 */
	printf ("siopchkintr: target %x ds %x\n", target, &acb->ds);
	printf ("scripts %x ds %x rp %x dsp %x dcmd %x\n", sc->sc_scriptspa,
	    kvtop(&acb->ds), kvtop(rp), rp->siop_dsp,
	    *((long *)&rp->siop_dcmd));
	printf ("siopchkintr: istat %x dstat %x sstat0 %x dsps %x dsa %x sbcl %x sts %x msg %x %x sfbr %x\n",
	    istat, dstat, sstat0, rp->siop_dsps, rp->siop_dsa,
	     rp->siop_sbcl, acb->stat[0], acb->msg[0], acb->msg[1], rp->siop_sfbr);
#ifdef DEBUG
	if (siop_debug & 0x20)
		panic("siopchkintr: **** temp ****");
#endif
#ifdef DDB
	Debugger ();
#endif
	siopreset (sc);		/* hard reset */
	*status = -1;
	return 0;		/* siopreset cleaned up */
}

void
siop_select(sc)
	struct siop_softc *sc;
{
	siop_regmap_p rp;
	struct siop_acb *acb = sc->sc_nexus;

#ifdef DEBUG
	if (siop_debug & 1)
		printf ("%s: select ", sc->sc_dev.dv_xname);
#endif

	rp = sc->sc_siopp;
	if (acb->xs->flags & SCSI_POLL || siop_no_dma) {
		sc->sc_flags |= SIOP_INTSOFF;
		sc->sc_flags &= ~SIOP_INTDEFER;
		if ((rp->siop_istat & 0x08) == 0) {
			rp->siop_sien = 0;
			rp->siop_dien = 0;
		}
#if 0
	} else if ((sc->sc_flags & SIOP_INTDEFER) == 0) {
		sc->sc_flags &= ~SIOP_INTSOFF;
		if ((rp->siop_istat & 0x08) == 0) {
			rp->siop_sien = sc->sc_sien;
			rp->siop_dien = sc->sc_dien;
		}
#endif
	}
#ifdef DEBUG
	if (siop_debug & 1)
		printf ("siop_select: target %x cmd %02x ds %x\n",
		    acb->xs->sc_link->target, acb->cmd.opcode,
		    &sc->sc_nexus->ds);
#endif

	siop_start(sc, acb->xs->sc_link->target, acb->xs->sc_link->lun,
	    &acb->cmd, acb->clen, acb->daddr, acb->dleft);

	return;
}

/*
 * 53C710 interrupt handler
 */

int
siopintr (sc)
	register struct siop_softc *sc;
{
	siop_regmap_p rp;
	register u_char istat, dstat, sstat0;
	int status;
	int s = splbio();

	istat = sc->sc_istat;
	if ((istat & (SIOP_ISTAT_SIP | SIOP_ISTAT_DIP)) == 0) {
		splx(s);
		return;
	}

	/* Got a valid interrupt on this device */
	rp = sc->sc_siopp;
	dstat = sc->sc_dstat;
	sstat0 = sc->sc_sstat0;
	if (dstat & SIOP_DSTAT_SIR)
		sc->sc_intcode = rp->siop_dsps;
	sc->sc_istat = 0;
#ifdef DEBUG
	if (siop_debug & 1)
		printf ("%s: intr istat %x dstat %x sstat0 %x\n",
		    sc->sc_dev.dv_xname, istat, dstat, sstat0);
	if (!sc->sc_active) {
		printf ("%s: spurious interrupt? istat %x dstat %x sstat0 %x status %x\n",
		    sc->sc_dev.dv_xname, istat, dstat, sstat0, sc->sc_nexus->stat[0]);
	}
#endif

#ifdef DEBUG
	if (siop_debug & 5) {
		DCIAS(kvtop(&sc->sc_nexus->stat[0]));
		printf ("%s: intr istat %x dstat %x sstat0 %x dsps %x sbcl %x sts %x msg %x\n",
		    sc->sc_dev.dv_xname, istat, dstat, sstat0,
		    rp->siop_dsps,  rp->siop_sbcl,
		    sc->sc_nexus->stat[0], sc->sc_nexus->msg[0]);
	}
#endif
	if (sc->sc_flags & SIOP_INTDEFER) {
		sc->sc_flags &= ~(SIOP_INTDEFER | SIOP_INTSOFF);
		rp->siop_sien = sc->sc_sien;
		rp->siop_dien = sc->sc_dien;
	}
	if (siop_checkintr (sc, istat, dstat, sstat0, &status)) {
#if 1
		if (status == 0xff)
			printf ("siopintr: status == 0xff\n");
#endif
		if ((sc->sc_flags & (SIOP_INTSOFF | SIOP_INTDEFER)) != SIOP_INTSOFF) {
#if 0
			if (rp->siop_sbcl & SIOP_BSY) {
				printf ("%s: SCSI bus busy at completion",
					sc->sc_dev.dv_xname);
				printf(" targ %d sbcl %02x sfbr %x lcrc %02x dsp +%x\n",
				    sc->sc_nexus->xs->sc_link->target,
				    rp->siop_sbcl, rp->siop_sfbr, rp->siop_lcrc,
				    rp->siop_dsp - sc->sc_scriptspa);
			}
#endif
			siop_scsidone(sc->sc_nexus, sc->sc_nexus->stat[0]);
		}
	}
	splx(s);
}

/*
 * This is based on the Progressive Peripherals 33Mhz Zeus driver and will
 * not be correct for other 53c710 boards.
 *
 */
scsi_period_to_siop (sc, target)
	struct siop_softc *sc;
{
	int period, offset, i, sxfer, sbcl;

	period = sc->sc_nexus->msg[4];
	offset = sc->sc_nexus->msg[5];
#ifdef DEBUG
	sxfer = 0;
	if (offset <= SIOP_MAX_OFFSET)
		sxfer = offset;
	for (i = 0; i < sizeof (sync_tab) / 2; ++i) {
		if (period <= sync_tab[i].p) {
			sxfer |= sync_tab[i].r & 0x70;
			sbcl = sync_tab[i].r & 0x03;
			break;
		}
	}
	printf ("siop sync old: siop_sxfr %02x, siop_sbcl %02x\n", sxfer, sbcl);
#endif
	for (sbcl = 1; sbcl < 4; ++sbcl) {
		sxfer = (period * 4 - 1) / sc->sc_tcp[sbcl] - 3;
		if (sxfer >= 0 && sxfer <= 7)
			break;
	}
	if (sbcl > 3) {
		printf("siop sync: unable to compute sync params for period %dns\n",
		    period * 4);
		/*
		 * XXX need to pick a value we can do and renegotiate
		 */
		sxfer = sbcl = 0;
	} else {
		sxfer = (sxfer << 4) | ((offset <= SIOP_MAX_OFFSET) ?
		    offset : SIOP_MAX_OFFSET);
		printf("siop sync: params for period %dns: sxfer %x sbcl %x",
		    period * 4, sxfer, sbcl);
		printf(" actual period %dns\n",
		    sc->sc_tcp[sbcl] * ((sxfer >> 4) + 4));
	}
	sc->sc_sync[target].sxfer = sxfer;
	sc->sc_sync[target].sbcl = sbcl;
#ifdef DEBUG
	printf ("siop sync: siop_sxfr %02x, siop_sbcl %02x\n", sxfer, sbcl);
#endif
}

#ifdef DEBUG

#if SIOP_TRACE_SIZE
void
siop_dump_trace()
{
	int i;

	printf("siop trace: next index %d\n", siop_trix);
	i = siop_trix;
	do {
		printf("%3d: '%c' %02x %02x %02x\n", i, siop_trbuf[i],
		    siop_trbuf[i + 1], siop_trbuf[i + 2], siop_trbuf[i + 3]);
		i = (i + 4) & (SIOP_TRACE_SIZE - 1);
	} while (i != siop_trix);
}
#endif

void
siop_dump_acb(acb)
	struct siop_acb *acb;
{
	u_char *b = (u_char *) &acb->cmd;
	int i;

	printf("acb@%x ", acb);
	if (acb->xs == NULL) {
		printf("<unused>\n");
		return;
	}
	printf("(%d:%d) flags %2x clen %2d cmd ", acb->xs->sc_link->target,
	    acb->xs->sc_link->lun, acb->flags, acb->clen);
	for (i = acb->clen; i; --i)
		printf(" %02x", *b++);
	printf("\n");
	printf("  xs: %08x data %8x:%04x ", acb->xs, acb->xs->data,
	    acb->xs->datalen);
	printf("va %8x:%04x ", acb->iob_buf, acb->iob_len);
	printf("cur %8x:%04x\n", acb->iob_curbuf, acb->iob_curlen);
}

void
siop_dump(sc)
	struct siop_softc *sc;
{
	struct siop_acb *acb;
	siop_regmap_p rp = sc->sc_siopp;
	int s;
	int i;

	s = splbio();
#if SIOP_TRACE_SIZE
	siop_dump_trace();
#endif
	printf("%s@%x regs %x istat %x\n",
	    sc->sc_dev.dv_xname, sc, rp, rp->siop_istat);
	if (acb = sc->free_list.tqh_first) {
		printf("Free list:\n");
		while (acb) {
			siop_dump_acb(acb);
			acb = acb->chain.tqe_next;
		}
	}
	if (acb = sc->ready_list.tqh_first) {
		printf("Ready list:\n");
		while (acb) {
			siop_dump_acb(acb);
			acb = acb->chain.tqe_next;
		}
	}
	if (acb = sc->nexus_list.tqh_first) {
		printf("Nexus list:\n");
		while (acb) {
			siop_dump_acb(acb);
			acb = acb->chain.tqe_next;
		}
	}
	if (sc->sc_nexus) {
		printf("Nexus:\n");
		siop_dump_acb(sc->sc_nexus);
	}
	for (i = 0; i < 8; ++i) {
		if (sc->sc_tinfo[i].cmds > 2) {
			printf("tgt %d: cmds %d disc %d senses %d lubusy %x\n",
			    i, sc->sc_tinfo[i].cmds,
			    sc->sc_tinfo[i].dconns,
			    sc->sc_tinfo[i].senses,
			    sc->sc_tinfo[i].lubusy);
		}
	}
	splx(s);
}
#endif
