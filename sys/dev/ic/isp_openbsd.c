/* 	$OpenBSD: isp_openbsd.c,v 1.3 1999/03/25 22:58:38 mjacob Exp $ */
/* release_03_25_99 */
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
 *  2339 3rd Street
 *  Suite 24
 *  San Francisco, CA, 94107
 */

#include <dev/ic/isp_openbsd.h>

static void ispminphys __P((struct buf *));
static int32_t ispcmd __P((ISP_SCSI_XFER_T *));

static struct scsi_device isp_dev = { NULL, NULL, NULL, NULL };
static int isp_poll __P((struct ispsoftc *, ISP_SCSI_XFER_T *, int));
static void isp_watch __P((void *));

struct cfdriver isp_cd = {
	NULL, "isp", DV_DULL
};


#define	FC_OPENINGS	RQUEST_QUEUE_LEN / (MAX_FC_TARG-1)
#define	PI_OPENINGS	RQUEST_QUEUE_LEN / (MAX_TARGETS-1)
#define	DTHR		1

/*
 * Complete attachment of hardware, include subdevices.
 */
void
isp_attach(isp)
	struct ispsoftc *isp;
{
	isp->isp_osinfo._adapter.scsi_cmd = ispcmd;
	isp->isp_osinfo._adapter.scsi_minphys = ispminphys;

	isp->isp_state = ISP_RUNSTATE;
	/*
	 * OpenBSD will lose on the 1240 support because you don't
	 * get multiple SCSI busses per adapter instance.
	 */
#if	0
	isp->isp_osinfo._link.channel = SCSI_CHANNEL_ONLY_ONE;
#endif
	isp->isp_osinfo._link.adapter_softc = isp;
	isp->isp_osinfo._link.device = &isp_dev;
	isp->isp_osinfo._link.adapter = &isp->isp_osinfo._adapter;

	if (isp->isp_type & ISP_HA_FC) {
		isp->isp_osinfo._link.openings = FC_OPENINGS;
		isp->isp_osinfo._link.adapter_buswidth = MAX_FC_TARG;
		/* We can set max lun width here */
		isp->isp_osinfo._link.adapter_target =
			((fcparam *)isp->isp_param)->isp_loopid;
	} else {
		isp->isp_osinfo.delay_throttle_count = DTHR;
		isp->isp_osinfo._link.openings = PI_OPENINGS;
		isp->isp_osinfo._link.adapter_buswidth = MAX_TARGETS;
		/* We can set max lun width here */
		isp->isp_osinfo._link.adapter_target =
			((sdparam *)isp->isp_param)->isp_initiator_id;
	}
	if (isp->isp_osinfo._link.openings < 2)
		isp->isp_osinfo._link.openings = 2;

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
		(void) isp_control(isp, ISPCTL_RESET_BUS, NULL);
		/*
		 * Wait for it to settle.
		 */
		delay(2 * 1000000);
	}

	/*
	 * Start the watchdog.
	 *
	 * The wathdog will, ridiculously enough, also enable Sync negotiation.
	 */
	isp->isp_dogactive = 1;
	timeout(isp_watch, isp, WATCH_INTERVAL * hz);

	/*
	 * And attach children (if any).
	 */
	config_found((void *)isp, &isp->isp_osinfo._link, scsiprint);
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

static int
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
	DISABLE_INTS(isp);
	result = ispscsicmd(xs);
	ENABLE_INTS(isp);
	if (result != CMD_QUEUED || (xs->flags & SCSI_POLL) == 0) {
		(void) splx(s);
		return (result);
	}

	/*
	 * If we can't use interrupts, poll on completion.
	 */
	if (isp_poll(isp, xs, xs->timeout)) {
		/*
		 * If no other error occurred but we didn't finish,
		 * something bad happened.
		 */
		if ((xs->flags & ITSDONE) == 0) {
			isp->isp_nactive--;
			if (isp->isp_nactive < 0)
				isp->isp_nactive = 0;
			if (xs->error == XS_NOERROR) {
				isp_lostcmd(isp, xs);
				xs->error = XS_DRIVER_STUFFUP;
			}
		}
	}
	(void) splx(s);
	return (COMPLETE);
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
	ISP_SCSI_XFER_T *xs;
	int s = splbio();

	/*
	 * Look for completely dead commands.
	 */
	for (i = 0; i < RQUEST_QUEUE_LEN; i++) {
		if ((xs = (ISP_SCSI_XFER_T *) isp->isp_xflist[i]) == NULL) {
			continue;
		}
		if (XS_TIME(xs) == 0) {
			continue;
		}
		XS_TIME(xs) -= (WATCH_INTERVAL * 1000);
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
		if (IS_SCSI(isp)) {
			isp->isp_osinfo.delay_throttle_count = DTHR;
		}
		if (isp_control(isp, ISPCTL_ABORT_CMD, xs)) {
			printf("%s: isp_watch failed to abort command\n",
			    isp->isp_name);
			isp_restart(isp);
			break;
		}
	}

	if (isp->isp_osinfo.delay_throttle_count) {
		if (--isp->isp_osinfo.delay_throttle_count == 0) {
			sdparam *sdp = isp->isp_param;
			for (i = 0; i < MAX_TARGETS; i++) {
				sdp->isp_devparam[i].dev_flags |=
					DPARM_WIDE|DPARM_SYNC|DPARM_TQING;
				sdp->isp_devparam[i].dev_update = 1;
			}
			isp->isp_update = 1;
		}
	}
	timeout(isp_watch, isp, WATCH_INTERVAL * hz);
	isp->isp_dogactive = 1;
	(void) splx(s);
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

int
isp_async(isp, cmd, arg)
	struct ispsoftc *isp;
	ispasync_t cmd;
	void *arg;
{
	int s = splbio();
	switch (cmd) {
	case ISPASYNC_NEW_TGT_PARAMS:
		if (IS_SCSI(isp)) {
			sdparam *sdp = isp->isp_param;
			char *wt;
			int mhz, flags, tgt, period;

			tgt = *((int *) arg);

			flags = sdp->isp_devparam[tgt].cur_dflags;
			period = sdp->isp_devparam[tgt].cur_period;
			if ((flags & DPARM_SYNC) && period &&
			    (sdp->isp_devparam[tgt].cur_offset) != 0) {
				if (sdp->isp_lvdmode) {
					switch (period) {
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
				printf("%s: Target %d at %dMHz Max Offset %d%s",
				    isp->isp_name, tgt, mhz,
				    sdp->isp_devparam[tgt].cur_offset, wt);
			} else {
				printf("%s: Target %d Async Mode%s",
				    isp->isp_name, tgt, wt);
			}
		}
		break;
	case ISPASYNC_BUS_RESET:
		printf("%s: SCSI bus reset detected\n", isp->isp_name);
		break;
	case ISPASYNC_LOOP_DOWN:
		printf("%s: Loop DOWN\n", isp->isp_name);
		break;
	case ISPASYNC_LOOP_UP:
		printf("%s: Loop UP\n", isp->isp_name);
		break;
	case ISPASYNC_PDB_CHANGE_COMPLETE:
#if	0
	if (isp->isp_type & ISP_HA_FC) {
		int i;
		static char *roles[4] = {
		    "No", "Target", "Initiator", "Target/Initiator"
		};
		for (i = 0; i < MAX_FC_TARG; i++)  {
			isp_pdb_t *pdbp =
			    &((fcparam *)isp->isp_param)->isp_pdb[i];
			if (pdbp->pdb_options == INVALID_PDB_OPTIONS)
				continue;
			printf("%s: Loop ID %d, %s role\n",
			    isp->isp_name, pdbp->pdb_loopid,
			    roles[(pdbp->pdb_prli_svc3 >> 4) & 0x3]);
			printf("     Node Address 0x%x WWN 0x"
			    "%02x%02x%02x%02x%02x%02x%02x%02x\n",
			    BITS2WORD(pdbp->pdb_portid_bits),
			    pdbp->pdb_portname[0], pdbp->pdb_portname[1],
			    pdbp->pdb_portname[2], pdbp->pdb_portname[3],
			    pdbp->pdb_portname[4], pdbp->pdb_portname[5],
			    pdbp->pdb_portname[6], pdbp->pdb_portname[7]);
			if (pdbp->pdb_options & PDB_OPTIONS_ADISC)
				printf("     Hard Address 0x%x WWN 0x"
				    "%02x%02x%02x%02x%02x%02x%02x%02x\n",
				    BITS2WORD(pdbp->pdb_hardaddr_bits),
				    pdbp->pdb_nodename[0],
				    pdbp->pdb_nodename[1],
				    pdbp->pdb_nodename[2],
				    pdbp->pdb_nodename[3],
				    pdbp->pdb_nodename[4],
				    pdbp->pdb_nodename[5],
				    pdbp->pdb_nodename[6],
				    pdbp->pdb_nodename[7]);
			switch (pdbp->pdb_prli_svc3 & SVC3_ROLE_MASK) {
			case SVC3_TGT_ROLE|SVC3_INI_ROLE:
				printf("     Master State=%s, Slave State=%s\n",
				    isp2100_pdb_statename(pdbp->pdb_mstate),
				    isp2100_pdb_statename(pdbp->pdb_sstate));
				break;
			case SVC3_TGT_ROLE:
				printf("     Master State=%s\n",
				    isp2100_pdb_statename(pdbp->pdb_mstate));
				break;
			case SVC3_INI_ROLE:
				printf("     Slave State=%s\n",
				    isp2100_pdb_statename(pdbp->pdb_sstate));
				break;
			default:
				break;
			}
		}
		break;
	}
#else
		break;
#endif
	case ISPASYNC_CHANGE_NOTIFY:
		printf("%s: Name Server Database Changed\n", isp->isp_name);
		break;
	default:
		break;
	}
	(void) splx(s);
	return (0);
}
