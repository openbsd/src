/* 	$OpenBSD: isp_openbsd.c,v 1.12 2000/10/16 01:02:00 mjacob Exp $ */
/*
 * Platform (OpenBSD) dependent common attachment code for Qlogic adapters.
 *
 * Copyright (c) 1999, 2000 by Matthew Jacob
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

static void ispminphys __P((struct buf *));
static int32_t ispcmd_slow __P((XS_T *));
static int32_t ispcmd __P((XS_T *));

static struct scsi_device isp_dev = { NULL, NULL, NULL, NULL };

static int isp_polled_cmd __P((struct ispsoftc *, XS_T *));
static void isp_wdog __P((void *));
static void isp_requeue(void *);
static void isp_internal_restart(void *);

struct cfdriver isp_cd = {
	NULL, "isp", DV_DULL
};


/*
 * Complete attachment of hardware, include subdevices.
 */
void
isp_attach(isp)
	struct ispsoftc *isp;
{
	struct scsi_link *lptr = &isp->isp_osinfo._link[0];
	isp->isp_osinfo._adapter.scsi_minphys = ispminphys;

	isp->isp_state = ISP_RUNSTATE;

	/*
	 * We only manage a single wait queues for dual bus controllers.
	 * This is arguably broken.
	 */
	isp->isp_osinfo.wqf = isp->isp_osinfo.wqt = NULL;

	lptr->adapter_softc = isp;
	lptr->device = &isp_dev;
	lptr->adapter = &isp->isp_osinfo._adapter;
	lptr->openings = isp->isp_maxcmds;
	if (IS_FC(isp)) {
		isp->isp_osinfo._adapter.scsi_cmd = ispcmd;
		lptr->adapter_buswidth = MAX_FC_TARG;
		/* We can set max lun width here */
		/* loopid set below */
	} else {
		sdparam *sdp = isp->isp_param;
		isp->isp_osinfo._adapter.scsi_cmd = ispcmd_slow;
		lptr->adapter_buswidth = MAX_TARGETS;
		/* We can set max lun width here */
		lptr->adapter_target = sdp->isp_initiator_id;
		isp->isp_osinfo.discovered[0] = 1 << sdp->isp_initiator_id;
		if (IS_DUALBUS(isp)) {
			struct scsi_link *lptrb = &isp->isp_osinfo._link[1];
			lptrb->adapter_softc = isp;
			lptrb->device = &isp_dev;
			lptrb->adapter = &isp->isp_osinfo._adapter;
			lptrb->openings = isp->isp_maxcmds;
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
		printf("%s: waiting 4 seconds for bus reset settling\n",
		    isp->isp_name);
		delay(4 * 1000000);
	} else {
		int defid;
		fcparam *fcp = isp->isp_param;
		delay(2 * 1000000);
		defid = MAX_FC_TARG;
		ISP_LOCK(isp);
		/*
		 * We probably won't have clock interrupts running,
		 * so we'll be really short (smoke test, really)
		 * at this time.
		 */
		if (isp_control(isp, ISPCTL_FCLINK_TEST, NULL)) {
			(void) isp_control(isp, ISPCTL_PDB_SYNC, NULL);
			if (fcp->isp_fwstate == FW_READY &&
			    fcp->isp_loopstate >= LOOP_PDB_RCVD) { 
				defid = fcp->isp_loopid;
			}
		}
		ISP_UNLOCK(isp);
		lptr->adapter_target = fcp->isp_loopid;
	}

	/*
	 * After this point, we *could* be doing the new configuration
	 * schema which allows interrups, so we can do tsleep/wakeup
	 * for mailbox stuff at that point.
	 */
#if	0
	isp->isp_osinfo.no_mbox_ints = 0;
#endif

	/*
	 * And attach children (if any).
	 */
	config_found((void *)isp, lptr, scsiprint);
	if (IS_DUALBUS(isp)) {
		lptr++;
		config_found((void *)isp, lptr, scsiprint);
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
ispminphys(bp)
	struct buf *bp;
{
	/*
	 * XX: Only the 1020 has a 24 bit limit.
	 */
	if (bp->b_bcount >= (1 << 24)) {
		bp->b_bcount = (1 << 24);
	}
	minphys(bp);
}

static int32_t
ispcmd_slow(xs)
	XS_T *xs;
{
	sdparam *sdp;
	int tgt, chan;
	u_int16_t f;
	struct ispsoftc *isp = XS_ISP(xs);

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

	f = DPARM_DEFAULT;
	if (xs->sc_link->quirks & SDEV_NOSYNC) {
		f ^= DPARM_SYNC;
#ifdef	DEBUG
	} else {
		printf("%s: channel %d target %d can do SYNC xfers\n",
		    isp->isp_name, chan, tgt);
#endif
	}
	if (xs->sc_link->quirks & SDEV_NOWIDE) {
		f ^= DPARM_WIDE;
#ifdef	DEBUG
	} else {
		printf("%s: channel %d target %d can do WIDE xfers\n",
		    isp->isp_name, chan, tgt);
#endif
	}
	if (xs->sc_link->quirks & SDEV_NOTAGS) {
		f ^= DPARM_TQING;
#ifdef	DEBUG
	} else {
		printf("%s: channel %d target %d can do TAGGED xfers\n",
		    isp->isp_name, chan, tgt);
#endif
	}

	/*
	 * Okay, we know about this device now,
	 * so mark parameters to be updated for it.
	 */
	sdp->isp_devparam[tgt].dev_flags = f;
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

static int32_t
ispcmd(xs)
	XS_T *xs;
{
	struct ispsoftc *isp;
	int result;

	/*
	 * Make sure that there's *some* kind of sane setting.
	 */
	timeout_set(&xs->stimeout, isp_wdog, isp);
	timeout_del(&xs->stimeout);

	isp = XS_ISP(xs);

	ISP_LOCK(isp);
	if (isp->isp_state < ISP_RUNSTATE) {
		DISABLE_INTS(isp);
		isp_init(isp);
		if (isp->isp_state != ISP_INITSTATE) {
			ENABLE_INTS(isp);
			ISP_UNLOCK(isp);
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_COMPLETE);
		}
		isp->isp_state = ISP_RUNSTATE;
		ENABLE_INTS(isp);
	}

	/*
	 * Check for queue blockage...
	 */
	if (isp->isp_osinfo.blocked) {
		isp_prt(isp, ISP_LOGDEBUG2, "blocked");
		if (xs->flags & SCSI_POLL) {
			xs->error = XS_DRIVER_STUFFUP;
			ISP_UNLOCK(isp);
			return (TRY_AGAIN_LATER);
		}
		if (isp->isp_osinfo.wqf != NULL) {
			isp->isp_osinfo.wqt->free_list.le_next = xs;
		} else {
			isp->isp_osinfo.wqf = xs;
		}
		isp->isp_osinfo.wqt = xs;
		xs->free_list.le_next = NULL;
		ISP_UNLOCK(isp);
		return (SUCCESSFULLY_QUEUED);
	}

	if (xs->flags & SCSI_POLL) {
		volatile u_int8_t ombi = isp->isp_osinfo.no_mbox_ints;
		isp->isp_osinfo.no_mbox_ints = 1;
		result = isp_polled_cmd(isp, xs);
		isp->isp_osinfo.no_mbox_ints = ombi;
		ISP_UNLOCK(isp);
		return (result);
	}

	result = isp_start(xs);

#if	0
{
	static int na[16] = { 0 };
	if (na[isp->isp_unit] < isp->isp_nactive) {
		isp_prt(isp, ISP_LOGALL, "active hiwater %d", isp->isp_nactive);
		na[isp->isp_unit] = isp->isp_nactive;
	}
}
#endif

	switch (result) {
	case CMD_QUEUED:
		result = SUCCESSFULLY_QUEUED;
		if (xs->timeout) {
			timeout_add(&xs->stimeout, _XT(xs));
		}
		break;
	case CMD_EAGAIN:
#if	0
		result = TRY_AGAIN_LATER;
		break;
#endif
	case CMD_RQLATER:
		result = SUCCESSFULLY_QUEUED;
		timeout_set(&xs->stimeout, isp_requeue, xs);
		timeout_add(&xs->stimeout, hz);
		break;
	case CMD_COMPLETE:
		result = COMPLETE;
		break;
	}
	ISP_UNLOCK(isp);
	return (result);
}

static int
isp_polled_cmd(isp, xs)
	struct ispsoftc *isp;
	XS_T *xs;
{
	int result;
	int infinite = 0, mswait;

	result = isp_start(xs);

	switch (result) {
	case CMD_QUEUED:
		result = SUCCESSFULLY_QUEUED;
		break;
	case CMD_RQLATER:
	case CMD_EAGAIN:
		if (XS_NOERR(xs)) {
			xs->error = XS_DRIVER_STUFFUP;
		}
		result = TRY_AGAIN_LATER;
		break;
	case CMD_COMPLETE:
		result = COMPLETE;
		break;
		
	}

	if (result != SUCCESSFULLY_QUEUED) {
		return (result);
	}

	/*
	 * If we can't use interrupts, poll on completion.
	 */
	if ((mswait = XS_TIME(xs)) == 0)
		infinite = 1;

	while (mswait || infinite) {
		if (isp_intr((void *)isp)) {
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
	result = COMPLETE;
	return (result);
}

void
isp_done(xs)
	XS_T *xs;
{
	XS_CMD_S_DONE(xs);
	if (XS_CMD_WDOG_P(xs) == 0) {
		if (xs->timeout) {
			timeout_del(&xs->stimeout);
		}
		if (XS_CMD_GRACE_P(xs)) {
			struct ispsoftc *isp = XS_ISP(xs);
			isp_prt(isp, ISP_LOGDEBUG1,
			    "finished command on borrowed time");
		}
		XS_CMD_S_CLEAR(xs);
		scsi_done(xs);
	}
}

static void
isp_wdog(arg)
	void *arg;
{
	XS_T *xs = arg;
	struct ispsoftc *isp = XS_ISP(xs);
	u_int32_t handle;

	/*
	 * We've decided this command is dead. Make sure we're not trying
	 * to kill a command that's already dead by getting it's handle and
	 * and seeing whether it's still alive.
	 */
	ISP_LOCK(isp);
	handle = isp_find_handle(isp, xs);
	if (handle) {
		u_int16_t r, r1, i;

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

		i = 0;
		do {
			r = ISP_READ(isp, BIU_ISR);
			USEC_DELAY(1);
			r1 = ISP_READ(isp, BIU_ISR);
		} while (r != r1 && ++i < 1000);

		if (INT_PENDING(isp, r) && isp_intr(isp) && XS_CMD_DONE_P(xs)) {
			isp_prt(isp, ISP_LOGDEBUG1, "watchdog cleanup (%x, %x)",
			    isp->isp_name, handle, r);
			XS_CMD_C_WDOG(xs);
			isp_done(xs);
		} else if (XS_CMD_GRACE_P(xs)) {
			/*
			 * Make sure the command is *really* dead before we
			 * release the handle (and DMA resources) for reuse.
			 */
			(void) isp_control(isp, ISPCTL_ABORT_CMD, arg);

			/*
			 * After this point, the comamnd is really dead.
			 */
			if (XS_XFRLEN(xs)) {
				ISP_DMAFREE(isp, xs, handle);
			}
			printf("%s: watchdog timeout (%x, %x)\n", handle, r);
			isp_destroy_handle(isp, handle);
			XS_SETERR(xs, XS_TIMEOUT);
			XS_CMD_S_CLEAR(xs);
			isp_done(xs);
		} else {
			u_int16_t iptr, optr;
			ispreq_t *mp;

			isp_prt(isp, ISP_LOGDEBUG2,
			    "possible command timeout (%x, %x)", handle, r);

			XS_CMD_C_WDOG(xs);
			timeout_add(&xs->stimeout, _XT(xs));
			if (isp_getrqentry(isp, &iptr, &optr, (void **) &mp)) {
				ISP_UNLOCK(isp);
				return;
			}
			XS_CMD_S_GRACE(xs);
			MEMZERO((void *) mp, sizeof (*mp));
			mp->req_header.rqs_entry_count = 1;
			mp->req_header.rqs_entry_type = RQSTYPE_MARKER;
			mp->req_modifier = SYNC_ALL;
			mp->req_target = XS_CHANNEL(xs) << 7;
			ISP_SWIZZLE_REQUEST(isp, mp);
			ISP_ADD_REQUEST(isp, iptr);
		}
	} else if (isp->isp_dblev) {
		isp_prt(isp, ISP_LOGDEBUG2, "watchdog with no command");
	}
	ISP_UNLOCK(isp);
}

/*
 * Free any associated resources prior to decommissioning and
 * set the card to a known state (so it doesn't wake up and kick
 * us when we aren't expecting it to).
 *
 * Locks are held before coming here.
 */
void
isp_uninit(isp)
	struct ispsoftc *isp;
{
	ISP_LOCK(isp);
	/*
	 * Leave with interrupts disabled.
	 */
	DISABLE_INTS(isp);

	ISP_UNLOCK(isp);
}

/*
 * Restart function for a command to be requeued later.
 */
static void
isp_requeue(void *arg)
{
	struct scsi_xfer *xs = arg;
	struct ispsoftc *isp = XS_ISP(xs);
	ISP_LOCK(isp);
	switch (ispcmd_slow(xs)) {
	case SUCCESSFULLY_QUEUED:
		printf("%s: isp_command_reque: queued %d.%d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		if (xs->timeout) {
			timeout_set(&xs->stimeout, isp_wdog, isp);
			timeout_add(&xs->stimeout, _XT(xs));
		}
		break;
	case TRY_AGAIN_LATER:
		printf("%s: EAGAIN for %d.%d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		timeout_set(&xs->stimeout, isp_requeue, xs);
		timeout_add(&xs->stimeout, hz);
		break;
	case COMPLETE:
		/* can only be an error */
		if (XS_NOERR(xs))
			XS_SETERR(xs, XS_DRIVER_STUFFUP);
		XS_CMD_S_DONE(xs);
		break;
	}
	ISP_UNLOCK(isp);
}

/*
 * Restart function after a LOOP UP event (e.g.),
 * done as a timeout for some hysteresis.
 */
static void
isp_internal_restart(void *arg)
{
	struct ispsoftc *isp = arg;
	int result, nrestarted = 0;

	ISP_LOCK(isp);
	if (isp->isp_osinfo.blocked == 0) {
		struct scsi_xfer *xs;
		while ((xs = isp->isp_osinfo.wqf) != NULL) {
			isp->isp_osinfo.wqf = xs->free_list.le_next;
			xs->free_list.le_next = NULL;
			result = isp_start(xs);
			if (result != CMD_QUEUED) {
				printf("%s: botched command restart (0x%x)\n",
				    isp->isp_name, result);
				if (XS_NOERR(xs))
					XS_SETERR(xs, XS_DRIVER_STUFFUP);
				XS_CMD_S_DONE(xs);
			} else if (xs->timeout) {
				timeout_add(&xs->stimeout, _XT(xs));
			}
			nrestarted++;
		}
		printf("%s: requeued %d commands\n", isp->isp_name, nrestarted);
	}
	ISP_UNLOCK(isp);
}

int
isp_async(isp, cmd, arg)
	struct ispsoftc *isp;
	ispasync_t cmd;
	void *arg;
{
	int bus, tgt;

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
		flags = sdp->isp_devparam[tgt].cur_dflags;
		period = sdp->isp_devparam[tgt].cur_period;

		if ((flags & DPARM_SYNC) && period &&
		    (sdp->isp_devparam[tgt].cur_offset) != 0) {
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
			    bus, tgt, mhz, sdp->isp_devparam[tgt].cur_offset,
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
		isp->isp_osinfo.blocked = 1;
		isp_prt(isp, ISP_LOGINFO, "Loop DOWN");
		break;
        case ISPASYNC_LOOP_UP:
		isp->isp_osinfo.blocked = 0;
		timeout_set(&isp->isp_osinfo.rqt, isp_internal_restart, isp);
		timeout_add(&isp->isp_osinfo.rqt, 1);
		isp_prt(isp, ISP_LOGINFO, "Loop UP");
		break;
	case ISPASYNC_PDB_CHANGED:
	if (IS_FC(isp) && isp->isp_dblev) {
		const char *fmt = "Target %d (Loop 0x%x) Port ID 0x%x "
		    "role %s %s\n Port WWN 0x%08x%08x\n Node WWN 0x%08x%08x";
		const static char *roles[4] = {
		    "No", "Target", "Initiator", "Target/Initiator"
		};
		char *ptr;
		fcparam *fcp = isp->isp_param;
		int tgt = *((int *) arg);
		struct lportdb *lp = &fcp->portdb[tgt]; 

		if (lp->valid) {
			ptr = "arrived";
		} else {
			ptr = "disappeared";
		}
		isp_prt(isp, ISP_LOGINFO, fmt, tgt, lp->loopid, lp->portid,
		    roles[lp->roles & 0x3], ptr,
		    (u_int32_t) (lp->port_wwn >> 32),
		    (u_int32_t) (lp->port_wwn & 0xffffffffLL),
		    (u_int32_t) (lp->node_wwn >> 32),
		    (u_int32_t) (lp->node_wwn & 0xffffffffLL));
		break;
	}
#ifdef	ISP2100_FABRIC
	case ISPASYNC_CHANGE_NOTIFY:
		isp_prt(isp, ISP_LOGINFO, "Name Server Database Changed");
		break;
	case ISPASYNC_FABRIC_DEV:
	{
		int target;
		struct lportdb *lp;
		sns_scrsp_t *resp = (sns_scrsp_t *) arg;
		u_int32_t portid;
		u_int64_t wwn;
		fcparam *fcp = isp->isp_param;

		portid =
		    (((u_int32_t) resp->snscb_port_id[0]) << 16) |
		    (((u_int32_t) resp->snscb_port_id[1]) << 8) |
		    (((u_int32_t) resp->snscb_port_id[2]));
		wwn =
		    (((u_int64_t)resp->snscb_portname[0]) << 56) |
		    (((u_int64_t)resp->snscb_portname[1]) << 48) |
		    (((u_int64_t)resp->snscb_portname[2]) << 40) |
		    (((u_int64_t)resp->snscb_portname[3]) << 32) |
		    (((u_int64_t)resp->snscb_portname[4]) << 24) |
		    (((u_int64_t)resp->snscb_portname[5]) << 16) |
		    (((u_int64_t)resp->snscb_portname[6]) <<  8) |
		    (((u_int64_t)resp->snscb_portname[7]));

		isp_prt(isp, ISP_LOGINFO,
		    "Fabric Device (Type 0x%x)@PortID 0x%x WWN 0x%08x%08x",
		    resp->snscb_port_type, portid, ((u_int32_t)(wwn >> 32)),
		    ((u_int32_t)(wwn & 0xffffffff)));

		for (target = FC_SNS_ID+1; target < MAX_FC_TARG; target++) {
			lp = &fcp->portdb[target];
			if (lp->port_wwn == wwn)
				break;
		}
		if (target < MAX_FC_TARG) {
			break;
		}
		for (target = FC_SNS_ID+1; target < MAX_FC_TARG; target++) {
			lp = &fcp->portdb[target];
			if (lp->port_wwn == 0)
				break;
		}
		if (target == MAX_FC_TARG) {
			isp_prt(isp, ISP_LOGWARN,
			    "no more space for fabric devices");
			return (-1);
		}
		lp->port_wwn = lp->node_wwn = wwn;
		lp->portid = portid;
		break;
	}
#endif
	default:
		break;
	}
	return (0);
}

void
#ifdef	__STDC__
isp_prt(struct ispsoftc *isp, int level, const char *fmt, ...)
#else
isp_log(isp, fmt, va_alist)
	struct ispsoftc *isp;
	char *fmt;
	va_dcl;
#endif
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
