/* 	$OpenBSD: isp_openbsd.c,v 1.7 2000/02/20 21:22:41 mjacob Exp $ */
/*
 * Platform (OpenBSD) dependent common attachment code for Qlogic adapters.
 *
 *---------------------------------------
 * Copyright (c) 1999 by Matthew Jacob
 * NASA/Ames Research Center
 * All rights reserved.
 *---------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 *  mjacob@nas.nasa.gov
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

static void ispminphys __P((struct buf *));
static int32_t ispcmd_slow __P((ISP_SCSI_XFER_T *));
static int32_t ispcmd __P((ISP_SCSI_XFER_T *));

static struct scsi_device isp_dev = { NULL, NULL, NULL, NULL };

static int isp_poll __P((struct ispsoftc *, ISP_SCSI_XFER_T *, int));
static void isp_watch __P((void *));
static void isp_command_requeue(void *);
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
	 * XXX: For now, skip resets for FC because the method by which
	 * XXX: we deal with loop down after issuing resets (which causes
	 * XXX: port logouts for all devices) needs interrupts to run so
	 * XXX: that async events happen.
	 */
	if (IS_SCSI(isp)) {
		int bus = 0;
		(void) isp_control(isp, ISPCTL_RESET_BUS, &bus);
		if (IS_DUALBUS(isp)) {
			bus++;
			(void) isp_control(isp, ISPCTL_RESET_BUS, &bus);
		}
		/*
		 * wait for the bus to settle.
		 */
		printf("%s: waiting 4 seconds for bus reset settling\n",
		    isp->isp_name);
		delay(4 * 1000000);
	} else {
		int i, j;
		fcparam *fcp = isp->isp_param;
		/*
		 * wait for the loop to settle.
		 */
		printf("%s: waiting 2 seconds for loop reset settling\n",
		    isp->isp_name);
		delay(2 * 1000000);
		for (j = 0; j < 5; j++) {
			for (i = 0; i < 5; i++) {
				if (isp_control(isp, ISPCTL_FCLINK_TEST, NULL))
					continue;
#ifdef	ISP2100_FABRIC
				/*
				 * Wait extra time to see if the f/w
				 * eventually completed an FLOGI that
				 * will allow us to know we're on a
				 * fabric.
				 */
				if (fcp->isp_onfabric == 0) {
					delay(1 * 1000000);
					continue;
				}
#endif
				break;
			}
			if (fcp->isp_fwstate == FW_READY &&
			    fcp->isp_loopstate >= LOOP_PDB_RCVD) { 
				break;
			}
		}
		lptr->adapter_target = fcp->isp_loopid;
	}

	/*
	 * Start the watchdog.
	 *
	 * The watchdog will, ridiculously enough, also enable Sync negotiation.
	 */
	isp->isp_dogactive = 1;
	timeout(isp_watch, isp, WATCH_INTERVAL * hz);

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
	ISP_SCSI_XFER_T *xs;
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
		CFGPRINTF("%s: allowing sync/wide negotiation and "
		    "tag usage\n", isp->isp_name);
		isp->isp_osinfo._adapter.scsipi_cmd = ispcmd;
		if (IS_12X0(isp))
			isp->isp_update |= 2;
	}
#endif
	return (ispcmd(xs));
}

static int32_t
ispcmd(xs)
	ISP_SCSI_XFER_T *xs;
{
	struct ispsoftc *isp;
	int result;
	int s;

	isp = xs->sc_link->adapter_softc;
	s = splbio();

	if (isp->isp_state < ISP_RUNSTATE) {
		DISABLE_INTS(isp);
		isp_init(isp);
		if (isp->isp_state != ISP_INITSTATE) {
			ENABLE_INTS(isp);
			(void) splx(s);
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
		IDPRINTF(2, ("%s: blocked\n", isp->isp_name));
		if (xs->flags & SCSI_POLL) {
			xs->error = XS_DRIVER_STUFFUP;
			splx(s);
			return (TRY_AGAIN_LATER);
		}
		if (isp->isp_osinfo.wqf != NULL) {
			isp->isp_osinfo.wqt->free_list.le_next = xs;
		} else {
			isp->isp_osinfo.wqf = xs;
		}
		isp->isp_osinfo.wqt = xs;
		xs->free_list.le_next = NULL;
		splx(s);
		return (SUCCESSFULLY_QUEUED);
	}
	DISABLE_INTS(isp);
	result = ispscsicmd(xs);
	ENABLE_INTS(isp);

	if ((xs->flags & SCSI_POLL) == 0) {
		switch (result) {
		case CMD_QUEUED:
			result = SUCCESSFULLY_QUEUED;
			break;
		case CMD_EAGAIN:
			result = TRY_AGAIN_LATER;
			break;
		case CMD_RQLATER:
			result = SUCCESSFULLY_QUEUED;
			timeout(isp_command_requeue, xs, hz);
			break;
		case CMD_COMPLETE:
			result = COMPLETE;
			break;
		}
		(void) splx(s);
		return (result);
	}

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

	/*
	 * We can't use interrupts so poll on completion.
	 */
	if (result == SUCCESSFULLY_QUEUED) {
		if (isp_poll(isp, xs, xs->timeout)) {
			/*
			 * If no other error occurred but we didn't finish,
			 * something bad happened.
			 */
			if (XS_IS_CMD_DONE(xs) == 0) {
				if (isp_control(isp, ISPCTL_ABORT_CMD, xs)) {
					isp_restart(isp);
				}
				if (XS_NOERR(xs)) {
					XS_SETERR(xs, HBA_BOTCH);
				}
			}
		}
		result = COMPLETE;
	}
	(void) splx(s);
	return (result);
}

static int
isp_poll(isp, xs, mswait)
	struct ispsoftc *isp;
	ISP_SCSI_XFER_T *xs;
	int mswait;
{

	while (mswait) {
		/* Try the interrupt handling routine */
		(void)isp_intr((void *)isp);

		/* See if the xs is now done */
		if (XS_IS_CMD_DONE(xs)) {
			return (0);
		}
		delay(1000);	/* wait one millisecond */
		mswait--;
	}
	return (1);
}


static void
isp_watch(arg)
	void *arg;
{
	int i;
	struct ispsoftc *isp = arg;
	struct scsi_xfer *xs;
	int s;

	/*
	 * Look for completely dead commands (but not polled ones).
	 */
	s = splbio();
	for (i = 0; i < isp->isp_maxcmds; i++) {
		if ((xs = (struct scsi_xfer *) isp->isp_xflist[i]) == NULL) {
			continue;
		}
		if (xs->timeout == 0 || (xs->flags & SCSI_POLL)) {
			continue;
		}
		xs->timeout -= (WATCH_INTERVAL * 1000);

		/*
		 * Avoid later thinking that this
		 * transaction is not being timed.
		 * Then give ourselves to watchdog
		 * periods of grace.
		 */
		if (xs->timeout == 0) {
			xs->timeout = 1;
		} else if (xs->timeout > -(2 * WATCH_INTERVAL * 1000)) {
			continue;
		}
		if (isp_control(isp, ISPCTL_ABORT_CMD, xs)) {
			printf("%s: isp_watch failed to abort command\n",
			    isp->isp_name);
			isp_restart(isp);
			break;
		}
	}
        timeout(isp_watch, isp, WATCH_INTERVAL * hz);
	isp->isp_dogactive = 1;
	splx(s);
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
	int s = splbio();
	/*
	 * Leave with interrupts disabled.
	 */
	DISABLE_INTS(isp);

	/*
	 * Turn off the watchdog (if active).
	 */
	if (isp->isp_dogactive) {
		untimeout(isp_watch, isp);
		isp->isp_dogactive = 0;
	}

	splx(s);
}

/*
 * Restart function for a command to be requeued later.
 */
static void
isp_command_requeue(void *arg)
{
	struct scsi_xfer *xs = arg;
	struct ispsoftc *isp = XS_ISP(xs);
	int s = splbio();
	switch (ispcmd_slow(xs)) {
	case SUCCESSFULLY_QUEUED:
		printf("%s: isp_command_reque: queued %d.%d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;
	case TRY_AGAIN_LATER:
		printf("%s: EAGAIN for %d.%d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		/* FALLTHROUGH */
	case COMPLETE:
		/* can only be an error */
		if (XS_NOERR(xs))
			XS_SETERR(xs, XS_DRIVER_STUFFUP);
		XS_CMD_DONE(xs);
		break;
	}
	(void) splx(s);
}

/*
 * Restart function after a LOOP UP event (e.g.),
 * done as a timeout for some hysteresis.
 */
static void
isp_internal_restart(void *arg)
{
	struct ispsoftc *isp = arg;
	int result, nrestarted = 0, s;

	s = splbio();
	if (isp->isp_osinfo.blocked == 0) {
		struct scsi_xfer *xs;
		while ((xs = isp->isp_osinfo.wqf) != NULL) {
			isp->isp_osinfo.wqf = xs->free_list.le_next;
			xs->free_list.le_next = NULL;
			DISABLE_INTS(isp);
			result = ispscsicmd(xs);
			ENABLE_INTS(isp);
			if (result != CMD_QUEUED) {
				printf("%s: botched command restart (0x%x)\n",
				    isp->isp_name, result);
				if (XS_NOERR(xs))
					XS_SETERR(xs, XS_DRIVER_STUFFUP);
				XS_CMD_DONE(xs);
			}
			nrestarted++;
		}
		printf("%s: requeued %d commands\n", isp->isp_name, nrestarted);
	}
	(void) splx(s);
}

int
isp_async(isp, cmd, arg)
	struct ispsoftc *isp;
	ispasync_t cmd;
	void *arg;
{
	int bus, tgt;
	int s = splbio();
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
			wt = ", 16 bit wide\n";
			break;
		case DPARM_TQING:
			wt = ", Tagged Queueing Enabled\n";
			break;
		case DPARM_WIDE|DPARM_TQING:
			wt = ", 16 bit wide, Tagged Queueing Enabled\n";
			break;
		default:
			wt = "\n";
			break;
		}
		if (mhz) {
			CFGPRINTF("%s: Bus %d Target %d at %dMHz Max "
			    "Offset %d%s", isp->isp_name, bus, tgt, mhz,
			    sdp->isp_devparam[tgt].cur_offset, wt);
		} else {
			CFGPRINTF("%s: Bus %d Target %d Async Mode%s",
			    isp->isp_name, bus, tgt, wt);
		}
		break;
	}
	case ISPASYNC_BUS_RESET:
		if (arg)
			bus = *((int *) arg);
		else
			bus = 0;
		printf("%s: SCSI bus %d reset detected\n", isp->isp_name, bus);
		break;
	case ISPASYNC_LOOP_DOWN:
		/*
		 * Hopefully we get here in time to minimize the number
		 * of commands we are firing off that are sure to die.
		 */
		isp->isp_osinfo.blocked = 1;
		printf("%s: Loop DOWN\n", isp->isp_name);
		break;
        case ISPASYNC_LOOP_UP:
		isp->isp_osinfo.blocked = 0;
		timeout(isp_internal_restart, isp, 1);
		printf("%s: Loop UP\n", isp->isp_name);
		break;
	case ISPASYNC_PDB_CHANGED:
	if (IS_FC(isp) && isp->isp_dblev) {
		const char *fmt = "%s: Target %d (Loop 0x%x) Port ID 0x%x "
		    "role %s %s\n Port WWN 0x%08x%08x\n Node WWN 0x%08x%08x\n";
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
		printf(fmt, isp->isp_name, tgt, lp->loopid, lp->portid,
		    roles[lp->roles & 0x3], ptr,
		    (u_int32_t) (lp->port_wwn >> 32),
		    (u_int32_t) (lp->port_wwn & 0xffffffffLL),
		    (u_int32_t) (lp->node_wwn >> 32),
		    (u_int32_t) (lp->node_wwn & 0xffffffffLL));
		break;
	}
#ifdef	ISP2100_FABRIC
	case ISPASYNC_CHANGE_NOTIFY:
		printf("%s: Name Server Database Changed\n", isp->isp_name);
		break;
	case ISPASYNC_FABRIC_DEV:
	{
		int target;
		struct lportdb *lp;
		char *pt;
		sns_ganrsp_t *resp = (sns_ganrsp_t *) arg;
		u_int32_t portid;
		u_int64_t wwpn, wwnn;
		fcparam *fcp = isp->isp_param;

		portid =
		    (((u_int32_t) resp->snscb_port_id[0]) << 16) |
		    (((u_int32_t) resp->snscb_port_id[1]) << 8) |
		    (((u_int32_t) resp->snscb_port_id[2]));

		wwpn =
		    (((u_int64_t)resp->snscb_portname[0]) << 56) |
		    (((u_int64_t)resp->snscb_portname[1]) << 48) |
		    (((u_int64_t)resp->snscb_portname[2]) << 40) |
		    (((u_int64_t)resp->snscb_portname[3]) << 32) |
		    (((u_int64_t)resp->snscb_portname[4]) << 24) |
		    (((u_int64_t)resp->snscb_portname[5]) << 16) |
		    (((u_int64_t)resp->snscb_portname[6]) <<  8) |
		    (((u_int64_t)resp->snscb_portname[7]));

		wwnn =
		    (((u_int64_t)resp->snscb_nodename[0]) << 56) |
		    (((u_int64_t)resp->snscb_nodename[1]) << 48) |
		    (((u_int64_t)resp->snscb_nodename[2]) << 40) |
		    (((u_int64_t)resp->snscb_nodename[3]) << 32) |
		    (((u_int64_t)resp->snscb_nodename[4]) << 24) |
		    (((u_int64_t)resp->snscb_nodename[5]) << 16) |
		    (((u_int64_t)resp->snscb_nodename[6]) <<  8) |
		    (((u_int64_t)resp->snscb_nodename[7]));
		if (portid == 0 || wwpn == 0) {
			break;
		}

		switch (resp->snscb_port_type) {
		case 1:
			pt = "   N_Port";
			break;
		case 2:
			pt = "  NL_Port";
			break;
		case 3:
			pt = "F/NL_Port";
			break;
		case 0x7f:
			pt = "  Nx_Port";
			break;
		case 0x81:
			pt = "  F_port";
			break;
		case 0x82:
			pt = "  FL_Port";
			break;
		case 0x84:
			pt = "   E_port";
			break;
		default:
			pt = "?";
			break;
		}
		CFGPRINTF("%s: %s @ 0x%x, Node 0x%08x%08x Port %08x%08x\n",
		    isp->isp_name, pt, portid,
		    ((u_int32_t) (wwnn >> 32)), ((u_int32_t) wwnn),
		    ((u_int32_t) (wwpn >> 32)), ((u_int32_t) wwpn));
#if	0
		if ((resp->snscb_fc4_types[1] & 0x1) == 0) {
			printf("Types 0..3: 0x%x 0x%x 0x%x 0x%x\n",
			    resp->snscb_fc4_types[0], resp->snscb_fc4_types[1],
			    resp->snscb_fc4_types[3], resp->snscb_fc4_types[3]);
			break;
		}
#endif
		for (target = FC_SNS_ID+1; target < MAX_FC_TARG; target++) {
			lp = &fcp->portdb[target];
			if (lp->port_wwn == wwpn && lp->node_wwn == wwnn)
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
			printf("%s: no more space for fabric devices\n",
			    isp->isp_name);
			break;
		}
		lp->node_wwn = wwnn;
		lp->port_wwn = wwpn;
		lp->portid = portid;
		break;
	}
#endif
	default:
		break;
	}
	(void) splx(s);
	return (0);
}
