/*	$OpenBSD: ssh.c,v 1.4 2003/02/11 19:20:26 mickey Exp $ */

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
 *	@(#)ssh.c	7.5 (Berkeley) 5/4/91
 */

/*
 * 53C710 scsi adaptor driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/malloc.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <mvme68k/dev/sshreg.h>
#include <mvme68k/dev/sshvar.h>

/*
 * SCSI delays
 * In u-seconds, primarily for state changes on the SPC.
 */
#define	SCSI_CMD_WAIT	500000	/* wait per step of 'immediate' cmds */
#define	SCSI_DATA_WAIT	500000	/* wait per data in/out step */
#define	SCSI_INIT_WAIT	500000	/* wait per step (both) during init */

void ssh_select(struct ssh_softc *);
void sshabort(struct ssh_softc *, ssh_regmap_p, char *);
void ssherror(struct ssh_softc *, ssh_regmap_p, u_char);
void sshstart(struct ssh_softc *);
void sshreset(struct ssh_softc *);
void sshsetdelay(int);
void ssh_scsidone(struct ssh_acb *, int);
void ssh_sched(struct ssh_softc *);
int  ssh_poll(struct ssh_softc *, struct ssh_acb *);
void sshintr(struct ssh_softc *);
void sshinitialize(struct ssh_softc *);
void ssh_start(struct ssh_softc *, int, int, u_char *, int, u_char *, int);
int  ssh_checkintr(struct ssh_softc *, u_char, u_char, u_char, int *);
void scsi_period_to_ssh(struct ssh_softc *, int);

/* 53C710 script */
const
#include <mvme68k/dev/ssh_script.out>

/* default to not inhibit sync negotiation on any drive */
u_char ssh_inhibit_sync[8] = { 0, 0, 0, 0, 0, 0, 0};	/* initialize, so patchable */
u_char ssh_allow_disc[8] = { 3, 3, 3, 3, 3, 3, 3, 3};
int ssh_no_dma = 0;

int ssh_reset_delay = 250;	/* delay after reset, in milleseconds */

int ssh_cmd_wait = SCSI_CMD_WAIT;
int ssh_data_wait = SCSI_DATA_WAIT;
int ssh_init_wait = SCSI_INIT_WAIT;

#ifdef DEBUG
/*
 *	0x01 - full debug
 *	0x02 - DMA chaining
 *	0x04 - sshintr
 *	0x08 - phase mismatch
 *	0x10 - <not used>
 *	0x20 - panic on unhandled exceptions
 *	0x100 - disconnect/reselect
 */
int   ssh_debug = 0;
int   sshsync_debug = 0;
int   sshdma_hits = 0;
int   sshdma_misses = 0;
int   sshchain_ints = 0;
int   sshstarts = 0;
int   sshints = 0;
int   sshphmm = 0;
#define SSH_TRACE_SIZE	128
#define SSH_TRACE(a,b,c,d) \
	ssh_trbuf[ssh_trix] = (a); \
	ssh_trbuf[ssh_trix+1] = (b); \
	ssh_trbuf[ssh_trix+2] = (c); \
	ssh_trbuf[ssh_trix+3] = (d); \
	ssh_trix = (ssh_trix + 4) & (SSH_TRACE_SIZE - 1);
u_char   ssh_trbuf[SSH_TRACE_SIZE];
int   ssh_trix;
#else
#define SSH_TRACE(a,b,c,d)
#endif


/*
 * default minphys routine for ssh based controllers
 */
void
ssh_minphys(bp)
struct buf *bp;
{

	/*
	 * No max transfer at this level.
	 */
	minphys(bp);
}

/*
 * used by specific ssh controller
 *
 */
int
ssh_scsicmd(xs)
struct scsi_xfer *xs;
{
	struct ssh_acb *acb;
	struct ssh_softc *sc;
	struct scsi_link *slp;
	int flags, s;

	slp = xs->sc_link;
	sc = slp->adapter_softc;
	flags = xs->flags;

	/* XXXX ?? */
	if (flags & SCSI_DATA_UIO)
		panic("ssh: scsi data uio requested");

	/* XXXX ?? */
	if (sc->sc_nexus && flags & SCSI_POLL)
		panic("ssh_scsicmd: busy");

	s = splbio();
	acb = sc->free_list.tqh_first;
	if (acb) {
		TAILQ_REMOVE(&sc->free_list, acb, chain);
	}
	splx(s);

	if (acb == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
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
		ssh_sched(sc);

	splx(s);

	if (flags & SCSI_POLL || ssh_no_dma)
		return (ssh_poll(sc, acb));
	return (SUCCESSFULLY_QUEUED);
}

int
ssh_poll(sc, acb)
struct ssh_softc *sc;
struct ssh_acb *acb;
{
	ssh_regmap_p rp = sc->sc_sshp;
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
		printf("%s: ssh_poll called with disconnected device\n",
				 sc->sc_dev.dv_xname);
	for (;;) {
		/* use cmd_wait values? */
		i = 50000;
		spl0();
		while (((istat = rp->ssh_istat) &
				  (SSH_ISTAT_SIP | SSH_ISTAT_DIP)) == 0) {
			if (--i <= 0) {
#ifdef DEBUG
				printf ("waiting: tgt %d cmd %02x sbcl %02x dsp %x (+%x) dcmd %x ds %x timeout %d\n",
						  xs->sc_link->target, acb->cmd.opcode,
						  rp->ssh_sbcl, rp->ssh_dsp,
						  rp->ssh_dsp - sc->sc_scriptspa,
						  *((long *)&rp->ssh_dcmd), &acb->ds, acb->xs->timeout);
#endif
				i = 50000;
				--to;
				if (to <= 0) {
					sshreset(sc);
					return (COMPLETE);
				}
			}
			delay(10);
		}
		sstat0 = rp->ssh_sstat0;
		dstat = rp->ssh_dstat;
		if (ssh_checkintr(sc, istat, dstat, sstat0, &status)) {
			if (acb != sc->sc_nexus)
				printf("%s: ssh_poll disconnected device completed\n",
						 sc->sc_dev.dv_xname);
			else if ((sc->sc_flags & SSH_INTDEFER) == 0) {
				sc->sc_flags &= ~SSH_INTSOFF;
				rp->ssh_sien = sc->sc_sien;
				rp->ssh_dien = sc->sc_dien;
			}
			ssh_scsidone(sc->sc_nexus, status);
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
ssh_sched(sc)
struct ssh_softc *sc;
{
	struct scsi_link *slp;
	struct ssh_acb *acb;
	int i;

#ifdef DEBUG
	if (sc->sc_nexus) {
		printf("%s: ssh_sched- nexus %x/%d ready %x/%d\n",
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
		if (!(sc->sc_tinfo[i].lubusy & (1 << slp->lun))) {
			struct ssh_tinfo *ti = &sc->sc_tinfo[i];

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
		printf("%s: ssh_sched didn't find ready command\n",
				 sc->sc_dev.dv_xname);
#endif
		return;
	}

	if (acb->xs->flags & SCSI_RESET)
		sshreset(sc);

#if 0
	acb->cmd.bytes[0] |= slp->lun << 5;	/* XXXX */
#endif
	++sc->sc_active;
	ssh_select(sc);
}

void
ssh_scsidone(acb, stat)
struct ssh_acb *acb;
int stat;
{
	struct scsi_xfer *xs = acb->xs;
	struct scsi_link *slp = xs->sc_link;
	struct ssh_softc *sc = slp->adapter_softc;
	int dosched = 0;

#ifdef DIAGNOSTIC
	if (acb == NULL || xs == NULL)
		panic("ssh_scsidone");
#endif
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
				ssh_sched(sc);
			}
			SSH_TRACE('d','s',0,0)
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
		SSH_TRACE('d','a',stat,0)
	} else if (sc->ready_list.tqh_last == &acb->chain.tqe_next) {
		TAILQ_REMOVE(&sc->ready_list, acb, chain);
		SSH_TRACE('d','r',stat,0)
	} else {
		register struct ssh_acb *acb2;
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
		SSH_TRACE('d','n',stat,0);
	}
	/* Put it on the free list. */
	acb->flags = ACB_FREE;
	TAILQ_INSERT_HEAD(&sc->free_list, acb, chain);

	sc->sc_tinfo[slp->target].cmds++;

	scsi_done(xs);

	if (dosched && sc->sc_nexus == NULL)
		ssh_sched(sc);
}

void
sshabort(sc, rp, where)
register struct ssh_softc *sc;
ssh_regmap_p rp;
char *where;
{
#ifdef fix_this
	int i;
#endif

	printf ("%s: abort %s: dstat %02x, sstat0 %02x sbcl %02x\n",
			  sc->sc_dev.dv_xname,
			  where, rp->ssh_dstat, rp->ssh_sstat0, rp->ssh_sbcl);

	if (sc->sc_active > 0) {
#ifdef TODO
		SET_SBIC_cmd (rp, SBIC_CMD_ABORT);
		WAIT_CIP (rp);

		GET_SBIC_asr (rp, asr);
		if (asr & (SBIC_ASR_BSY|SBIC_ASR_LCI)) {
			/* ok, get more drastic.. */

			SET_SBIC_cmd (rp, SBIC_CMD_RESET);
			delay(25);
			SBIC_WAIT(rp, SBIC_ASR_INT, 0);
			GET_SBIC_csr (rp, csr);			/* clears interrupt also */

			return;
		}

		do {
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
				printf ("sshabort: cleanup!\n");
				sc->sc_iob[i].sc_xs = NULL;
			}
		}
#endif	/* fix_this */
/*		sc->sc_active = 0; */
	}
}

void
sshinitialize(sc)
	struct ssh_softc *sc;
{
	/*
	 * Need to check that scripts is on a long word boundary
	 * Also should verify that dev doesn't span non-contiguous
	 * physical pages.
	 */
	sc->sc_scriptspa = kvtop((vaddr_t)scripts);

	/*
	 * malloc sc_acb to ensure that DS is on a long word boundary.
	 */

	MALLOC(sc->sc_acb, struct ssh_acb *, 
			 sizeof(struct ssh_acb) * SSH_NACB, M_DEVBUF, M_NOWAIT);
	if (sc->sc_acb == NULL)
		panic("sshinitialize: ACB malloc failed!");

	sc->sc_tcp[1] = 1000 / sc->sc_clock_freq;
	sc->sc_tcp[2] = 1500 / sc->sc_clock_freq;
	sc->sc_tcp[3] = 2000 / sc->sc_clock_freq;
	sc->sc_minsync = sc->sc_tcp[1];		/* in 4ns units */
	if (sc->sc_minsync < 25)
		sc->sc_minsync = 25;
#if not_used
	if (sc->sc_clock_freq <= 25)
		sc->sc_tcp[0] = sc->sc_tcp[1];
	else if (sc->sc_clock_freq <= 37)
		sc->sc_tcp[0] = sc->sc_tcp[2];
	else if (sc->sc_clock_freq <= 50)
		sc->sc_tcp[0] = sc->sc_tcp[3];
	else
		sc->sc_tcp[0] = 3000 / sc->sc_clock_freq;
#endif

	sshreset (sc);
}

void
sshreset(sc)
struct ssh_softc *sc;
{
	ssh_regmap_p rp;
	u_int i, s;
	u_char  dummy;
	struct ssh_acb *acb;

	rp = sc->sc_sshp;

	if (sc->sc_flags & SSH_ALIVE)
		sshabort(sc, rp, "reset");

	s = splbio();

	/*
	 * Reset the chip
	 * XXX - is this really needed?
	 */
	rp->ssh_istat |= SSH_ISTAT_ABRT;	/* abort current script */
	rp->ssh_istat |= SSH_ISTAT_RST;		/* reset chip */
	rp->ssh_istat &= ~SSH_ISTAT_RST;
	/*
	 * Reset SCSI bus (do we really want this?)
	 */
	rp->ssh_sien = 0;
	rp->ssh_scntl1 |= SSH_SCNTL1_RST;
	delay(1);
	rp->ssh_scntl1 &= ~SSH_SCNTL1_RST;

	/*
	 * Set up various chip parameters
	 */
	rp->ssh_scntl0 = SSH_ARB_FULL | SSH_SCNTL0_EPC | SSH_SCNTL0_EPG;
	rp->ssh_scntl1 = SSH_SCNTL1_ESR;
	rp->ssh_dcntl = sc->sc_dcntl;
	rp->ssh_dmode = 0x80;	/* burst length = 4 */
	rp->ssh_sien = 0x00;	/* don't enable interrupts yet */
	rp->ssh_dien = 0x00;	/* don't enable interrupts yet */
	rp->ssh_scid = 1 << sc->sc_link.adapter_target;
	rp->ssh_dwt = 0x00;
	rp->ssh_ctest0 |= SSH_CTEST0_BTD | SSH_CTEST0_EAN;
	rp->ssh_ctest7 = sc->sc_ctest7;

	/* will need to re-negotiate sync xfers */
	bzero(&sc->sc_sync, sizeof (sc->sc_sync));

	i = rp->ssh_istat;
	if (i & SSH_ISTAT_SIP)
		dummy = rp->ssh_sstat0;
	if (i & SSH_ISTAT_DIP)
		dummy = rp->ssh_dstat;

	splx(s);

	delay(ssh_reset_delay * 1000);
	printf(": version %d target %d\n", rp->ssh_ctest8 >> 4,
			 sc->sc_link.adapter_target);

	if ((sc->sc_flags & SSH_ALIVE) == 0) {
		TAILQ_INIT(&sc->ready_list);
		TAILQ_INIT(&sc->nexus_list);
		TAILQ_INIT(&sc->free_list);
		sc->sc_nexus = NULL;
		acb = sc->sc_acb;
		bzero(acb, sizeof(struct ssh_acb) * SSH_NACB);
		for (i = 0; i < SSH_NACB; i++) {
			TAILQ_INSERT_TAIL(&sc->free_list, acb, chain);
			acb++;
		}
		bzero(sc->sc_tinfo, sizeof(sc->sc_tinfo));
	} else {
		if (sc->sc_nexus != NULL) {
			sc->sc_nexus->xs->error = XS_DRIVER_STUFFUP;
			ssh_scsidone(sc->sc_nexus, sc->sc_nexus->stat[0]);
		}
		while ((acb = sc->nexus_list.tqh_first)) {
			acb->xs->error = XS_DRIVER_STUFFUP;
			ssh_scsidone(acb, acb->stat[0]);
		}
	}

	sc->sc_flags |= SSH_ALIVE;
	sc->sc_flags &= ~(SSH_INTDEFER|SSH_INTSOFF);
	/* enable SCSI and DMA interrupts */
	sc->sc_sien = SSH_SIEN_M_A | SSH_SIEN_STO | /*SSH_SIEN_SEL |*/ SSH_SIEN_SGE |
					  SSH_SIEN_UDC | SSH_SIEN_RST | SSH_SIEN_PAR;
	sc->sc_dien = SSH_DIEN_BF | SSH_DIEN_ABRT | SSH_DIEN_SIR |
					  /*SSH_DIEN_WTD |*/ SSH_DIEN_IID;
	rp->ssh_sien = sc->sc_sien;
	rp->ssh_dien = sc->sc_dien;
}

/*
 * Setup Data Storage for 53C710 and start SCRIPTS processing
 */

void
ssh_start (sc, target, lun, cbuf, clen, buf, len)
	struct ssh_softc *sc;
	int target;
	int lun;
	u_char *cbuf;
	int clen;
	u_char *buf;
	int len;
{
	ssh_regmap_p rp = sc->sc_sshp;
#ifdef DEBUG
	int i;
#endif
	int nchain;
	int count, tcount;
	char *addr, *dmaend;
	struct ssh_acb *acb = sc->sc_nexus;

#ifdef DEBUG
	if (ssh_debug & 0x100 && rp->ssh_sbcl & SSH_BSY) {
		printf ("ACK! ssh was busy: rp %x script %x dsa %x active %d\n",
				  rp, &scripts, &acb->ds, sc->sc_active);
		printf ("istat %02x sfbr %02x lcrc %02x sien %02x dien %02x\n",
				  rp->ssh_istat, rp->ssh_sfbr, rp->ssh_lcrc,
				  rp->ssh_sien, rp->ssh_dien);
#ifdef DDB
		/*Debugger();*/
#endif
	}
#endif
	acb->msgout[0] = MSG_IDENTIFY | lun;
	if (ssh_allow_disc[target] & 2 ||
		 (ssh_allow_disc[target] && len == 0))
		acb->msgout[0] = MSG_IDENTIFY_DR | lun;
	acb->status = 0;
	acb->stat[0] = -1;
	acb->msg[0] = -1;
	acb->ds.scsi_addr = (0x10000 << target) | (sc->sc_sync[target].sxfer << 8);
	acb->ds.idlen = 1;
	acb->ds.idbuf = (char *) kvtop((vaddr_t)&acb->msgout[0]);
	acb->ds.cmdlen = clen;
	acb->ds.cmdbuf = (char *) kvtop((vaddr_t)cbuf);
	acb->ds.stslen = 1;
	acb->ds.stsbuf = (char *) kvtop((vaddr_t)&acb->stat[0]);
	acb->ds.msglen = 1;
	acb->ds.msgbuf = (char *) kvtop((vaddr_t)&acb->msg[0]);
	acb->msg[1] = -1;
	acb->ds.msginlen = 1;
	acb->ds.extmsglen = 1;
	acb->ds.synmsglen = 3;
	acb->ds.msginbuf = (char *) kvtop((vaddr_t)&acb->msg[1]);
	acb->ds.extmsgbuf = (char *) kvtop((vaddr_t)&acb->msg[2]);
	acb->ds.synmsgbuf = (char *) kvtop((vaddr_t)&acb->msg[3]);
	bzero(&acb->ds.chain, sizeof (acb->ds.chain));

	if (sc->sc_sync[target].state == SYNC_START) {
		if (ssh_inhibit_sync[target]) {
			sc->sc_sync[target].state = SYNC_DONE;
			sc->sc_sync[target].sbcl = 0;
			sc->sc_sync[target].sxfer = 0;
#ifdef DEBUG
			if (sshsync_debug)
				printf ("Forcing target %d asynchronous\n", target);
#endif
		} else {
			acb->msg[2] = -1;
			acb->msgout[1] = MSG_EXT_MESSAGE;
			acb->msgout[2] = 3;
			acb->msgout[3] = MSG_SYNC_REQ;
#ifdef MAXTOR_SYNC_KLUDGE
			acb->msgout[4] = 50 / 4;	/* ask for ridiculous period */
#else
			acb->msgout[4] = sc->sc_minsync;
#endif
			acb->msgout[5] = SSH_MAX_OFFSET;
			acb->ds.idlen = 6;
			sc->sc_sync[target].state = SYNC_SENT;
#ifdef DEBUG
			if (sshsync_debug)
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
		acb->ds.chain[nchain].databuf = (char *) kvtop ((vaddr_t)addr);
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
			++sshdma_hits;
#endif
		} else {
			dmaend = acb->ds.chain[nchain].databuf +
						acb->ds.chain[nchain].datalen;
			acb->ds.chain[nchain].datalen = tcount;
#ifdef DEBUG
			if (nchain)	/* Don't count miss on first one */
				++sshdma_misses;
#endif
		}
		++nchain;
	}
#ifdef DEBUG
	if (nchain != 1 && len != 0 && ssh_debug & 3) {
		printf ("DMA chaining set: %d\n", nchain);
		for (i = 0; i < nchain; ++i) {
			printf ("  [%d] %8x %4x\n", i, acb->ds.chain[i].databuf,
					  acb->ds.chain[i].datalen);
		}
	}
#endif

	/* push data cache for all data the 53c710 needs to access */
	dma_cachectl ((caddr_t)acb, sizeof (struct ssh_acb));
	dma_cachectl (cbuf, clen);
	if (buf != NULL && len != 0)
		dma_cachectl (buf, len);

#ifdef DEBUG
	if (ssh_debug & 0x100 && rp->ssh_sbcl & SSH_BSY) {
		printf ("ACK! ssh was busy at start: rp %x script %x dsa %x active %d\n",
				  rp, &scripts, &acb->ds, sc->sc_active);
#ifdef DDB
		/*Debugger();*/
#endif
	}
#endif

	if (sc->nexus_list.tqh_first == NULL) {
		if (rp->ssh_istat & SSH_ISTAT_CON)
			printf("%s: ssh_select while connected?\n",
					 sc->sc_dev.dv_xname);
		rp->ssh_temp = 0;
		rp->ssh_sbcl = sc->sc_sync[target].sbcl;
		rp->ssh_dsa = kvtop((vaddr_t)&acb->ds);
		rp->ssh_dsp = sc->sc_scriptspa;
		SSH_TRACE('s',1,0,0)
	} else {
		if ((rp->ssh_istat & SSH_ISTAT_CON) == 0) {
			rp->ssh_istat = SSH_ISTAT_SIGP;
			SSH_TRACE('s',2,0,0);
		} else {
			SSH_TRACE('s',3,rp->ssh_istat,0);
		}
	}
#ifdef DEBUG
	++sshstarts;
#endif
}

/*
 * Process a DMA or SCSI interrupt from the 53C710 SSH
 */

int
ssh_checkintr(sc, istat, dstat, sstat0, status)
	struct   ssh_softc *sc;
	u_char   istat;
	u_char   dstat;
	u_char   sstat0;
	int   *status;
{
	ssh_regmap_p rp = sc->sc_sshp;
	struct ssh_acb *acb = sc->sc_nexus;
	int   target;
	int   dfifo, dbc, sstat1;

	dfifo = rp->ssh_dfifo;
	dbc = rp->ssh_dbc0;
	sstat1 = rp->ssh_sstat1;
	rp->ssh_ctest8 |= SSH_CTEST8_CLF;
	while ((rp->ssh_ctest1 & SSH_CTEST1_FMT) != SSH_CTEST1_FMT)
		;
	rp->ssh_ctest8 &= ~SSH_CTEST8_CLF;
#ifdef DEBUG
	++sshints;
#if 0
	if (ssh_debug & 0x100) {
		DCIAS(&acb->stat[0]);	/* XXX */
		printf ("sshchkintr: istat %x dstat %x sstat0 %x dsps %x sbcl %x sts %x msg %x\n",
				  istat, dstat, sstat0, rp->ssh_dsps, rp->ssh_sbcl, acb->stat[0], acb->msg[0]);
		printf ("sync msg in: %02x %02x %02x %02x %02x %02x\n",
				  acb->msg[0], acb->msg[1], acb->msg[2],
				  acb->msg[3], acb->msg[4], acb->msg[5]);
	}
#endif
	if (rp->ssh_dsp && (rp->ssh_dsp < sc->sc_scriptspa ||
								rp->ssh_dsp >= sc->sc_scriptspa + sizeof(scripts))) {
		printf ("%s: dsp not within script dsp %x scripts %x:%x",
				  sc->sc_dev.dv_xname, rp->ssh_dsp, sc->sc_scriptspa,
				  sc->sc_scriptspa + sizeof(scripts));
		printf(" istat %x dstat %x sstat0 %x\n",
				 istat, dstat, sstat0);
		Debugger();
	}
#endif
	SSH_TRACE('i',dstat,istat,(istat&SSH_ISTAT_DIP)?rp->ssh_dsps&0xff:sstat0);
	if (dstat & SSH_DSTAT_SIR && rp->ssh_dsps == 0xff00) {
		/* Normal completion status, or check condition */
#ifdef DEBUG
		if (rp->ssh_dsa != kvtop(&acb->ds)) {
			printf ("ssh: invalid dsa: %x %x\n", rp->ssh_dsa,
					  kvtop(&acb->ds));
			panic("*** ssh DSA invalid ***");
		}
#endif
		target = acb->xs->sc_link->target;
		if (sc->sc_sync[target].state == SYNC_SENT) {
#ifdef DEBUG
			if (sshsync_debug)
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
						acb->msg[4]	= 100 / 4;
				}
#endif /* MAXTOR_KLUDGE */
				printf ("%s: target %d now synchronous, period=%dns, offset=%d\n",
						  sc->sc_dev.dv_xname, target,
						  acb->msg[4] * 4, acb->msg[5]);
				scsi_period_to_ssh (sc, target);
			}
		}
		dma_cachectl(&acb->stat[0], 1);
		*status = acb->stat[0];
#ifdef DEBUG
		if (rp->ssh_sbcl & SSH_BSY) {
			/*printf ("ACK! ssh was busy at end: rp %x script %x dsa %x\n",
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
			rp->ssh_dcntl |= SSH_DCNTL_STD;
		return 1;
	}
	if (sstat0 & SSH_SSTAT0_M_A) {		/* Phase mismatch */
#ifdef DEBUG
		++sshphmm;
		if (acb == NULL)
			printf("%s: Phase mismatch with no active command?\n",
					 sc->sc_dev.dv_xname);
#endif
		if (acb->iob_len) {
			int adjust;
			adjust = ((dfifo - (dbc & 0x7f)) & 0x7f);
			if (sstat1 & SSH_SSTAT1_ORF)
				++adjust;
			if (sstat1 & SSH_SSTAT1_OLF)
				++adjust;
			acb->iob_curlen = *((long *)&rp->ssh_dcmd) & 0xffffff;
			acb->iob_curlen += adjust;
			acb->iob_curbuf = *((long *)&rp->ssh_dnad) - adjust;
#ifdef DEBUG
			if (ssh_debug & 0x100) {
				int i;
				printf ("Phase mismatch: curbuf %x curlen %x dfifo %x dbc %x sstat1 %x adjust %x sbcl %x starts %d acb %x\n",
						  acb->iob_curbuf, acb->iob_curlen, dfifo,
						  dbc, sstat1, adjust, rp->ssh_sbcl, sshstarts, acb);
				if (acb->ds.chain[1].datalen) {
					for (i = 0; acb->ds.chain[i].datalen; ++i)
						printf("chain[%d] addr %x len %x\n",
								 i, acb->ds.chain[i].databuf,
								 acb->ds.chain[i].datalen);
				}
			}
#endif
			dma_cachectl ((caddr_t)acb, sizeof(*acb));
		}
#ifdef DEBUG
		SSH_TRACE('m',rp->ssh_sbcl,(rp->ssh_dsp>>8),rp->ssh_dsp);
		if (ssh_debug & 9)
			printf ("Phase mismatch: %x dsp +%x dcmd %x\n",
					  rp->ssh_sbcl,
					  rp->ssh_dsp - sc->sc_scriptspa,
					  *((long *)&rp->ssh_dcmd));
#endif
		if ((rp->ssh_sbcl & SSH_REQ) == 0) {
			printf ("Phase mismatch: REQ not asserted! %02x dsp %x\n",
					  rp->ssh_sbcl, rp->ssh_dsp);
#ifdef DEBUG
			Debugger();
#endif
		}
		switch (rp->ssh_sbcl & 7) {
			case 0:		/* data out */
			case 1:		/* data in */
			case 2:		/* status */
			case 3:		/* command */
			case 6:		/* message in */
			case 7:		/* message out */
				rp->ssh_dsp = sc->sc_scriptspa + Ent_switch;
				break;
			default:
				goto bad_phase;
		}
		return 0;
	}
	if (sstat0 & SSH_SSTAT0_STO) {		/* Select timed out */
#ifdef DEBUG
		if (acb == NULL)
			printf("%s: Select timeout with no active command?\n",
					 sc->sc_dev.dv_xname);
		if (rp->ssh_sbcl & SSH_BSY) {
			printf ("ACK! ssh was busy at timeout: rp %x script %x dsa %x\n",
					  rp, &scripts, &acb->ds);
			printf(" sbcl %x sdid %x istat %x dstat %x sstat0 %x\n",
					 rp->ssh_sbcl, rp->ssh_sdid, istat, dstat, sstat0);
			if (!(rp->ssh_sbcl & SSH_BSY)) {
				printf ("Yikes, it's not busy now!\n");
#if 0
				*status = -1;
				if (sc->nexus_list.tqh_first)
					rp->ssh_dsp = sc->sc_scriptspa + Ent_wait_reselect;
				return 1;
#endif
			}
/*			rp->ssh_dcntl |= SSH_DCNTL_STD;*/
			return (0);
#ifdef DDB
			Debugger();
#endif
		}
#endif
		*status = -1;
		acb->xs->error = XS_SELTIMEOUT;
		if (sc->nexus_list.tqh_first)
			rp->ssh_dsp = sc->sc_scriptspa + Ent_wait_reselect;
		return 1;
	}
	if (acb)
		target = acb->xs->sc_link->target;
	else
		target = 7;
	if (sstat0 & SSH_SSTAT0_UDC) {
#ifdef DEBUG
		if (acb == NULL)
			printf("%s: Unexpected disconnect with no active command?\n",
					 sc->sc_dev.dv_xname);
		printf ("%s: target %d disconnected unexpectedly\n",
				  sc->sc_dev.dv_xname, target);
#endif
#if 0
		sshabort (sc, rp, "sshchkintr");
#endif
		*status = STS_BUSY;
		if (sc->nexus_list.tqh_first)
			rp->ssh_dsp = sc->sc_scriptspa + Ent_wait_reselect;
		return 1;
	}
	if (dstat & SSH_DSTAT_SIR && (rp->ssh_dsps == 0xff01 ||
											 rp->ssh_dsps == 0xff02)) {
#ifdef DEBUG
		if (ssh_debug & 0x100)
			printf ("%s: ID %02x disconnected TEMP %x (+%x) curbuf %x curlen %x buf %x len %x dfifo %x dbc %x sstat1 %x starts %d acb %x\n",
					  sc->sc_dev.dv_xname, 1 << target, rp->ssh_temp,
					  rp->ssh_temp ? rp->ssh_temp - sc->sc_scriptspa : 0,
					  acb->iob_curbuf, acb->iob_curlen,
					  acb->ds.chain[0].databuf, acb->ds.chain[0].datalen, dfifo, dbc, sstat1, sshstarts, acb);
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
		if (acb->iob_len && rp->ssh_temp) {
			int n = rp->ssh_temp - sc->sc_scriptspa;

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
			if (ssh_debug & 0x100) {
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
			if (ssh_debug & 0x100)
				printf ("%s: adjusting DMA chain\n",
						  sc->sc_dev.dv_xname);
			if (rp->ssh_dsps == 0xff02)
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
			if (ssh_debug & 0x100)
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
				if (ssh_debug & 0x100)
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
			DCIAS(kvtop((vaddr_t)&acb->ds.chain));
		}
		++sc->sc_tinfo[target].dconns;
		/*
		 * add nexus to waiting list
		 * clear nexus
		 * try to start another command for another target/lun
		 */
		acb->status = sc->sc_flags & SSH_INTSOFF;
		TAILQ_INSERT_HEAD(&sc->nexus_list, acb, chain);
		sc->sc_nexus = NULL;		/* no current device */
		/* start script to wait for reselect */
		if (sc->sc_nexus == NULL)
			rp->ssh_dsp = sc->sc_scriptspa + Ent_wait_reselect;
/* XXXX start another command ? */
		if (sc->ready_list.tqh_first)
			ssh_sched(sc);
		return (0);
	}
	if (dstat & SSH_DSTAT_SIR && rp->ssh_dsps == 0xff03) {
		int reselid = rp->ssh_scratch & 0x7f;
		int reselun = rp->ssh_sfbr & 0x07;

		sc->sc_sstat1 = rp->ssh_sbcl;	/* XXXX save current SBCL */
#ifdef DEBUG
		if (ssh_debug & 0x100)
			printf ("%s: target ID %02x reselected dsps %x\n",
					  sc->sc_dev.dv_xname, reselid,
					  rp->ssh_dsps);
		if ((rp->ssh_sfbr & 0x80) == 0)
			printf("%s: Reselect message in was not identify: %x\n",
					 sc->sc_dev.dv_xname, rp->ssh_sfbr);
#endif
		if (sc->sc_nexus) {
#ifdef DEBUG
			if (ssh_debug & 0x100)
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
			DCIAS(kvtop((vaddr_t)&acb->stat[0]));
			rp->ssh_dsa = kvtop((vaddr_t)&acb->ds);
			rp->ssh_sxfer = sc->sc_sync[acb->xs->sc_link->target].sxfer;
			rp->ssh_sbcl = sc->sc_sync[acb->xs->sc_link->target].sbcl;
			break;
		}
		if (acb == NULL) {
			printf("%s: target ID %02x reselect nexus_list %x\n",
					 sc->sc_dev.dv_xname, reselid,
					 sc->nexus_list.tqh_first);
			panic("unable to find reselecting device");
		}
		dma_cachectl ((caddr_t)acb, sizeof(*acb));
		rp->ssh_temp = 0;
		rp->ssh_dcntl |= SSH_DCNTL_STD;
		return (0);
	}
	if (dstat & SSH_DSTAT_SIR && rp->ssh_dsps == 0xff04) {
#ifdef DEBUG
		u_short ctest2 = rp->ssh_ctest2;

		/* reselect was interrupted (by Sig_P or select) */
		if (ssh_debug & 0x100 ||
			 (ctest2 & SSH_CTEST2_SIGP) == 0)
			printf ("%s: reselect interrupted (Sig_P?) scntl1 %x ctest2 %x 
					  sfbr %x istat %x/%x\n", sc->sc_dev.dv_xname, rp->ssh_scntl1,
					  ctest2, rp->ssh_sfbr, istat, rp->ssh_istat);
#endif
		/* XXX assumes it was not select */
		if (sc->sc_nexus == NULL) {
			printf("%s: reselect interrupted, sc_nexus == NULL\n",
					 sc->sc_dev.dv_xname);
#if 0
			ssh_dump(sc);
#ifdef DDB
			Debugger();
#endif
#endif
			rp->ssh_dcntl |= SSH_DCNTL_STD;
			return (0);
		}
		target = sc->sc_nexus->xs->sc_link->target;
		rp->ssh_temp = 0;
		rp->ssh_dsa = kvtop((vaddr_t)&sc->sc_nexus->ds);
		rp->ssh_sxfer = sc->sc_sync[target].sxfer;
		rp->ssh_sbcl = sc->sc_sync[target].sbcl;
		rp->ssh_dsp = sc->sc_scriptspa;
		return (0);
	}
	if (dstat & SSH_DSTAT_SIR && rp->ssh_dsps == 0xff06) {
		if (acb == NULL)
			printf("%s: Bad message-in with no active command?\n",
					 sc->sc_dev.dv_xname);
		/* Unrecognized message in byte */
		dma_cachectl (&acb->msg[1],1);
		printf ("%s: Unrecognized message in data sfbr %x msg %x sbcl %x\n",
				  sc->sc_dev.dv_xname, rp->ssh_sfbr, acb->msg[1], rp->ssh_sbcl);
		/* what should be done here? */
		DCIAS(kvtop((vaddr_t)&acb->msg[1]));
		rp->ssh_dsp = sc->sc_scriptspa + Ent_switch;
		return (0);
	}
	if (dstat & SSH_DSTAT_SIR && rp->ssh_dsps == 0xff0a) {
		/* Status phase wasn't followed by message in phase? */
		printf ("%s: Status phase not followed by message in phase? sbcl %x sbdl %x\n",
				  sc->sc_dev.dv_xname, rp->ssh_sbcl, rp->ssh_sbdl);
		if (rp->ssh_sbcl == 0xa7) {
			/* It is now, just continue the script? */
			rp->ssh_dcntl |= SSH_DCNTL_STD;
			return (0);
		}
	}
	if (sstat0 == 0 && dstat & SSH_DSTAT_SIR) {
		dma_cachectl (&acb->stat[0], 1);
		dma_cachectl (&acb->msg[0], 1);
		printf ("SSH interrupt: %x sts %x msg %x %x sbcl %x\n",
				  rp->ssh_dsps, acb->stat[0], acb->msg[0], acb->msg[1],
				  rp->ssh_sbcl);
		sshreset (sc);
		*status = -1;
		return 0;	/* sshreset has cleaned up */
	}
	if (sstat0 & SSH_SSTAT0_SGE)
		printf ("SSH: SCSI Gross Error\n");
	if (sstat0 & SSH_SSTAT0_PAR)
		printf ("SSH: Parity Error\n");
	if (dstat & SSH_DSTAT_IID)
		printf ("SSH: Invalid instruction detected\n");
	bad_phase:
	/*
	 * temporary panic for unhandled conditions
	 * displays various things about the 53C710 status and registers
	 * then panics.
	 * XXXX need to clean this up to print out the info, reset, and continue
	 */
	printf ("sshchkintr: target %x ds %x\n", target, &acb->ds);
	printf ("scripts %x ds %x rp %x dsp %x dcmd %x\n", sc->sc_scriptspa,
	  kvtop((vaddr_t)&acb->ds), kvtop((vaddr_t)rp), rp->ssh_dsp,
	  *((long *)&rp->ssh_dcmd));
	printf ("sshchkintr: istat %x dstat %x sstat0 %x dsps %x "
			  "dsa %x sbcl %x sts %x msg %x %x sfbr %x\n",
			  istat, dstat, sstat0, rp->ssh_dsps, rp->ssh_dsa,
			  rp->ssh_sbcl, acb->stat[0], acb->msg[0], acb->msg[1],
			  rp->ssh_sfbr);
#ifdef DEBUG
	if (ssh_debug & 0x20)
		panic("sshchkintr: **** temp ****");
#ifdef DDB
	/* Debugger(); */
#endif
#endif
	sshreset (sc);		/* hard reset */
	*status = -1;
	return 0;		/* sshreset cleaned up */
}

void
ssh_select(sc)
struct ssh_softc *sc;
{
	ssh_regmap_p rp;
	struct ssh_acb *acb = sc->sc_nexus;

#ifdef DEBUG
	if (ssh_debug & 1)
		printf ("%s: select ", sc->sc_dev.dv_xname);
#endif

	rp = sc->sc_sshp;
	if (acb->xs->flags & SCSI_POLL || ssh_no_dma) {
		sc->sc_flags |= SSH_INTSOFF;
		sc->sc_flags &= ~SSH_INTDEFER;
		if ((rp->ssh_istat & 0x08) == 0) {
			rp->ssh_sien = 0;
			rp->ssh_dien = 0;
		}
#if 0
	} else if ((sc->sc_flags & SSH_INTDEFER) == 0) {
		sc->sc_flags &= ~SSH_INTSOFF;
		if ((rp->ssh_istat & 0x08) == 0) {
			rp->ssh_sien = sc->sc_sien;
			rp->ssh_dien = sc->sc_dien;
		}
#endif
	}
#ifdef DEBUG
	if (ssh_debug & 1)
		printf ("ssh_select: target %x cmd %02x ds %x\n",
				  acb->xs->sc_link->target, acb->cmd.opcode,
				  &sc->sc_nexus->ds);
#endif

	ssh_start(sc, acb->xs->sc_link->target, acb->xs->sc_link->lun,
	  (u_char *)&acb->cmd, acb->clen, acb->daddr, acb->dleft);

	return;
}

/*
 * 53C710 interrupt handler
 */
void
sshintr(sc)
	register struct ssh_softc *sc;
{
	ssh_regmap_p rp;
	register u_char istat, dstat, sstat0;
	int status;
	int s = splbio();

	istat = sc->sc_istat;
	if ((istat & (SSH_ISTAT_SIP | SSH_ISTAT_DIP)) == 0) {
		splx(s);
		return;
	}

	/* Got a valid interrupt on this device */
	rp = sc->sc_sshp;
	dstat = sc->sc_dstat;
	sstat0 = sc->sc_sstat0;
	if (dstat & SSH_DSTAT_SIR)
		sc->sc_intcode = rp->ssh_dsps;
	sc->sc_istat = 0;

#ifdef DEBUG
	if (ssh_debug & 1)
		printf ("%s: intr istat %x dstat %x sstat0 %x\n",
				  sc->sc_dev.dv_xname, istat, dstat, sstat0);
	if (!sc->sc_active) {
		printf ("%s: spurious interrupt? istat %x dstat %x sstat0 %x status %x\n",
				  sc->sc_dev.dv_xname, istat, dstat, sstat0, sc->sc_nexus->stat[0]);
	}
#else 
	if (!sc->sc_active) {
		printf ("%s: spurious interrupt? istat %x dstat %x sstat0 %x status %x\n",
				  sc->sc_dev.dv_xname, istat, dstat, sstat0, sc->sc_nexus->stat[0]);
		return;
	}
#endif

#ifdef DEBUG
	if (ssh_debug & 5) {
		DCIAS(kvtop(&sc->sc_nexus->stat[0]));
		printf ("%s: intr istat %x dstat %x sstat0 %x dsps %x sbcl %x sts %x msg %x\n",
				  sc->sc_dev.dv_xname, istat, dstat, sstat0,
				  rp->ssh_dsps,  rp->ssh_sbcl,
				  sc->sc_nexus->stat[0], sc->sc_nexus->msg[0]);
	}
#endif
	if (sc->sc_flags & SSH_INTDEFER) {
		sc->sc_flags &= ~(SSH_INTDEFER | SSH_INTSOFF);
		rp->ssh_sien = sc->sc_sien;
		rp->ssh_dien = sc->sc_dien;
	}
	if (ssh_checkintr (sc, istat, dstat, sstat0, &status)) {
#if 1
		if (status == 0xff)
			printf ("sshintr: status == 0xff\n");
#endif
		if ((sc->sc_flags & (SSH_INTSOFF | SSH_INTDEFER)) != SSH_INTSOFF) {
#if 0
			if (rp->ssh_sbcl & SSH_BSY) {
				printf ("%s: SCSI bus busy at completion",
						  sc->sc_dev.dv_xname);
				printf(" targ %d sbcl %02x sfbr %x lcrc %02x dsp +%x\n",
						 sc->sc_nexus->xs->sc_link->target,
						 rp->ssh_sbcl, rp->ssh_sfbr, rp->ssh_lcrc,
						 rp->ssh_dsp - sc->sc_scriptspa);
			}
#endif
			ssh_scsidone(sc->sc_nexus, sc->sc_nexus->stat[0]);
		}
	}
	splx(s);
}

/*
 * This is based on the Progressive Peripherals 33MHz Zeus driver and will
 * not be correct for other 53c710 boards.
 *
 */
void
scsi_period_to_ssh (sc, target)
	struct ssh_softc *sc;
	int target;
{
	int period, offset, sxfer, sbcl;

	period = sc->sc_nexus->msg[4];
	offset = sc->sc_nexus->msg[5];
	for (sbcl = 1; sbcl < 4; ++sbcl) {
		sxfer = (period * 4 - 1) / sc->sc_tcp[sbcl] - 3;
		if (sxfer >= 0 && sxfer <= 7)
			break;
	}
	if (sbcl > 3) {
		printf("ssh_sync: unable to compute sync params for period %dns\n",
				 period * 4);
		/*
		 * XXX need to pick a value we can do and renegotiate
		 */
		sxfer = sbcl = 0;
	} else
		sxfer	= (sxfer << 4) | ((offset <= SSH_MAX_OFFSET) ?
										offset : SSH_MAX_OFFSET);
	sc->sc_sync[target].sxfer = sxfer;
	sc->sc_sync[target].sbcl = sbcl;
#ifdef DEBUG
	printf ("ssh sync: ssh_sxfr %02x, ssh_sbcl %02x\n", sxfer, sbcl);
#endif
}

#ifdef DEBUG

#if SSH_TRACE_SIZE
void
ssh_dump_trace()
{
	int i;

	printf("ssh trace: next index %d\n", ssh_trix);
	i = ssh_trix;
	do {
		printf("%3d: '%c' %02x %02x %02x\n", i, ssh_trbuf[i],
				 ssh_trbuf[i + 1], ssh_trbuf[i + 2], ssh_trbuf[i + 3]);
		i = (i + 4) & (SSH_TRACE_SIZE - 1);
	} while (i != ssh_trix);
}
#endif

void
ssh_dump_acb(acb)
struct ssh_acb *acb;
{
	u_char *b = (u_char *) &acb->cmd;
	int i;

#if SSH_TRACE_SIZE
	ssh_dump_trace();
#endif
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
ssh_dump(sc)
struct ssh_softc *sc;
{
	struct ssh_acb *acb;
	ssh_regmap_p rp = sc->sc_sshp;
	int s;
	int i;

	s = splbio();
	printf("%s@%x regs %x istat %x\n",
			 sc->sc_dev.dv_xname, sc, rp, rp->ssh_istat);
	if (acb = sc->free_list.tqh_first) {
		printf("Free list:\n");
		while (acb) {
			ssh_dump_acb(acb);
			acb = acb->chain.tqe_next;
		}
	}
	if (acb = sc->ready_list.tqh_first) {
		printf("Ready list:\n");
		while (acb) {
			ssh_dump_acb(acb);
			acb = acb->chain.tqe_next;
		}
	}
	if (acb = sc->nexus_list.tqh_first) {
		printf("Nexus list:\n");
		while (acb) {
			ssh_dump_acb(acb);
			acb = acb->chain.tqe_next;
		}
	}
	if (sc->sc_nexus) {
		printf("Nexus:\n");
		ssh_dump_acb(sc->sc_nexus);
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
