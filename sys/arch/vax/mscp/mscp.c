/*	$NetBSD: mscp.c,v 1.5 1997/01/11 11:20:31 ragge Exp $	*/

/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 *	@(#)mscp.c	7.5 (Berkeley) 12/16/90
 */

/*
 * MSCP generic driver routines
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <vax/mscp/mscp.h>
#include <vax/mscp/mscpvar.h>

#define	PCMD	PSWP		/* priority for command packet waits */

/*
 * During transfers, mapping info is saved in the buffer's b_resid.
 */
#define	b_info b_resid

/*
 * Get a command packet.  Second argument is true iff we are
 * to wait if necessary.  Return NULL if none are available and
 * we cannot wait.
 */
struct mscp *
mscp_getcp(mi, canwait)
	register struct mscp_softc *mi;
	int canwait;
{
#define	mri	(&mi->mi_cmd)
	register struct mscp *mp;
	register int i;
	int s = splbio();

again:
	/*
	 * Ensure that we have some command credits, and
	 * that the next command packet is free.
	 */
	if (mi->mi_credits <= MSCP_MINCREDITS) {
		if (!canwait) {
			splx(s);
			return (NULL);
		}
		mi->mi_wantcredits = 1;
		sleep((caddr_t) &mi->mi_wantcredits, PCMD);
		goto again;
	}
	i = mri->mri_next;
	if (mri->mri_desc[i] & MSCP_OWN) {
		if (!canwait) {
			splx(s);
			return (NULL);
		}
		mi->mi_wantcmd = 1;
		sleep((caddr_t) &mi->mi_wantcmd, PCMD);
		goto again;
	}
	mi->mi_credits--;
	mri->mri_desc[i] &= ~MSCP_INT;
	mri->mri_next = (mri->mri_next + 1) % mri->mri_size;
	splx(s);
	mp = &mri->mri_ring[i];

	/*
	 * Initialise some often-zero fields.
	 * ARE THE LAST TWO NECESSARY IN GENERAL?  IT SURE WOULD BE
	 * NICE IF DEC SOLD DOCUMENTATION FOR THEIR OWN CONTROLLERS.
	 */
	mp->mscp_msglen = MSCP_MSGLEN;
	mp->mscp_flags = 0;
	mp->mscp_modifier = 0;
	mp->mscp_seq.seq_bytecount = 0;
	mp->mscp_seq.seq_buffer = 0;
	mp->mscp_seq.seq_mapbase = 0;
/*???*/	mp->mscp_sccc.sccc_errlgfl = 0;
/*???*/	mp->mscp_sccc.sccc_copyspd = 0;
	return (mp);
#undef	mri
}

#ifdef AVOID_EMULEX_BUG
int	mscp_aeb_xor = 0x8000bb80;
#endif

/*
 * Handle a response ring transition.
 */
void
mscp_dorsp(mi)
	register struct mscp_softc *mi;
{
	struct device *drive;
	struct mscp_device *me = mi->mi_me;
	struct mscp_ctlr *mc = mi->mi_mc;
	register struct buf *bp;
	register struct mscp *mp;
	register int nextrsp;
	int st, error, info;
	extern int cold;
	extern struct mscp slavereply;

	nextrsp = mi->mi_rsp.mri_next;
loop:
	if (mi->mi_rsp.mri_desc[nextrsp] & MSCP_OWN) {
		/*
		 * No more responses.  Remember the next expected
		 * response index.  Check to see if we have some
		 * credits back, and wake up sleepers if so.
		 */
		mi->mi_rsp.mri_next = nextrsp;
		if (mi->mi_wantcredits && mi->mi_credits > MSCP_MINCREDITS) {
			mi->mi_wantcredits = 0;
			wakeup((caddr_t) &mi->mi_wantcredits);
		}
		return;
	}

	mp = &mi->mi_rsp.mri_ring[nextrsp];
	mi->mi_credits += MSCP_CREDITS(mp->mscp_msgtc);
	/*
	 * Controllers are allowed to interrupt as any drive, so we
	 * must check the command before checking for a drive.
	 */
	if (mp->mscp_opcode == (M_OP_SETCTLRC | M_OP_END)) {
		if ((mp->mscp_status & M_ST_MASK) == M_ST_SUCCESS) {
			mi->mi_flags |= MSC_READY;
		} else {
			printf("%s: SETCTLRC failed: %d ",
			    mi->mi_dev.dv_xname, mp->mscp_status);
			mscp_printevent(mp);
		}
		goto done;
	}

	/*
	 * Found a response.  Update credit information.  If there is
	 * nothing else to do, jump to `done' to get the next response.
	 */
	if (mp->mscp_unit >= mi->mi_driveno) { /* Must expand drive table */
		int tmpno = ((mp->mscp_unit + 32) & 0xffe0) * sizeof(void *);
		struct device **tmp = (struct device **)
		    malloc(tmpno, M_DEVBUF, M_NOWAIT);
		bzero(tmp, tmpno);
		if (mi->mi_driveno) {
			bcopy(mi->mi_dp, tmp, mi->mi_driveno);
			free(mi->mi_dp, mi->mi_driveno);
		}
		mi->mi_driveno = tmpno;
		mi->mi_dp = tmp;
	}

	drive = mi->mi_dp[mp->mscp_unit];

	switch (MSCP_MSGTYPE(mp->mscp_msgtc)) {

	case MSCPT_SEQ:
		break;

	case MSCPT_DATAGRAM:
		(*me->me_dgram)(drive, mp, mi);
		goto done;

	case MSCPT_CREDITS:
		goto done;

	case MSCPT_MAINTENANCE:
	default:
		printf("%s: unit %d: unknown message type 0x%x ignored\n",
			mi->mi_dev.dv_xname, mp->mscp_unit,
			MSCP_MSGTYPE(mp->mscp_msgtc));
		goto done;
	}

	/*
	 * Handle individual responses.
	 */
	st = mp->mscp_status & M_ST_MASK;
	error = 0;
	switch (mp->mscp_opcode) {

	case M_OP_END:
		/*
		 * The controller presents a bogus END packet when
		 * a read/write command is given with an illegal
		 * block number.  This is contrary to the MSCP
		 * specification (ENDs are to be given only for
		 * invalid commands), but that is the way of it.
		 */
		if (st == M_ST_INVALCMD && mp->mscp_cmdref != 0) {
			printf("%s: bad lbn (%d)?\n", drive->dv_xname,
				(int)mp->mscp_seq.seq_lbn);
			error = EIO;
			goto rwend;
		}
		goto unknown;

	case M_OP_ONLINE | M_OP_END:
		/*
		 * Finished an ON LINE request.  Call the driver to
		 * find out whether it succeeded.  If so, mark it on
		 * line.
		 */
		(*me->me_online)(drive, mp);
		break;

	case M_OP_GETUNITST | M_OP_END:
		/*
		 * Got unit status.  If we are autoconfiguring, save
		 * the mscp struct so that mscp_attach know what to do.
		 * If the drive isn't configured, call config_found()
		 * to set it up, otherwise it's just a "normal" unit
		 * status.
		 */
		if (cold)
			bcopy(mp, &slavereply, sizeof(struct mscp));

		if (mp->mscp_status == (M_ST_OFFLINE|M_OFFLINE_UNKNOWN))
			break;

		if (drive == 0) {
			struct	drive_attach_args da;

			da.da_mp = (struct mscp *)mp;
			da.da_typ = mi->mi_type;
			config_found(&mi->mi_dev, (void *)&da, mscp_print);
		} else
			/* Hack to avoid complaints */
			if (!(((mp->mscp_event & M_ST_MASK) == M_ST_AVAILABLE)
			    && cold))
				(*me->me_gotstatus)(drive, mp);
		break;

	case M_OP_AVAILATTN:
		/*
		 * The drive went offline and we did not notice.
		 * Mark it off line now, to force an on line request
		 * next, so we can make sure it is still the same
		 * drive.
		 *
		 * IF THE UDA DRIVER HAS A COMMAND AWAITING UNIBUS
		 * RESOURCES, THAT COMMAND MAY GO OUT BEFORE THE ON
		 * LINE.  IS IT WORTH FIXING??
		 */
#ifdef notyet
		(*md->md_offline)(ui, mp);
#endif
		break;

	case M_OP_POS | M_OP_END:
	case M_OP_WRITM | M_OP_END:
		/*
		 * A non-data transfer operation completed.
		 */
		(*me->me_cmddone)(drive, mp);
		break;

	case M_OP_READ | M_OP_END:
	case M_OP_WRITE | M_OP_END:
		/*
		 * A transfer finished.  Get the buffer, and release its
		 * map registers via ubadone().  If the command finished
		 * with an off line or available status, the drive went
		 * off line (the idiot controller does not tell us until
		 * it comes back *on* line, or until we try to use it).
		 */
#ifdef DIAGNOSTIC
		if (mp->mscp_cmdref == 0) {
			/*
			 * No buffer means there is a bug somewhere!
			 */
			printf("%s: io done, but no buffer?\n",
			    drive->dv_xname);
			mscp_hexdump(mp);
			break;
		}
#endif
rwend:
		bp = (struct buf *) mp->mscp_cmdref;

		/*
		 * Mark any error-due-to-bad-LBN (via `goto rwend').
		 * WHAT STATUS WILL THESE HAVE?  IT SURE WOULD BE NICE
		 * IF DEC SOLD DOCUMENTATION FOR THEIR OWN CONTROLLERS.
		 */
		if (error) {
			bp->b_flags |= B_ERROR;
			bp->b_error = error;
		}
		if (st == M_ST_OFFLINE || st == M_ST_AVAILABLE) {
#ifdef notyet
			(*md->md_offline)(ui, mp);
#endif
		}

		/*
		 * Unlink the transfer from the wait queue.
		 */
		_remque(&bp->b_actf);

		/*
		 * If the transfer has something to do with bad
		 * block forwarding, let the driver handle the
		 * rest.
		 */
		if ((bp->b_flags & B_BAD) != 0 && me->me_bb != NULL) {
			(*me->me_bb)(drive, mp, bp);
			goto out;
		}

		/*
		 * If the transfer failed, give the driver a crack
		 * at fixing things up.
		 */
		if (st != M_ST_SUCCESS) {
			switch ((*me->me_ioerr)(drive, mp, bp)) {

			case MSCP_DONE:		/* fixed */
				break;

			case MSCP_RESTARTED:	/* still working on it */
				goto out;

			case MSCP_FAILED:	/* no luck */
				/* XXX must move to ra.c */
				mscp_printevent(mp);
				break;
			}
		}

		/*
		 * Set the residual count and mark the transfer as
		 * done.  If the I/O wait queue is now empty, release
		 * the shared BDP, if any.
		 */
		info = bp->b_info;	/* we are about to clobber it */
		bp->b_resid = bp->b_bcount - mp->mscp_seq.seq_bytecount;

		(*mc->mc_ctlrdone)(mi->mi_dev.dv_parent, info);
		(*me->me_iodone)(drive, bp);
out:
		break;
		
	case M_OP_REPLACE | M_OP_END:
		/*
		 * A replace operation finished.  Just let the driver
		 * handle it (if it does replaces).
		 */
		if (me->me_replace == NULL)
			printf("%s: bogus REPLACE end\n", drive->dv_xname);
		else
			(*me->me_replace)(drive, mp);
		break;

	default:
		/*
		 * If it is not one of the above, we cannot handle it.
		 * (And we should not have received it, for that matter.)
		 */
unknown:
		printf("%s: unknown opcode 0x%x status 0x%x ignored\n",
			drive->dv_xname, mp->mscp_opcode, mp->mscp_status);
#ifdef DIAGNOSTIC
		mscp_hexdump(mp);
#endif
		break;
	}

	/*
	 * If the drive needs to be put back in the controller queue,
	 * do that now.  (`bp' below ought to be `dp', but they are all
	 * struct buf *.)  Note that b_active was cleared in the driver;
	 * we presume that there is something to be done, hence reassert it.
	 */
#ifdef notyet /* XXX */
	if (ui->ui_flags & UNIT_REQUEUE) {
		bp = &md->md_utab[ui->ui_unit];
		if (bp->b_active) panic("mscp_dorsp requeue");
		MSCP_APPEND(bp, mi->mi_XXXtab, b_hash.le_next);
/* Was:		MSCP_APPEND(bp, mi->mi_XXXtab, b_forw); */
		bp->b_active = 1;
		ui->ui_flags &= ~UNIT_REQUEUE;
	}
#endif
done:
	/*
	 * Give back the response packet, and take a look at the next.
	 */
	mp->mscp_msglen = MSCP_MSGLEN;
	mi->mi_rsp.mri_desc[nextrsp] |= MSCP_OWN;
	nextrsp = (nextrsp + 1) % mi->mi_rsp.mri_size;
	goto loop;
}

/*
 * Requeue outstanding transfers, e.g., after bus reset.
 * Also requeue any drives that have on line or unit status
 * info pending.
 */
void
mscp_requeue(mi)
	struct mscp_softc *mi;
{
	register struct mscp_device *me = mi->mi_me;
	register struct buf *bp, *dp;
	register int unit;
	struct buf *nextbp;

panic("mscp_requeue");
	/*
	 * Clear the controller chain.  Mark everything un-busy; we
	 * will soon fix any that are in fact busy.
	 */
#ifdef notyet /* XXX */
	mi->mi_XXXtab->b_actf = NULL;
	mi->mi_XXXtab->b_active = 0;
	for (unit = 0, dp = md->md_utab; unit < md->md_nunits; unit++, dp++) {
		ui = md->md_dinfo[unit];
		if (ui == NULL || !ui->ui_alive || ui->ui_ctlr != mi->mi_ctlr)
			continue;	/* not ours */
		dp->b_hash.le_next = NULL;
		dp->b_active = 0;
	}
	/*
	 * Scan the wait queue, linking buffers onto drive queues.
	 * Note that these must be put at the front of the drive queue,
	 * lest we reorder I/O operations.
	 */
	for (bp = *mi->mi_XXXwtab.b_actb; bp != &mi->mi_XXXwtab; bp = nextbp) {
		nextbp = *bp->b_actb;
		dp = &md->md_utab[minor(bp->b_dev) >> md->md_unitshift];
		bp->b_actf = dp->b_actf;
		if (dp->b_actf == NULL)
			dp->b_actb = (void *)bp;
		dp->b_actf = bp;
	}
	mi->mi_XXXwtab.b_actf = *mi->mi_XXXwtab.b_actb = &mi->mi_XXXwtab;

	/*
	 * Scan for drives waiting for on line or status responses,
	 * and for drives with pending transfers.  Put these on the
	 * controller queue, and mark the controller busy.
	 */
	for (unit = 0, dp = md->md_utab; unit < md->md_nunits; unit++, dp++) {
		ui = md->md_dinfo[unit];
		if (ui == NULL || !ui->ui_alive || ui->ui_ctlr != mi->mi_ctlr)
			continue;
		ui->ui_flags &= ~(UNIT_HAVESTATUS | UNIT_ONLINE);
		if ((ui->ui_flags & UNIT_REQUEUE) == 0 && dp->b_actf == NULL)
			continue;
		ui->ui_flags &= ~UNIT_REQUEUE;
		MSCP_APPEND(dp, mi->mi_XXXtab, b_hash.le_next);

		dp->b_active = 1;
		mi->mi_XXXtab->b_active = 1;
	}

#endif
#ifdef AVOID_EMULEX_BUG
	/*
	 * ... and clear the index-to-buffer table.
	 */
	for (unit = 0; unit < AEB_MAX_BP; unit++)
		mi->mi_bp[unit] = 0;
#endif
}

