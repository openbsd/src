/*	$NetBSD: isp.c,v 1.7 1997/06/08 06:31:52 thorpej Exp $	*/

/*
 * Machine Independent (well, as best as possible)
 * code for the Qlogic ISP SCSI adapters.
 *
 * Specific probe attach and support routines for Qlogic ISP SCSI adapters.
 *
 * Copyright (c) 1997 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
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
 */

/*
 * Inspiration and ideas about this driver are from Erik Moe's Linux driver
 * (qlogicisp.c) and Dave Miller's SBus version of same (qlogicisp.c)
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>  
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h> 
#include <sys/proc.h>
#include <sys/user.h>


#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <scsi/scsi_message.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsiconf.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/ic/ispreg.h>
#include <dev/ic/ispvar.h>
#include <dev/ic/ispmbox.h>

#define	MBOX_DELAY_COUNT	1000000 / 100

struct cfdriver isp_cd = {
	NULL, "isp", DV_DULL
};

static void	ispminphys __P((struct buf *));
static int32_t	ispscsicmd __P((struct scsi_xfer *xs));
static int	isp_mboxcmd __P((struct ispsoftc *, mbreg_t *));

static struct scsi_adapter isp_switch = {
	ispscsicmd, ispminphys, 0, 0
};

static struct scsi_device isp_dev = { NULL, NULL, NULL, NULL };

static int isp_poll __P((struct ispsoftc *, struct scsi_xfer *, int));	
static int isp_parse_status __P((struct ispsoftc *, ispstatusreq_t *));
static void isp_lostcmd __P((struct ispsoftc *, struct scsi_xfer *));

/*
 * Reset Hardware.
 *
 * Only looks at sc_dev.dv_xname, sc_iot and sc_ioh fields.
 */
void
isp_reset(isp)
	struct ispsoftc *isp;
{
	mbreg_t mbs;
	int loops, i;
	u_int8_t clock;

	isp->isp_state = ISP_NILSTATE;
	/*
	 * Do MD specific pre initialization
	 */
	ISP_RESET0(isp);

	/*
	 * Try and get old clock rate out before we hit the
	 * chip over the head- but if and only if we don't
	 * know our desired clock rate.
	 */
	clock = isp->isp_mdvec->dv_clock;
	if (clock == 0) {
		mbs.param[0] = MBOX_GET_CLOCK_RATE;
		(void) isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
			clock = mbs.param[1];
			printf("board-clock 0x%x ", clock);
		} else {
			clock = 0;
		}
	}

	/*
	 * Hit the chip over the head with hammer.
	 */

	ISP_WRITE(isp, BIU_ICR, BIU_ICR_SOFT_RESET);
	/*
	 * Give the ISP a chance to recover...
	 */
	delay(100);

	/*
	 * Clear data && control DMA engines.
	 */
	ISP_WRITE(isp, CDMA_CONTROL,
		      DMA_CNTRL_CLEAR_CHAN | DMA_CNTRL_RESET_INT);
	ISP_WRITE(isp, DDMA_CONTROL,
		      DMA_CNTRL_CLEAR_CHAN | DMA_CNTRL_RESET_INT);
	/*
	 * Wait for ISP to be ready to go...
	 */
	loops = MBOX_DELAY_COUNT;
	while ((ISP_READ(isp, BIU_ICR) & BIU_ICR_SOFT_RESET) != 0) {
		delay(100);
		if (--loops < 0) {
			printf("chip reset timed out\n", isp->isp_name);
			return;
		}
	}
	/*
	 * More initialization
	 */

	ISP_WRITE(isp, BIU_CONF1, 0);
	ISP_WRITE(isp, HCCR, HCCR_CMD_RESET);
	delay(100);

	if (isp->isp_mdvec->dv_conf1) {
		ISP_SETBITS(isp, BIU_CONF1, isp->isp_mdvec->dv_conf1);
		if (isp->isp_mdvec->dv_conf1 & BIU_BURST_ENABLE) {
			ISP_SETBITS(isp, CDMA_CONF, DMA_ENABLE_BURST);
			ISP_SETBITS(isp, DDMA_CONF, DMA_ENABLE_BURST);
		}
	} else {
		ISP_WRITE(isp, BIU_CONF1, 0);
	}

#if	0
	ISP_WRITE(isp, RISC_MTR, 0x1212);	/* FM */
#endif
	ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE); /* release paused processor */

	/*
	 * Do MD specific post initialization
	 */
	ISP_RESET1(isp);

	/*
	 * Enable interrupts
	 */
	ISP_WRITE(isp, BIU_ICR,
		  BIU_ICR_ENABLE_RISC_INT | BIU_ICR_ENABLE_ALL_INTS);

	/*
	 * Do some sanity checking.
	 */

	mbs.param[0] = MBOX_NO_OP;
	(void) isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		printf("NOP test failed\n");
		return;
	}

	mbs.param[0] = MBOX_MAILBOX_REG_TEST;
	mbs.param[1] = 0xdead;
	mbs.param[2] = 0xbeef;
	mbs.param[3] = 0xffff;
	mbs.param[4] = 0x1111;
	mbs.param[5] = 0xa5a5;
	(void) isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		printf("Mailbox Register test didn't complete\n");
		return;
	}
	i = 0;
	if (mbs.param[1] != 0xdead) {
		printf("Register Test Failed @reg %d (got %x)\n",
			1, mbs.param[1]);
		i++;
	}
	if (mbs.param[2] != 0xbeef) {
		printf("Register Test Failed @reg %d (got %x)\n",
			2, mbs.param[2]);
		i++;
	}
	if (mbs.param[3] != 0xffff) {
		printf("Register Test Failed @reg %d (got %x)\n",
			3, mbs.param[3]);
		i++;
	}
	if (mbs.param[4] != 0x1111) {
		printf("Register Test Failed @reg %d (got %x)\n",
			4, mbs.param[4]);
		i++;
	}
	if (mbs.param[5] != 0xa5a5) {
		printf("Register Test Failed @reg %d (got %x)\n",
			5, mbs.param[5]);
		i++;
	}
	if (i) {
		return;
	}

	/*
	 * Download new Firmware
	 */
	for (i = 0; i < isp->isp_mdvec->dv_fwlen; i++) {
		mbs.param[0] = MBOX_WRITE_RAM_WORD;
		mbs.param[1] = isp->isp_mdvec->dv_codeorg + i;
		mbs.param[2] = isp->isp_mdvec->dv_ispfw[i];
		(void) isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			printf("f/w download failed\n");
			return;
		}
	}

	/*
	 * Verify that it downloaded correctly.
	 */
	mbs.param[0] = MBOX_VERIFY_CHECKSUM;
	mbs.param[1] = isp->isp_mdvec->dv_codeorg;
	(void) isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		printf("ram checksum failure\n");
		return;
	}

	/*
	 * Now start it rolling...
	 */

	mbs.param[0] = MBOX_EXEC_FIRMWARE;
	mbs.param[1] = isp->isp_mdvec->dv_codeorg;
	(void) isp_mboxcmd(isp, &mbs);

	/*
	 * Set CLOCK RATE
	 */
	if (clock) {
		mbs.param[0] = MBOX_SET_CLOCK_RATE;
		mbs.param[1] = clock;
		(void) isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			printf("failed to set CLOCKRATE\n");
			return;
		}
	}
	mbs.param[0] = MBOX_ABOUT_FIRMWARE;
	(void) isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		printf("ABOUT FIRMWARE command failed\n");
		return;
	}
	printf("F/W rev %d.%d ", mbs.param[1], mbs.param[2]);
	isp->isp_state = ISP_RESETSTATE;
}

/*
 * Initialize Hardware to known state
 */
void
isp_init(isp)
	struct ispsoftc *isp;
{
	mbreg_t mbs;
	int s, i, l;

	/*
	 * Set Default Host Adapter Parameters
	 * XXX: Should try and get them out of NVRAM
	 */

	isp->isp_adapter_enabled = 1;
	isp->isp_req_ack_active_neg = 1;
	isp->isp_data_line_active_neg = 1;
	isp->isp_cmd_dma_burst_enable = 1;
	isp->isp_data_dma_burst_enabl = 1;
	isp->isp_fifo_threshold = 2;
	isp->isp_initiator_id = 7;
	isp->isp_async_data_setup = 6;
	isp->isp_selection_timeout = 250;
	isp->isp_max_queue_depth = 256;
	isp->isp_tag_aging = 8;
	isp->isp_bus_reset_delay = 3;
	isp->isp_retry_count = 0;
	isp->isp_retry_delay = 1;
	for (i = 0; i < MAX_TARGETS; i++) {
		isp->isp_devparam[i].dev_flags = DPARM_DEFAULT;
		isp->isp_devparam[i].exc_throttle = 16;
		isp->isp_devparam[i].sync_period = 25;
		isp->isp_devparam[i].sync_offset = 12;
		isp->isp_devparam[i].dev_enable = 1;
	}


	s = splbio();

	mbs.param[0] = MBOX_SET_INIT_SCSI_ID;
	mbs.param[1] = isp->isp_initiator_id;
	(void) isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		(void) splx(s);
		printf("%s: failed to set initiator id\n", isp->isp_name);
		return;
	}

	mbs.param[0] = MBOX_SET_RETRY_COUNT;
	mbs.param[1] = isp->isp_retry_count;
	mbs.param[2] = isp->isp_retry_delay;
	(void) isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		(void) splx(s);
		printf("%s: failed to set retry count and delay\n",
		       isp->isp_name);
		return;
	}

	mbs.param[0] = MBOX_SET_ASYNC_DATA_SETUP_TIME;
	mbs.param[1] = isp->isp_async_data_setup;
	(void) isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		(void) splx(s);
		printf("%s: failed to set async data setup time\n",
		       isp->isp_name);
		return;
	}

	mbs.param[0] = MBOX_SET_ACTIVE_NEG_STATE;
	mbs.param[1] =
		(isp->isp_req_ack_active_neg << 4) |
		(isp->isp_data_line_active_neg << 5);
	(void) isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		(void) splx(s);
		printf("%s: failed to set active negation state\n",
		       isp->isp_name);
		return;
	}


	mbs.param[0] = MBOX_SET_TAG_AGE_LIMIT;
	mbs.param[1] = isp->isp_tag_aging;
	(void) isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		(void) splx(s);
		printf("%s: failed to set tag age limit\n", isp->isp_name);
		return;
	}

	mbs.param[0] = MBOX_SET_SELECT_TIMEOUT;
	mbs.param[1] = isp->isp_selection_timeout;
	(void) isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		(void) splx(s);
		printf("%s: failed to set selection timeout\n", isp->isp_name);
		return;
	}

	for (i = 0; i < MAX_TARGETS; i++) {
		if (isp->isp_devparam[i].dev_enable == 0)
			continue;

		mbs.param[0] = MBOX_SET_TARGET_PARAMS;
		mbs.param[1] = i << 8;
		mbs.param[2] = isp->isp_devparam[i].dev_flags << 8;
		mbs.param[3] =
			(isp->isp_devparam[i].sync_offset << 8) |
			(isp->isp_devparam[i].sync_period);
		(void) isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			(void) splx(s);
			printf("%s: failed to set target parameters\n",
			       isp->isp_name);
			return;
		}

		for (l = 0; l < MAX_LUNS; l++) {
			mbs.param[0] = MBOX_SET_DEV_QUEUE_PARAMS;
			mbs.param[1] = (i << 8) | l;
			mbs.param[2] = isp->isp_max_queue_depth;
			mbs.param[3] = isp->isp_devparam[i].exc_throttle;
			(void) isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				(void) splx(s);
				printf("%s: failed to set device queue "
				       "parameters\n", isp->isp_name);
				return;
			}
		}
	}


	/*
	 * Set up DMA for the request and result mailboxes.
	 */
	if (ISP_MBOXDMASETUP(isp)) {
		(void) splx(s);
		printf("%s: can't setup DMA for mailboxes\n", isp->isp_name);
		return;
	}

	mbs.param[0] = MBOX_INIT_RES_QUEUE;
	mbs.param[1] = RESULT_QUEUE_LEN;
	mbs.param[2] = (u_int16_t) (isp->isp_result_dma >> 16);
	mbs.param[3] = (u_int16_t) (isp->isp_result_dma & 0xffff);
	mbs.param[4] = 0;
	mbs.param[5] = 0;
	(void) isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		(void) splx(s);
		printf("%s: set of response queue failed\n", isp->isp_name);
		return;
	}
	isp->isp_residx = 0;

	mbs.param[0] = MBOX_INIT_REQ_QUEUE;
	mbs.param[1] = RQUEST_QUEUE_LEN;
	mbs.param[2] = (u_int16_t) (isp->isp_rquest_dma >> 16);
	mbs.param[3] = (u_int16_t) (isp->isp_rquest_dma & 0xffff);
	mbs.param[4] = 0;
	(void) isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		(void) splx(s);
		printf("%s: set of request queue failed\n", isp->isp_name);
		return;
	}
	isp->isp_reqidx = 0;

	/*	
	 * Unfortunately, this is the only way right now for
	 * forcing a sync renegotiation. If we boot off of
	 * an Alpha, it's put the chip in SYNC mode, but we
	 * haven't necessarily set up the parameters the
	 * same, so we'll have to yank the reset line to
	 * get everyone to renegotiate.
	 */


	mbs.param[0] = MBOX_BUS_RESET;
	mbs.param[1] = 2;
	(void) isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		(void) splx(s);
		printf("%s: SCSI bus reset failed\n", isp->isp_name);
	}
	isp->isp_sendmarker = 1;
	(void) splx(s);
	isp->isp_state = ISP_INITSTATE;
}

/*
 * Complete attachment of Hardware, include subdevices.
 */
void
isp_attach(isp)
	struct ispsoftc *isp;
{
	isp->isp_state = ISP_RUNSTATE;
	isp->isp_link.adapter_softc = isp;
	isp->isp_link.adapter_target = isp->isp_initiator_id;
	isp->isp_link.adapter = &isp_switch;
	isp->isp_link.device = &isp_dev;
	isp->isp_link.openings = RQUEST_QUEUE_LEN / (MAX_TARGETS - 1);
	isp->isp_link.adapter_buswidth = MAX_TARGETS;
	config_found((void *)isp, &isp->isp_link, scsiprint);
}


/*
 * Free any associated resources prior to decommissioning.
 */
void
isp_uninit(isp)
	struct ispsoftc *isp;
{
}

/*
 * minphys our xfers
 */

static void
ispminphys(bp)
	struct buf *bp;
{
	/*
	 * XX: Only the 1020 has a 24 bit limit.
	 */
	if (bp->b_bcount >= (1 << 24)) {
		bp->b_bcount = (1 << 24) - 1;
	}
	minphys(bp);
}

/*
 * start an xfer
 */
static int32_t
ispscsicmd(xs)
	struct scsi_xfer *xs;
{
	struct ispsoftc *isp;
	u_int8_t iptr, optr;
	ispreq_t *req;
	int s, i;

	isp = xs->sc_link->adapter_softc;

	optr = ISP_READ(isp, OUTMAILBOX4);
	iptr = isp->isp_reqidx;

	req = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
	iptr = (iptr + 1) & (RQUEST_QUEUE_LEN - 1);
	if (iptr == optr) {
		printf("%s: Request Queue Overflow\n", isp->isp_name);
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
	}

	s = splbio();
	if (isp->isp_sendmarker) {
		ipsmarkreq_t *marker = (ipsmarkreq_t *) req;

		bzero((void *) marker, sizeof (*marker));
		marker->req_header.rqs_entry_count = 1;
		marker->req_header.rqs_entry_type = RQSTYPE_MARKER;
		marker->req_modifier = SYNC_ALL;

		isp->isp_sendmarker = 0;

		if (((iptr + 1) & (RQUEST_QUEUE_LEN - 1)) == optr) {
			ISP_WRITE(isp, INMAILBOX4, iptr);
			isp->isp_reqidx = iptr;
			(void) splx(s);
			printf("%s: Request Queue Overflow+\n", isp->isp_name);
			xs->error = XS_DRIVER_STUFFUP;
			return (TRY_AGAIN_LATER);
		}
		req = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
		iptr = (iptr + 1) & (RQUEST_QUEUE_LEN - 1);
	}


	bzero((void *) req, sizeof (*req));
	req->req_header.rqs_entry_count = 1;
	req->req_header.rqs_entry_type = RQSTYPE_REQUEST;
	req->req_header.rqs_flags = 0;
	req->req_header.rqs_seqno = isp->isp_seqno++;

	for (i = 0; i < RQUEST_QUEUE_LEN; i++) {
		if (isp->isp_xflist[i] == NULL)
			break;
	}
	if (i == RQUEST_QUEUE_LEN) {
		panic("%s: ran out of xflist pointers\n", isp->isp_name);
		/* NOTREACHED */
	} else {
		isp->isp_xflist[i] = xs;
		req->req_handle = i;
	}

	req->req_flags = 0;
	req->req_lun_trn = xs->sc_link->lun;
	req->req_target = xs->sc_link->target;
	req->req_cdblen = xs->cmdlen;
	bcopy((void *)xs->cmd, req->req_cdb, xs->cmdlen);

#if	0
	printf("%s(%d.%d): START%d cmd 0x%x datalen %d\n", isp->isp_name,
		xs->sc_link->target, xs->sc_link->lun,
		req->req_header.rqs_seqno, *(u_char *) xs->cmd, xs->datalen);
#endif

	req->req_time = xs->timeout / 1000;
	req->req_seg_count = 0;
	if (ISP_DMASETUP(isp, xs, req, &iptr, optr)) {
		(void) splx(s);
		xs->error = XS_DRIVER_STUFFUP;
		return (COMPLETE);
	}
	xs->error = 0;
	ISP_WRITE(isp, INMAILBOX4, iptr);
	isp->isp_reqidx = iptr;
	(void) splx(s);
	if ((xs->flags & SCSI_POLL) == 0) {
		return (SUCCESSFULLY_QUEUED);
	}

	/*
	 * If we can't use interrupts, poll on completion.
	 */
	if (isp_poll(isp, xs, xs->timeout)) {
#if 0
		/* XXX try to abort it, or whatever */
		if (isp_poll(isp, xs, xs->timeout) {
			/* XXX really nuke it */
		}
#endif
		/*
		 * If no other error occurred but we didn't finish,
		 * something bad happened.
		 */
		if ((xs->flags & ITSDONE) == 0 && xs->error == XS_NOERROR) {
			isp_lostcmd(isp, xs);
			xs->error = XS_DRIVER_STUFFUP;
		}
	}
	return (COMPLETE);
}

/*
 * Interrupt Service Routine(s)
 */

int
isp_poll(isp, xs, mswait)
	struct ispsoftc *isp;
	struct scsi_xfer *xs;
	int mswait;
{

	while (mswait) {
		/* Try the interrupt handling routine */
		(void)isp_intr((void *)isp);

		/* See if the xs is now done */
		if (xs->flags & ITSDONE)
			return (0);
		delay(1000);		/* wait one millisecond */
		mswait--;
	}
	return (1);
}

int
isp_intr(arg)
	void *arg;
{
	struct scsi_xfer *xs;
	struct ispsoftc *isp = arg;
	u_int16_t iptr, optr, isr;

	isr = ISP_READ(isp, BIU_ISR);
	if (isr == 0 || (isr & BIU_ISR_RISC_INT) == 0) {
#if	0
		if (isr) {
			printf("%s: isp_intr isr=%x\n", isp->isp_name, isr);
		}
#endif
		return (0);
	}

	optr = isp->isp_residx;
	iptr = ISP_READ(isp, OUTMAILBOX5);
	ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
	ISP_WRITE(isp, BIU_ICR,
		  BIU_ICR_ENABLE_RISC_INT | BIU_ICR_ENABLE_ALL_INTS);

	if (ISP_READ(isp, BIU_SEMA) & 1) {
		u_int16_t mbox0 = ISP_READ(isp, OUTMAILBOX0);
		switch (mbox0) {
		case ASYNC_BUS_RESET:
		case ASYNC_TIMEOUT_RESET:
			printf("%s: bus or timeout reset\n", isp->isp_name);
			isp->isp_sendmarker = 1;
			break;
		default:
			printf("%s: async %x\n", isp->isp_name, mbox0);
			break;
		}
		ISP_WRITE(isp, BIU_SEMA, 0);
#if	0
	} else {
		if (optr == iptr) {
			printf("why'd we interrupt? isr %x iptr %x optr %x\n",
				isr, optr, iptr);
		}
#endif
	}

	while (optr != iptr) {
		ispstatusreq_t *sp;
		int buddaboom = 0;

		sp = (ispstatusreq_t *) ISP_QUEUE_ENTRY(isp->isp_result, optr);

		optr = (optr + 1) & (RESULT_QUEUE_LEN-1);
		if (sp->req_header.rqs_entry_type != RQSTYPE_RESPONSE) {
			printf("%s: not RESPONSE in RESPONSE Queue (0x%x)\n",
				isp->isp_name, sp->req_header.rqs_entry_type);
			if (sp->req_header.rqs_entry_type != RQSTYPE_REQUEST) {
				ISP_WRITE(isp, INMAILBOX5, optr);
				continue;
			}
			buddaboom = 1;
		}

		if (sp->req_header.rqs_flags & 0xf) {
			if (sp->req_header.rqs_flags & RQSFLAG_CONTINUATION) {
				ISP_WRITE(isp, INMAILBOX5, optr);
				continue;
			}
			printf("%s: rqs_flags=%x\n", isp->isp_name,
				sp->req_header.rqs_flags & 0xf);
		}
		if (sp->req_handle >= RQUEST_QUEUE_LEN) {
			printf("%s: bad request handle %d\n", isp->isp_name,
				sp->req_handle);
			ISP_WRITE(isp, INMAILBOX5, optr);
			continue;
		}
		xs = (struct scsi_xfer *) isp->isp_xflist[sp->req_handle];
		if (xs == NULL) {
			printf("%s: NULL xs in xflist\n", isp->isp_name);
			ISP_WRITE(isp, INMAILBOX5, optr);
			continue;
		}
		isp->isp_xflist[sp->req_handle] = NULL;
		if (sp->req_status_flags & RQSTF_BUS_RESET) {
			isp->isp_sendmarker = 1;
		}
		if (buddaboom) {
			xs->error = XS_DRIVER_STUFFUP;
		}
		if (sp->req_state_flags & RQSF_GOT_SENSE) {
			bcopy(sp->req_sense_data, &xs->sense,
				sizeof (xs->sense));
			xs->error = XS_SENSE;
		}
		xs->status = sp->req_scsi_status;
		if (xs->error == 0 && xs->status == SCSI_BUSY)
			xs->error = XS_BUSY;

		if (sp->req_header.rqs_entry_type == RQSTYPE_RESPONSE) {
			if (xs->error == 0)
				xs->error = isp_parse_status(isp, sp);
		} else {
			printf("%s: unknown return %x\n", isp->isp_name,
				sp->req_header.rqs_entry_type);
			if (xs->error == 0)
				xs->error = XS_DRIVER_STUFFUP;
		}
		xs->resid = sp->req_resid;
		xs->flags |= ITSDONE;
		if (xs->datalen) {
			ISP_DMAFREE(isp, xs, sp->req_handle);
		}
#if	0
		printf("%s(%d.%d): FINISH%d cmd 0x%x resid %d STS %x",
			isp->isp_name, xs->sc_link->target, xs->sc_link->lun,
			sp->req_header.rqs_seqno, *(u_char *) xs->cmd,
			xs->resid, xs->status);
		if (sp->req_state_flags & RQSF_GOT_SENSE) {
			printf(" Skey: %x", xs->sense.flags);
			if (xs->error != XS_SENSE) {
				printf(" BUT NOT SET");
			}
		}
		printf(" xs->error %d\n", xs->error);
#endif
		ISP_WRITE(isp, INMAILBOX5, optr);
		scsi_done(xs);
	}
	isp->isp_residx = optr;
	return (1);
}

/*
 * Support routines.
 */

static int
isp_parse_status(isp, sp)
	struct ispsoftc *isp;
	ispstatusreq_t *sp;
{
	switch (sp->req_completion_status) {
	case RQCS_COMPLETE:
		return (XS_NOERROR);
		break;
	case RQCS_INCOMPLETE:
		if ((sp->req_state_flags & RQSF_GOT_TARGET) == 0) {
			return (XS_SELTIMEOUT);
		}
		printf("%s: incomplete, state %x\n",
			isp->isp_name, sp->req_state_flags);
		break;
	case RQCS_DATA_UNDERRUN:
		return (XS_NOERROR);
	case RQCS_TIMEOUT:
		return (XS_TIMEOUT);
	case RQCS_RESET_OCCURRED:
		printf("%s: reset occurred\n", isp->isp_name);
		isp->isp_sendmarker = 1;
		break;
	case RQCS_ABORTED:
		printf("%s: command aborted\n", isp->isp_name);
		isp->isp_sendmarker = 1;
		break;
	default:
		printf("%s: comp status %x\n", isp->isp_name,
		       sp->req_completion_status);
		break;
	}
	return (XS_DRIVER_STUFFUP);
}

#define	HINIB(x)			((x) >> 0x4)
#define	LONIB(x)			((x)  & 0xf)
#define MAKNIB(a, b)			(((a) << 4) | (b))
static u_int8_t mbpcnt[] = {
	MAKNIB(1, 1),	/* MBOX_NO_OP */
	MAKNIB(5, 5),	/* MBOX_LOAD_RAM */
	MAKNIB(2, 0),	/* MBOX_EXEC_FIRMWARE */
	MAKNIB(5, 5),	/* MBOX_DUMP_RAM */
	MAKNIB(3, 3),	/* MBOX_WRITE_RAM_WORD */
	MAKNIB(2, 3),	/* MBOX_READ_RAM_WORD */
	MAKNIB(6, 6),	/* MBOX_MAILBOX_REG_TEST */
	MAKNIB(2, 3),	/* MBOX_VERIFY_CHECKSUM	*/
	MAKNIB(1, 3),	/* MBOX_ABOUT_FIRMWARE */
	MAKNIB(0, 0),	/* 0x0009 */
	MAKNIB(0, 0),	/* 0x000a */
	MAKNIB(0, 0),	/* 0x000b */
	MAKNIB(0, 0),	/* 0x000c */
	MAKNIB(0, 0),	/* 0x000d */
	MAKNIB(1, 2),	/* MBOX_CHECK_FIRMWARE */
	MAKNIB(0, 0),	/* 0x000f */
	MAKNIB(5, 5),	/* MBOX_INIT_REQ_QUEUE */
	MAKNIB(6, 6),	/* MBOX_INIT_RES_QUEUE */
	MAKNIB(4, 4),	/* MBOX_EXECUTE_IOCB */
	MAKNIB(2, 2),	/* MBOX_WAKE_UP	*/
	MAKNIB(1, 6),	/* MBOX_STOP_FIRMWARE */
	MAKNIB(4, 4),	/* MBOX_ABORT */
	MAKNIB(2, 2),	/* MBOX_ABORT_DEVICE */
	MAKNIB(3, 3),	/* MBOX_ABORT_TARGET */
	MAKNIB(2, 2),	/* MBOX_BUS_RESET */
	MAKNIB(2, 3),	/* MBOX_STOP_QUEUE */
	MAKNIB(2, 3),	/* MBOX_START_QUEUE */
	MAKNIB(2, 3),	/* MBOX_SINGLE_STEP_QUEUE */
	MAKNIB(2, 3),	/* MBOX_ABORT_QUEUE */
	MAKNIB(2, 4),	/* MBOX_GET_DEV_QUEUE_STATUS */
	MAKNIB(0, 0),	/* 0x001e */
	MAKNIB(1, 3),	/* MBOX_GET_FIRMWARE_STATUS */
	MAKNIB(1, 2),	/* MBOX_GET_INIT_SCSI_ID */
	MAKNIB(1, 2),	/* MBOX_GET_SELECT_TIMEOUT */
	MAKNIB(1, 3),	/* MBOX_GET_RETRY_COUNT	*/
	MAKNIB(1, 2),	/* MBOX_GET_TAG_AGE_LIMIT */
	MAKNIB(1, 2),	/* MBOX_GET_CLOCK_RATE */
	MAKNIB(1, 2),	/* MBOX_GET_ACT_NEG_STATE */
	MAKNIB(1, 2),	/* MBOX_GET_ASYNC_DATA_SETUP_TIME */
	MAKNIB(1, 3),	/* MBOX_GET_PCI_PARAMS */
	MAKNIB(2, 4),	/* MBOX_GET_TARGET_PARAMS */
	MAKNIB(2, 4),	/* MBOX_GET_DEV_QUEUE_PARAMS */
	MAKNIB(0, 0),	/* 0x002a */
	MAKNIB(0, 0),	/* 0x002b */
	MAKNIB(0, 0),	/* 0x002c */
	MAKNIB(0, 0),	/* 0x002d */
	MAKNIB(0, 0),	/* 0x002e */
	MAKNIB(0, 0),	/* 0x002f */
	MAKNIB(2, 2),	/* MBOX_SET_INIT_SCSI_ID */
	MAKNIB(2, 2),	/* MBOX_SET_SELECT_TIMEOUT */
	MAKNIB(3, 3),	/* MBOX_SET_RETRY_COUNT	*/
	MAKNIB(2, 2),	/* MBOX_SET_TAG_AGE_LIMIT */
	MAKNIB(2, 2),	/* MBOX_SET_CLOCK_RATE */
	MAKNIB(2, 2),	/* MBOX_SET_ACTIVE_NEG_STATE */
	MAKNIB(2, 2),	/* MBOX_SET_ASYNC_DATA_SETUP_TIME */
	MAKNIB(3, 3),	/* MBOX_SET_PCI_CONTROL_PARAMS */
	MAKNIB(4, 4),	/* MBOX_SET_TARGET_PARAMS */
	MAKNIB(4, 4),	/* MBOX_SET_DEV_QUEUE_PARAMS */
	MAKNIB(0, 0),	/* 0x003a */
	MAKNIB(0, 0),	/* 0x003b */
	MAKNIB(0, 0),	/* 0x003c */
	MAKNIB(0, 0),	/* 0x003d */
	MAKNIB(0, 0),	/* 0x003e */
	MAKNIB(0, 0),	/* 0x003f */
	MAKNIB(1, 2),	/* MBOX_RETURN_BIOS_BLOCK_ADDR */
	MAKNIB(6, 1),	/* MBOX_WRITE_FOUR_RAM_WORDS */
	MAKNIB(2, 3)	/* MBOX_EXEC_BIOS_IOCB */
};
#define	NMBCOM	(sizeof (mbpcnt) / sizeof (mbpcnt[0]))

static int
isp_mboxcmd(isp, mbp)
	struct ispsoftc *isp;
	mbreg_t *mbp;
{
	int outparam, inparam;
	int loops;

	if (mbp->param[0] > NMBCOM) {
		printf("%s: bad command %x\n", isp->isp_name, mbp->param[0]);
		return (-1);
	}

	inparam = HINIB(mbpcnt[mbp->param[0]]);
	outparam =  LONIB(mbpcnt[mbp->param[0]]);
	if (inparam == 0 && outparam == 0) {
		printf("%s: no parameters for %x\n", isp->isp_name,
			mbp->param[0]);
		return (-1);
	}

	/*
	 * Make sure we can send some words..
	 */

	loops = MBOX_DELAY_COUNT;
	while ((ISP_READ(isp, HCCR) & HCCR_HOST_INT) != 0) {
		delay(100);
		if (--loops < 0) {
			printf("%s: isp_mboxcmd timeout #1\n", isp->isp_name);
			return (-1);
		}
	}

	/*
	 * Write input parameters
	 */
	switch (inparam) {
	case 6: ISP_WRITE(isp, INMAILBOX5, mbp->param[5]); mbp->param[5] = 0;
	case 5: ISP_WRITE(isp, INMAILBOX4, mbp->param[4]); mbp->param[4] = 0;
	case 4: ISP_WRITE(isp, INMAILBOX3, mbp->param[3]); mbp->param[3] = 0;
	case 3: ISP_WRITE(isp, INMAILBOX2, mbp->param[2]); mbp->param[2] = 0;
	case 2: ISP_WRITE(isp, INMAILBOX1, mbp->param[1]); mbp->param[1] = 0;
	case 1: ISP_WRITE(isp, INMAILBOX0, mbp->param[0]); mbp->param[0] = 0;
	}

	/*
	 * Clear semaphore on mailbox registers
	 */
	ISP_WRITE(isp, BIU_SEMA, 0);

	/*
	 * Clear RISC int condition.
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);

	/*
	 * Set Host Interrupt condition so that RISC will pick up mailbox regs.
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_SET_HOST_INT);

	/*
	 * Wait until RISC int is set
	 */
	loops = MBOX_DELAY_COUNT;
	while ((ISP_READ(isp, BIU_ISR) & BIU_ISR_RISC_INT) != 0) {
		delay(100);
		if (--loops < 0) {
			printf("%s: isp_mboxcmd timeout #2\n", isp->isp_name);
			return (-1);
		}
	}

	/*
	 * Check to make sure that the semaphore has been set.
	 */
	loops = MBOX_DELAY_COUNT;
	while ((ISP_READ(isp, BIU_SEMA) & 1) == 0) {
		delay(100);
		if (--loops < 0) {
			printf("%s: isp_mboxcmd timeout #3\n", isp->isp_name);
			return (-1);
		}
	}

	/*
	 * Make sure that the MBOX_BUSY has gone away
	 */
	loops = MBOX_DELAY_COUNT;
	while (ISP_READ(isp, OUTMAILBOX0) == MBOX_BUSY) {
		delay(100);
		if (--loops < 0) {
			printf("%s: isp_mboxcmd timeout #4\n", isp->isp_name);
			return (-1);
		}
	}


	/*
	 * Pick up output parameters.
	 */
	switch (outparam) {
	case 6: mbp->param[5] = ISP_READ(isp, OUTMAILBOX5);
	case 5: mbp->param[4] = ISP_READ(isp, OUTMAILBOX4);
	case 4: mbp->param[3] = ISP_READ(isp, OUTMAILBOX3);
	case 3: mbp->param[2] = ISP_READ(isp, OUTMAILBOX2);
	case 2: mbp->param[1] = ISP_READ(isp, OUTMAILBOX1);
	case 1: mbp->param[0] = ISP_READ(isp, OUTMAILBOX0);
	}

	/*
	 * Clear RISC int.
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);

	/*
	 * Release semaphore on mailbox registers
	 */
	ISP_WRITE(isp, BIU_SEMA, 0);
	return (0);
}

static void
isp_lostcmd(struct ispsoftc *isp, struct scsi_xfer *xs)
{
	mbreg_t mbs;
	mbs.param[0] = MBOX_GET_FIRMWARE_STATUS;
	(void) isp_mboxcmd(isp, &mbs);

	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		printf("%s: couldn't GET FIRMWARE STATUS\n", isp->isp_name);
		return;
	}
	printf("%s: lost command, %d commands active of total %d\n",
	       isp->isp_name, mbs.param[1], mbs.param[2]);
	if (xs == NULL || xs->sc_link == NULL)
		return;

	mbs.param[0] = MBOX_GET_DEV_QUEUE_STATUS;
	mbs.param[1] = xs->sc_link->target << 8 | xs->sc_link->lun;
	(void) isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		printf("%s: couldn't GET DEVICE STATUS\n", isp->isp_name);
		return;
	}
	printf("%s: lost command, target %d lun %d, State: %x\n",
	       isp->isp_name, mbs.param[1] >> 8, mbs.param[1] & 0x7,
	       mbs.param[2] & 0xff);
}
