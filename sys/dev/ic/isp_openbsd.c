/* 	$OpenBSD: isp_openbsd.c,v 1.44 2010/06/28 18:31:02 krw Exp $ */
/*
 * Platform (OpenBSD) dependent common attachment code for QLogic adapters.
 *
 * Copyright (c) 1999, 2000, 2001 by Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 *    documentation and/or other materials provided with the distribution.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The author may be reached via electronic communications at
 *
 *  mjacob@feral.com
 *
 * or, via United States Postal Address
 *
 *  Matthew Jacob
 *  Feral Software
 *  PMB#825
 *  5214-F Diamond Heights Blvd.
 *  San Francisco, CA, 94131
 */

#include <dev/ic/isp_openbsd.h>

/*
 * Set a timeout for the watchdogging of a command.
 *
 * The dimensional analysis is
 *
 *	milliseconds * (seconds/millisecond) * (ticks/second) = ticks
 *
 *			=
 *
 *	(milliseconds / 1000) * hz = ticks
 *
 *
 * For timeouts less than 1 second, we'll get zero. Because of this, and
 * because we want to establish *our* timeout to be longer than what the
 * firmware might do, we just add 3 seconds at the back end.
 */
#define	_XT(xs)	((((xs)->timeout/1000) * hz) + (3 * hz))

static void ispminphys(struct buf *, struct scsi_link *);
static void ispcmd_slow(XS_T *);
static void ispcmd(XS_T *);

void isp_polled_cmd (struct ispsoftc *, XS_T *);
void isp_wdog (void *);
void isp_make_here(ispsoftc_t *, int);
void isp_make_gone(ispsoftc_t *, int);
void isp_requeue(void *);
void isp_trestart(void *);
void isp_restart(struct ispsoftc *);
void isp_add2_blocked_queue(struct ispsoftc *isp, XS_T *xs);
int isp_mstohz(int ms);

struct cfdriver isp_cd = {
	NULL, "isp", DV_DULL
};

static const char *roles[4] = {
    "(none)", "Target", "Initiator", "Target/Initiator"
};
static const char prom3[] =
    "PortID 0x%06x Departed from Target %u because of %s";
#define	isp_change_is_bad	0

/*
 * Complete attachment of hardware, include subdevices.
 */
void
isp_attach(struct ispsoftc *isp)
{
	struct scsibus_attach_args saa;
	struct scsi_link *lptr = &isp->isp_osinfo._link[0];
	isp->isp_osinfo._adapter.scsi_minphys = ispminphys;

	isp->isp_state = ISP_RUNSTATE;

	/*
	 * We only manage a single wait queues for dual bus controllers.
	 * This is arguably broken.
	 */
	isp->isp_osinfo.wqf = isp->isp_osinfo.wqt = NULL;

	lptr->adapter_softc = isp;
	lptr->adapter = &isp->isp_osinfo._adapter;
	lptr->openings = imin(isp->isp_maxcmds, MAXISPREQUEST(isp));
	isp->isp_osinfo._adapter.scsi_cmd = ispcmd_slow;
	if (IS_FC(isp)) {
		lptr->adapter_buswidth = MAX_FC_TARG;
		lptr->adapter_target = MAX_FC_TARG; /* i.e. ignore. */
		lptr->node_wwn = ISP_NODEWWN(isp);
		lptr->port_wwn = ISP_PORTWWN(isp);
	} else {
		sdparam *sdp = isp->isp_param;
		lptr->adapter_buswidth = MAX_TARGETS;
		/* We can set max lun width here */
		lptr->adapter_target = sdp->isp_initiator_id;
		isp->isp_osinfo.discovered[0] = 1 << sdp->isp_initiator_id;
		if (IS_DUALBUS(isp)) {
			struct scsi_link *lptrb = &isp->isp_osinfo._link[1];
			lptrb->adapter_softc = isp;
			lptrb->adapter = &isp->isp_osinfo._adapter;
			lptrb->openings = lptr->openings;
			lptrb->adapter_buswidth = MAX_TARGETS;
			lptrb->adapter_target = sdp->isp_initiator_id;
			lptrb->flags = SDEV_2NDBUS;
			isp->isp_osinfo.discovered[1] =
			    1 << (sdp+1)->isp_initiator_id;
		}
	}

	/*
	 * Send a SCSI Bus Reset (used to be done as part of attach,
	 * but now left to the OS outer layers).
	 *
	 * We don't do 'bus resets' for FC because the LIP that occurs
	 * when we start the firmware does all that for us.
	 */
	if (IS_SCSI(isp)) {
		int bus = 0;
		ISP_LOCK(isp);
		(void) isp_control(isp, ISPCTL_RESET_BUS, &bus);
		if (IS_DUALBUS(isp)) {
			bus++;
			(void) isp_control(isp, ISPCTL_RESET_BUS, &bus);
		}
		ISP_UNLOCK(isp);
		/*
		 * wait for the bus to settle.
		 */
		delay(4 * 1000000);
	} else {
		delay(2 * 1000000);
		ISP_LOCK(isp);
		isp_fc_runstate(isp, 10 * 1000000);
		ISP_UNLOCK(isp);
	}

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = lptr;

	/*
	 * And attach children (if any).
	 */
	config_found((void *)isp, &saa, scsiprint);
	if (IS_DUALBUS(isp)) {
		lptr++;
		bzero(&saa, sizeof(saa));
		saa.saa_sc_link = lptr;
		config_found((void *)isp, &saa, scsiprint);
	}
}

/*
 * minphys our xfers
 *
 * Unfortunately, the buffer pointer describes the target device- not the
 * adapter device, so we can't use the pointer to find out what kind of
 * adapter we are and adjust accordingly.
 */

static void
ispminphys(struct buf *bp, struct scsi_link *sl)
{
	/*
	 * XX: Only the 1020 has a 24 bit limit.
	 */
	if (bp->b_bcount >= (1 << 24)) {
		bp->b_bcount = (1 << 24);
	}
	minphys(bp);
}

void
ispcmd_slow(XS_T *xs)
{
	sdparam *sdp;
	int tgt, chan;
	u_int16_t f;
	struct ispsoftc *isp = XS_ISP(xs);

	if (IS_FC(isp)) {
		if (cold == 0) {
			isp->isp_osinfo.no_mbox_ints = 0;
			isp->isp_osinfo._adapter.scsi_cmd = ispcmd;
		}
		return (ispcmd(xs));
	}

	/*
	 * Have we completed discovery for this target on this adapter?
	 */
	sdp = isp->isp_param;
	tgt = XS_TGT(xs);
	chan = XS_CHANNEL(xs);
	sdp += chan;
	if ((xs->flags & SCSI_POLL) != 0 ||
	    (isp->isp_osinfo.discovered[chan] & (1 << tgt)) != 0) {
		return (ispcmd(xs));
	}
	if (cold == 0) {
		isp->isp_osinfo.no_mbox_ints = 0;
	}

	f = DPARM_DEFAULT;
	if (xs->sc_link->quirks & SDEV_NOSYNC) {
		f &= ~DPARM_SYNC;
	}
	if (xs->sc_link->quirks & SDEV_NOWIDE) {
		f &= ~DPARM_WIDE;
	}
	if (xs->sc_link->quirks & SDEV_NOTAGS) {
		f &= ~DPARM_TQING;
	}

	/*
	 * Okay, we know about this device now,
	 * so mark parameters to be updated for it.
	 */
	sdp->isp_devparam[tgt].goal_flags = f;
	sdp->isp_devparam[tgt].dev_update = 1;
	isp->isp_update |= (1 << chan);

	/*
	 * Now check to see whether we can get out of this checking mode now.
	 * XXX: WE CANNOT AS YET BECAUSE THERE IS NO MECHANISM TO TELL US
	 * XXX: WHEN WE'RE DONE DISCOVERY BECAUSE WE NEED ONE COMMAND AFTER
	 * XXX: DISCOVERY IS DONE FOR EACH TARGET TO TELL US THAT WE'RE DONE
	 * XXX: AND THAT DOESN'T HAPPEN HERE. AT BEST WE CAN MARK OURSELVES
	 * XXX: DONE WITH DISCOVERY FOR THIS TARGET AND SO SAVE MAYBE 20
	 * XXX: LINES OF C CODE.
	 */
	isp->isp_osinfo.discovered[chan] |= (1 << tgt);
	/* do not bother with these lines- they'll never execute correctly */
#if	0
	sdp = isp->isp_param;
	for (chan = 0; chan < (IS_12X0(isp)? 2 : 1); chan++, sdp++) {
		f = 0xffff & ~(1 << sdp->isp_initiator_id);
		if (isp->isp_osinfo.discovered[chan] != f) {
			break;
		}
	}
	if (chan == (IS_12X0(isp)? 2 : 1)) {
		isp->isp_osinfo._adapter.scsipi_cmd = ispcmd;
		if (IS_12X0(isp))
			isp->isp_update |= 2;
	}
#endif
	return (ispcmd(xs));
}

void
isp_add2_blocked_queue(struct ispsoftc *isp, XS_T *xs)
{
	if (isp->isp_osinfo.wqf != NULL) {
		isp->isp_osinfo.wqt->free_list.le_next = xs;
	} else {
		isp->isp_osinfo.wqf = xs;
	}
	isp->isp_osinfo.wqt = xs;
	xs->free_list.le_next = NULL;
}

void
ispcmd(XS_T *xs)
{
	struct ispsoftc *isp;
	int result;

	/*
	 * Make sure that there's *some* kind of sane setting.
	 */
	isp = XS_ISP(xs);

	timeout_set(&xs->stimeout, isp_wdog, xs);

	ISP_LOCK(isp);

	if (XS_LUN(xs) >= isp->isp_maxluns) {
		xs->error = XS_SELTIMEOUT;
		scsi_done(xs);
		ISP_UNLOCK(isp);
		return;
	}

	if (isp->isp_state < ISP_RUNSTATE) {
		ISP_DISABLE_INTS(isp);
		isp_init(isp);
		if (isp->isp_state != ISP_INITSTATE) {
			ISP_ENABLE_INTS(isp);
			XS_SETERR(xs, HBA_BOTCH);
			scsi_done(xs);
			ISP_UNLOCK(isp);
			return;
		}
		isp->isp_state = ISP_RUNSTATE;
		ISP_ENABLE_INTS(isp);
	}

	/*
	 * Check for queue blockage...
	 */
	if (isp->isp_osinfo.blocked) {
		if (xs->flags & SCSI_POLL) {
			xs->error = XS_NO_CCB;
			scsi_done(xs);
			ISP_UNLOCK(isp);
			return;
		}
		if (isp->isp_osinfo.blocked == 2) {
			isp_restart(isp);
		}
		if (isp->isp_osinfo.blocked) {
			isp_add2_blocked_queue(isp, xs);
			ISP_UNLOCK(isp);
			isp_prt(isp, ISP_LOGDEBUG0, "added to blocked queue");
			return;
		}
	}

	if (xs->flags & SCSI_POLL) {
		volatile u_int8_t ombi = isp->isp_osinfo.no_mbox_ints;
		isp->isp_osinfo.no_mbox_ints = 1;
		isp_polled_cmd(isp, xs);
		isp->isp_osinfo.no_mbox_ints = ombi;
		ISP_UNLOCK(isp);
		return;
	}

	result = isp_start(xs);

	switch (result) {
	case CMD_QUEUED:
		if (xs->timeout) {
			timeout_add(&xs->stimeout, _XT(xs));
			XS_CMD_S_TIMER(xs);
		}
		if (isp->isp_osinfo.hiwater < isp->isp_nactive) {
			isp->isp_osinfo.hiwater = isp->isp_nactive;
			isp_prt(isp, ISP_LOGDEBUG0,
			    "Active Hiwater Mark=%d", isp->isp_nactive);
		}
		break;
	case CMD_EAGAIN:
		isp->isp_osinfo.blocked |= 2;
		isp_prt(isp, ISP_LOGDEBUG0, "blocking queue");
		isp_add2_blocked_queue(isp, xs);
		break;
	case CMD_RQLATER:
		isp_prt(isp, ISP_LOGDEBUG1, "retrying later for %d.%d",
		    XS_TGT(xs), XS_LUN(xs));
		timeout_set(&xs->stimeout, isp_requeue, xs);
		timeout_add_sec(&xs->stimeout, 1);
		XS_CMD_S_TIMER(xs);
		break;
	case CMD_COMPLETE:
		break;
	}
	ISP_UNLOCK(isp);
}

void
isp_polled_cmd(struct ispsoftc *isp, XS_T *xs)
{
	int result;
	int infinite = 0, mswait;

	result = isp_start(xs);

	switch (result) {
	case CMD_QUEUED:
		break;
	case CMD_RQLATER:
	case CMD_EAGAIN:
		xs->error = XS_NO_CCB;
		/* FALLTHROUGH */
	case CMD_COMPLETE:
		scsi_done(xs);
		return;
	}

	/*
	 * If we can't use interrupts, poll on completion.
	 */
	if ((mswait = XS_TIME(xs)) == 0)
		infinite = 1;

	while (mswait || infinite) {
		u_int32_t isr;
		u_int16_t sema, mbox;
		if (ISP_READ_ISR(isp, &isr, &sema, &mbox)) {
			isp_intr(isp, isr, sema, mbox);
			if (XS_CMD_DONE_P(xs)) {
				break;
			}
		}
		USEC_DELAY(1000);
		mswait -= 1;
	}

	/*
	 * If no other error occurred but we didn't finish,
	 * something bad happened.
	 */
	if (XS_CMD_DONE_P(xs) == 0) {
		if (isp_control(isp, ISPCTL_ABORT_CMD, xs)) {
			isp_reinit(isp);
		}
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_BOTCH);
		}
	}
}

void
isp_done(XS_T *xs)
{
	XS_CMD_S_DONE(xs);
	if (XS_CMD_WDOG_P(xs) == 0) {
		struct ispsoftc *isp = XS_ISP(xs);
		if (XS_CMD_TIMER_P(xs)) {
			timeout_del(&xs->stimeout);
			XS_CMD_C_TIMER(xs);
		}
		if (XS_CMD_GRACE_P(xs)) {
			struct ispsoftc *isp = XS_ISP(xs);
			isp_prt(isp, ISP_LOGWARN,
			    "finished command on borrowed time");
		}
		XS_CMD_S_CLEAR(xs);
		scsi_done(xs);
		if (isp->isp_osinfo.blocked == 2) {
			isp->isp_osinfo.blocked = 0;
			isp_prt(isp, ISP_LOGDEBUG0, "restarting blocked queue");
			isp_restart(isp);
		}
	}
}

void
isp_wdog(void *arg)
{
	XS_T *xs = arg;
	struct ispsoftc *isp = XS_ISP(xs);
	u_int32_t handle;

	/*
	 * We've decided this command is dead. Make sure we're not trying
	 * to kill a command that's already dead by getting its handle and
	 * and seeing whether it's still alive.
	 */
	ISP_LOCK(isp);
	handle = isp_find_handle(isp, xs);
	if (handle) {
		u_int32_t isr;
		u_int16_t sema, mbox;

		if (XS_CMD_DONE_P(xs)) {
			isp_prt(isp, ISP_LOGDEBUG1,
			    "watchdog found done cmd (handle 0x%x)",
			    handle);
			ISP_UNLOCK(isp);
			return;
		}

		if (XS_CMD_WDOG_P(xs)) {
			isp_prt(isp, ISP_LOGDEBUG1,
			    "recursive watchdog (handle 0x%x)",
			    handle);
			ISP_UNLOCK(isp);
			return;
		}

		XS_CMD_S_WDOG(xs);

		if (ISP_READ_ISR(isp, &isr, &sema, &mbox)) {
			isp_intr(isp, isr, sema, mbox);
		}

		if (XS_CMD_DONE_P(xs)) {
			isp_prt(isp, ISP_LOGINFO,
			    "watchdog cleanup for handle 0x%x", handle);
			XS_CMD_C_WDOG(xs);
			isp_done(xs);
		} else if (XS_CMD_GRACE_P(xs)) {
			/*
			 * Make sure the command is *really* dead before we
			 * release the handle (and DMA resources) for reuse.
			 */
			(void) isp_control(isp, ISPCTL_ABORT_CMD, arg);

			/*
			 * After this point, the command is really dead.
			 */
			if (XS_XFRLEN(xs)) {
				ISP_DMAFREE(isp, xs, handle);
			}
			isp_prt(isp, ISP_LOGWARN,
			    "watchdog timeout on handle %x", handle);
			isp_destroy_handle(isp, handle);
			XS_SETERR(xs, XS_TIMEOUT);
			XS_CMD_S_CLEAR(xs);
			isp_done(xs);
		} else {
			u_int32_t optr, nxti;
			ispreq_t local, *mp = &local, *qe;

			isp_prt(isp, ISP_LOGWARN,
			    "possible command timeout on handle %x", handle);

			XS_CMD_C_WDOG(xs);
			timeout_add(&xs->stimeout, _XT(xs));
			XS_CMD_S_TIMER(xs);
			if (isp_getrqentry(isp, &nxti, &optr, (void **) &qe)) {
				ISP_UNLOCK(isp);
				return;
			}
			XS_CMD_S_GRACE(xs);
			MEMZERO((void *) mp, sizeof (*mp));
			mp->req_header.rqs_entry_count = 1;
			mp->req_header.rqs_entry_type = RQSTYPE_MARKER;
			mp->req_cdblen = SYNC_ALL;
			mp->req_target = XS_CHANNEL(xs) << 7;
			isp_put_request(isp, mp, qe);
			ISP_ADD_REQUEST(isp, nxti);
		}
	} else if (isp->isp_dblev) {
		isp_prt(isp, ISP_LOGDEBUG1, "watchdog with no command");
	}
	ISP_UNLOCK(isp);
}

void
isp_make_here(ispsoftc_t *isp, int tgt)
{
	isp_prt(isp, ISP_LOGINFO, "target %d has arrived", tgt);
}

void
isp_make_gone(ispsoftc_t *isp, int tgt)
{
	isp_prt(isp, ISP_LOGINFO, "target %d has departed", tgt);
}

/*
 * Free any associated resources prior to decommissioning and
 * set the card to a known state (so it doesn't wake up and kick
 * us when we aren't expecting it to).
 *
 * Locks are held before coming here.
 */
void
isp_uninit(struct ispsoftc *isp)
{
	ISP_LOCK(isp);
	/*
	 * Leave with interrupts disabled.
	 */
	ISP_DISABLE_INTS(isp);

	ISP_UNLOCK(isp);
}

/*
 * Restart function for a command to be requeued later.
 */
void
isp_requeue(void *arg)
{
	int r;
	struct scsi_xfer *xs = arg;
	struct ispsoftc *isp = XS_ISP(xs);
	ISP_LOCK(isp);
	r = isp_start(xs);
	switch (r) {
	case CMD_QUEUED:
		isp_prt(isp, ISP_LOGDEBUG1, "restarted command for %d.%d",
		    XS_TGT(xs), XS_LUN(xs));
		if (xs->timeout) {
			timeout_set(&xs->stimeout, isp_wdog, xs);
			timeout_add(&xs->stimeout, _XT(xs));
			XS_CMD_S_TIMER(xs);
		}
		break;
	case CMD_EAGAIN:
		isp_prt(isp, ISP_LOGDEBUG0, "blocked cmd again");
		isp->isp_osinfo.blocked |= 2;
		isp_add2_blocked_queue(isp, xs);
		break;
	case CMD_RQLATER:
		isp_prt(isp, ISP_LOGDEBUG0, "%s for %d.%d",
		    (r == CMD_EAGAIN)? "CMD_EAGAIN" : "CMD_RQLATER",
		    XS_TGT(xs), XS_LUN(xs));
		timeout_set(&xs->stimeout, isp_requeue, xs);
		timeout_add_sec(&xs->stimeout, 1);
		XS_CMD_S_TIMER(xs);
		break;
	case CMD_COMPLETE:
		/* can only be an error */
		if (XS_NOERR(xs))
			XS_SETERR(xs, XS_DRIVER_STUFFUP);
		isp_done(xs);
		break;
	}
	ISP_UNLOCK(isp);
}

/*
 * Restart function after a LOOP UP event or a command completing,
 * sometimes done as a timeout for some hysteresis.
 */
void
isp_trestart(void *arg)
{
	struct ispsoftc *isp = arg;
	struct scsi_xfer *list;

	ISP_LOCK(isp);
	isp->isp_osinfo.rtpend = 0;
	list = isp->isp_osinfo.wqf;
	if (isp->isp_osinfo.blocked == 0 && list != NULL) {
		int nrestarted = 0;

		isp->isp_osinfo.wqf = NULL;
		ISP_UNLOCK(isp);
		do {
			struct scsi_xfer *xs = list;
			list = xs->free_list.le_next;
			xs->free_list.le_next = NULL;
			isp_requeue(xs);
			if (isp->isp_osinfo.wqf == NULL)
				nrestarted++;
		} while (list != NULL);
		isp_prt(isp, ISP_LOGDEBUG0, "requeued %d commands", nrestarted);
	} else {
		ISP_UNLOCK(isp);
	}
}

void
isp_restart(struct ispsoftc *isp)
{
	struct scsi_xfer *list;

	list = isp->isp_osinfo.wqf;
	if (isp->isp_osinfo.blocked == 0 && list != NULL) {
		int nrestarted = 0;

		isp->isp_osinfo.wqf = NULL;
		do {
			struct scsi_xfer *xs = list;
			list = xs->free_list.le_next;
			xs->free_list.le_next = NULL;
			isp_requeue(xs);
			if (isp->isp_osinfo.wqf == NULL)
				nrestarted++;
		} while (list != NULL);
		isp_prt(isp, ISP_LOGDEBUG0, "requeued %d commands", nrestarted);
	}
}

int
isp_async(struct ispsoftc *isp, ispasync_t cmd, void *arg)
{
	int bus, tgt;
	char *msg = NULL;
	static const char prom[] =
	    "PortID 0x%06x handle 0x%x role %s %s\n"
	    "      WWNN 0x%08x%08x WWPN 0x%08x%08x";
	static const char prom2[] =
	    "PortID 0x%06x handle 0x%x role %s %s tgt %u\n"
	    "      WWNN 0x%08x%08x WWPN 0x%08x%08x";
	fcportdb_t *lp;

	switch (cmd) {
	case ISPASYNC_NEW_TGT_PARAMS:
	if (IS_SCSI(isp) && isp->isp_dblev) {
		sdparam *sdp = isp->isp_param;
		char *wt;
		int mhz, flags, period;

		tgt = *((int *) arg);
		bus = (tgt >> 16) & 0xffff;
		tgt &= 0xffff;
		sdp += bus;
		flags = sdp->isp_devparam[tgt].actv_flags;
		period = sdp->isp_devparam[tgt].actv_period;

		if ((flags & DPARM_SYNC) && period &&
		    (sdp->isp_devparam[tgt].actv_offset) != 0) {
			/*
			 * There's some ambiguity about our negotiated speed
			 * if we haven't detected LVD mode correctly (which
			 * seems to happen, unfortunately). If we're in LVD
			 * mode, then different rules apply about speed.
			 */
			if (sdp->isp_lvdmode || period < 0xc) {
				switch (period) {
				case 0x9:
					mhz = 80;
					break;
				case 0xa:
					mhz = 40;
					break;
				case 0xb:
					mhz = 33;
					break;
				case 0xc:
					mhz = 25;
					break;
				default:
					mhz = 1000 / (period * 4);
					break;
				}
			} else {
				mhz = 1000 / (period * 4);
			}
		} else {
			mhz = 0;
		}
		switch (flags & (DPARM_WIDE|DPARM_TQING)) {
		case DPARM_WIDE:
			wt = ", 16 bit wide";
			break;
		case DPARM_TQING:
			wt = ", Tagged Queueing Enabled";
			break;
		case DPARM_WIDE|DPARM_TQING:
			wt = ", 16 bit wide, Tagged Queueing Enabled";
			break;
		default:
			wt = " ";
			break;
		}
		if (mhz) {
			isp_prt(isp, ISP_LOGINFO,
			    "Bus %d Target %d at %dMHz Max Offset %d%s",
			    bus, tgt, mhz, sdp->isp_devparam[tgt].actv_offset,
			    wt);
		} else {
			isp_prt(isp, ISP_LOGINFO,
			    "Bus %d Target %d Async Mode%s", bus, tgt, wt);
		}
		break;
	}
	case ISPASYNC_BUS_RESET:
		if (arg)
			bus = *((int *) arg);
		else
			bus = 0;
		isp_prt(isp, ISP_LOGINFO, "SCSI bus %d reset detected", bus);
		break;
	case ISPASYNC_LOOP_DOWN:
		/*
		 * Hopefully we get here in time to minimize the number
		 * of commands we are firing off that are sure to die.
		 */
		isp->isp_osinfo.blocked |= 1;
		isp_prt(isp, ISP_LOGINFO, "Loop DOWN");
		break;
        case ISPASYNC_LOOP_UP:
		isp->isp_osinfo.blocked &= ~1;
		if (isp->isp_osinfo.rtpend == 0) {
			timeout_set(&isp->isp_osinfo.rqt, isp_trestart, isp);
			isp->isp_osinfo.rtpend = 1;
		}
		timeout_add(&isp->isp_osinfo.rqt, 1);
		isp_prt(isp, ISP_LOGINFO, "Loop UP");
		break;
	case ISPASYNC_DEV_ARRIVED:
		lp = arg;
		lp->reserved = 0;
		if ((isp->isp_role & ISP_ROLE_INITIATOR) &&
		    (lp->roles & (SVC3_TGT_ROLE >> SVC3_ROLE_SHIFT))) {
			int dbidx = lp - FCPARAM(isp)->portdb;
			int i;

			for (i = 0; i < MAX_FC_TARG; i++) {
				if (i >= FL_ID && i <= SNS_ID) {
					continue;
				}
				if (FCPARAM(isp)->isp_ini_map[i] == 0) {
					break;
				}
			}
			if (i < MAX_FC_TARG) {
				FCPARAM(isp)->isp_ini_map[i] = dbidx + 1;
				lp->ini_map_idx = i + 1;
			} else {
				isp_prt(isp, ISP_LOGWARN, "out of target ids");
				isp_dump_portdb(isp);
			}
		}
		if (lp->ini_map_idx) {
			tgt = lp->ini_map_idx - 1;
			isp_prt(isp, ISP_LOGCONFIG, prom2,
			    lp->portid, lp->handle,
		            roles[lp->roles], "arrived at", tgt,
		    	    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
			isp_make_here(isp, tgt);
		} else {
			isp_prt(isp, ISP_LOGCONFIG, prom,
			    lp->portid, lp->handle,
		            roles[lp->roles], "arrived",
		    	    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		}
		break;
	case ISPASYNC_DEV_CHANGED:
		lp = arg;
		if (isp_change_is_bad) {
			lp->state = FC_PORTDB_STATE_NIL;
			if (lp->ini_map_idx) {
				tgt = lp->ini_map_idx - 1;
				FCPARAM(isp)->isp_ini_map[tgt] = 0;
				lp->ini_map_idx = 0;
				isp_prt(isp, ISP_LOGCONFIG, prom3,
				    lp->portid, tgt, "change is bad");
				isp_make_gone(isp, tgt);
			} else {
				isp_prt(isp, ISP_LOGCONFIG, prom,
				    lp->portid, lp->handle,
				    roles[lp->roles],
				    "changed and departed",
				    (uint32_t) (lp->node_wwn >> 32),
				    (uint32_t) lp->node_wwn,
				    (uint32_t) (lp->port_wwn >> 32),
				    (uint32_t) lp->port_wwn);
			}
		} else {
			lp->portid = lp->new_portid;
			lp->roles = lp->new_roles;
			if (lp->ini_map_idx) {
				int t = lp->ini_map_idx - 1;
				FCPARAM(isp)->isp_ini_map[t] =
				    (lp - FCPARAM(isp)->portdb) + 1;
				tgt = lp->ini_map_idx - 1;
				isp_prt(isp, ISP_LOGCONFIG, prom2,
				    lp->portid, lp->handle,
				    roles[lp->roles], "changed at", tgt,
				    (uint32_t) (lp->node_wwn >> 32),
				    (uint32_t) lp->node_wwn,
				    (uint32_t) (lp->port_wwn >> 32),
				    (uint32_t) lp->port_wwn);
			} else {
				isp_prt(isp, ISP_LOGCONFIG, prom,
				    lp->portid, lp->handle,
				    roles[lp->roles], "changed",
				    (uint32_t) (lp->node_wwn >> 32),
				    (uint32_t) lp->node_wwn,
				    (uint32_t) (lp->port_wwn >> 32),
				    (uint32_t) lp->port_wwn);
			}
		}
		break;
	case ISPASYNC_DEV_STAYED:
		lp = arg;
		if (lp->ini_map_idx) {
			tgt = lp->ini_map_idx - 1;
			isp_prt(isp, ISP_LOGCONFIG, prom2,
			    lp->portid, lp->handle,
		    	    roles[lp->roles], "stayed at", tgt,
			    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		} else {
			isp_prt(isp, ISP_LOGCONFIG, prom,
			    lp->portid, lp->handle,
		    	    roles[lp->roles], "stayed",
			    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		}
		break;
	case ISPASYNC_DEV_GONE:
		lp = arg;
		if (lp->ini_map_idx && lp->reserved == 0) {
			lp->reserved = 1;
			lp->state = FC_PORTDB_STATE_ZOMBIE;
			tgt = lp->ini_map_idx - 1;
			isp_prt(isp, ISP_LOGCONFIG, prom2,
			    lp->portid, lp->handle,
		            roles[lp->roles], "gone zombie at", tgt,
		    	    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		} else if (lp->reserved == 0) {
			isp_prt(isp, ISP_LOGCONFIG, prom,
			    lp->portid, lp->handle,
			    roles[lp->roles], "departed",
			    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
			    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		}
		break;
	case ISPASYNC_CHANGE_NOTIFY:
		if (arg == ISPASYNC_CHANGE_PDB) {
			msg = "Port Database Changed";
		} else if (arg == ISPASYNC_CHANGE_SNS) {
			msg = "Name Server Database Changed";
		} else {
			msg = "Other Change Notify";
		}
		isp_prt(isp, ISP_LOGINFO, msg);
		break;
	case ISPASYNC_FW_CRASH:
	{
		u_int16_t mbox1, mbox6;
		mbox1 = ISP_READ(isp, OUTMAILBOX1);
		if (IS_DUALBUS(isp)) { 
			mbox6 = ISP_READ(isp, OUTMAILBOX6);
		} else {
			mbox6 = 0;
		}
                isp_prt(isp, ISP_LOGERR,
                    "Internal Firmware Error on bus %d @ RISC Address 0x%x",
                    mbox6, mbox1);
#ifdef	ISP_FW_CRASH_DUMP
		if (IS_FC(isp)) {
			isp->isp_osinfo.blocked |= 1;
			isp_fw_dump(isp);
		}
		isp_reinit(isp);
		isp_async(isp, ISPASYNC_FW_RESTART, NULL);
#endif
		break;
	}
	default:
		break;
	}
	return (0);
}

void
isp_prt(struct ispsoftc *isp, int level, const char *fmt, ...)
{
	va_list ap;
	if (level != ISP_LOGALL && (level & isp->isp_dblev) == 0) {
		return;
	}
	printf("%s: ", isp->isp_name);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

int
isp_mbox_acquire(ispsoftc_t *isp)
{
	if (isp->isp_osinfo.mboxbsy) {
		return (1);
	} else {
		isp->isp_osinfo.mboxcmd_done = 0;
		isp->isp_osinfo.mboxbsy = 1;
		return (0);
	}
}

void
isp_lock(struct ispsoftc *isp)
{
	int s = splbio();
	if (isp->isp_osinfo.islocked++ == 0) {
		isp->isp_osinfo.splsaved = s;
	} else {
		splx(s);
	}
}

void
isp_unlock(struct ispsoftc *isp)
{
	if (isp->isp_osinfo.islocked-- <= 1) {
		isp->isp_osinfo.islocked = 0;
		splx(isp->isp_osinfo.splsaved);
	}
}

/*
 * XXX Since the clocks aren't running yet during autoconf, we have to
 * keep track of time ourselves, otherwise we may end up waiting
 * forever for the FC link to go up.
 */
struct timespec isp_nanotime;

void
isp_delay(int usec)
{
	delay(usec);
	isp_nanotime.tv_nsec += (usec * 1000);
	if (isp_nanotime.tv_nsec >= 1000000000L) {
		isp_nanotime.tv_sec++;
		isp_nanotime.tv_nsec -= 1000000000L;
	}
}

u_int64_t
isp_nanotime_sub(struct timespec *b, struct timespec *a)
{
	struct timespec x;
	u_int64_t elapsed;
	timespecsub(b, a, &x);
	elapsed = GET_NANOSEC(&x);
	if (elapsed == 0)
		elapsed++;
	return (elapsed);
}

void
isp_mbox_wait_complete(ispsoftc_t *isp, mbreg_t *mbp)
{
	unsigned int usecs = mbp->timeout;
	unsigned int max, olim, ilim;

	if (usecs == 0) {
		usecs = MBCMD_DEFAULT_TIMEOUT;
	}
	max = isp->isp_mbxwrk0 + 1;

	if (isp->isp_osinfo.mbox_sleep_ok) {
		unsigned int ms = (usecs + 999) / 1000;

		isp->isp_osinfo.mbox_sleep_ok = 0;
		isp->isp_osinfo.mbox_sleeping = 1;
		for (olim = 0; olim < max; olim++) {
			tsleep(&isp->isp_mbxworkp, PRIBIO, "ispmbx_sleep",
			    isp_mstohz(ms));
			if (isp->isp_osinfo.mboxcmd_done) {
				break;
			}
		}
		isp->isp_osinfo.mbox_sleep_ok = 1;
		isp->isp_osinfo.mbox_sleeping = 0;
	} else {
		for (olim = 0; olim < max; olim++) {
			for (ilim = 0; ilim < usecs; ilim += 100) {
				uint32_t isr;
				uint16_t sema, mbox;
				if (isp->isp_osinfo.mboxcmd_done) {
					break;
				}
				if (ISP_READ_ISR(isp, &isr, &sema, &mbox)) {
					isp_intr(isp, isr, sema, mbox);
					if (isp->isp_osinfo.mboxcmd_done) {
						break;
					}
				}
				USEC_DELAY(100);
			}
			if (isp->isp_osinfo.mboxcmd_done) {
				break;
			}
		}
	}
	if (isp->isp_osinfo.mboxcmd_done == 0) {
		isp_prt(isp, ISP_LOGWARN,
		    "%s Mailbox Command (0x%x) Timeout (%uus)",
		    isp->isp_osinfo.mbox_sleep_ok? "Interrupting" : "Polled",
		    isp->isp_lastmbxcmd, usecs);
		mbp->param[0] = MBOX_TIMEOUT;
		isp->isp_osinfo.mboxcmd_done = 1;
	}
}

void
isp_mbox_notify_done(ispsoftc_t *isp)
{
	if (isp->isp_osinfo.mbox_sleeping) {
		wakeup(&isp->isp_mbxworkp);
	}
	isp->isp_osinfo.mboxcmd_done = 1;
}

void
isp_mbox_release(ispsoftc_t *isp)
{
	isp->isp_osinfo.mboxbsy = 0;
}

int
isp_mstohz(int ms)
{
	int hz;
	struct timeval t;
	t.tv_sec = ms / 1000;
	t.tv_usec = (ms % 1000) * 1000;
	hz = tvtohz(&t);
	if (hz < 0) {
		hz = 0x7fffffff;
	}
	if (hz == 0) {
		hz = 1;
	}
	return (hz);
}
