/*	$OpenBSD: mpt.c,v 1.22 2006/02/04 19:05:00 marco Exp $	*/
/*	$NetBSD: mpt.c,v 1.4 2003/11/02 11:07:45 wiz Exp $	*/

/*
 * Copyright (c) 2000, 2001 by Greg Ansley
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
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
 */
/*
 * Additional Copyright (c) 2002 by Matthew Jacob under same license.
 */

/*
 * mpt.c:
 *
 * Generic routines for LSI Fusion adapters.
 *
 * Adapted from the FreeBSD "mpt" driver by Jason R. Thorpe for
 * Wasabi Systems, Inc.
 */

#include <sys/cdefs.h>
/* __KERNEL_RCSID(0, "$NetBSD: mpt.c,v 1.4 2003/11/02 11:07:45 wiz Exp $"); */

#include <dev/ic/mpt.h>

#define MPT_MAX_TRIES 3
#define MPT_MAX_WAIT 300000

static int	maxwait_ack = 0;
static int	maxwait_int = 0;
static int	maxwait_state = 0;

#ifdef MPT_DEBUG
int		mpt_debug = 11;
#endif /* MPT_DEBUG */

__inline u_int32_t mpt_rd_db(struct mpt_softc *);
__inline u_int32_t mpt_rd_intr(struct mpt_softc *);
int mpt_wait_db_ack(struct mpt_softc *);
int mpt_wait_db_int(struct mpt_softc *);
int mpt_wait_state(struct mpt_softc *, enum DB_STATE_BITS);
int mpt_get_iocfacts(struct mpt_softc *, MSG_IOC_FACTS_REPLY *);
int mpt_get_portfacts(struct mpt_softc *, MSG_PORT_FACTS_REPLY *);
int mpt_send_ioc_init(struct mpt_softc *, u_int32_t);
void mpt_print_header(struct mpt_softc *, char *, CONFIG_PAGE_HEADER *);
int mpt_read_config_info_mfg(struct mpt_softc *);
int mpt_read_config_info_iou(struct mpt_softc *);
int mpt_read_config_info_ioc(struct mpt_softc *);
int mpt_read_config_info_raid(struct mpt_softc *);
int mpt_read_config_info_spi(struct mpt_softc *);
int mpt_set_initial_config_spi(struct mpt_softc *);
int mpt_send_port_enable(struct mpt_softc *, int);
int mpt_send_event_request(struct mpt_softc *, int);

__inline u_int32_t
mpt_rd_db(struct mpt_softc *mpt)
{
	return mpt_read(mpt, MPT_OFFSET_DOORBELL);
}

__inline u_int32_t
mpt_rd_intr(struct mpt_softc *mpt)
{
	return mpt_read(mpt, MPT_OFFSET_INTR_STATUS);
}

/* Busy wait for a door bell to be read by IOC */
int
mpt_wait_db_ack(struct mpt_softc *mpt)
{
	int i;
	for (i=0; i < MPT_MAX_WAIT; i++) {
		if (!MPT_DB_IS_BUSY(mpt_rd_intr(mpt))) {
			maxwait_ack = i > maxwait_ack ? i : maxwait_ack;
			return MPT_OK;
		}

		DELAY(100);
	}
	return MPT_FAIL;
}

/* Busy wait for a door bell interrupt */
int
mpt_wait_db_int(struct mpt_softc *mpt)
{
	int i;
	for (i=0; i < MPT_MAX_WAIT; i++) {
		if (MPT_DB_INTR(mpt_rd_intr(mpt))) {
			maxwait_int = i > maxwait_int ? i : maxwait_int;
			return MPT_OK;
		}
		DELAY(100);
	}
	return MPT_FAIL;
}

/* Wait for IOC to transition to a give state */
void
mpt_check_doorbell(struct mpt_softc *mpt)
{
	u_int32_t db = mpt_rd_db(mpt);

	/* prepare this function for error path */
	/* if (MPT_STATE(db) != MPT_DB_STATE_RUNNING) { */
	switch (MPT_STATE(db)) {
		case MPT_DB_STATE_FAULT:
		case MPT_DB_STATE_READY:
			/* 1030 needs reset, issue IOC_INIT */
			/* FIXME */
			if (mpt_init(mpt, MPT_DB_INIT_HOST) != 0)
				panic("%s: Can't get MPT IOC operational",
				    mpt->mpt_dev.dv_xname);
			break;

		default:
			/* nothing done for now */
			break;
	}
}

/* Wait for IOC to transition to a give state */
int
mpt_wait_state(struct mpt_softc *mpt, enum DB_STATE_BITS state)
{
	int i;

	for (i = 0; i < MPT_MAX_WAIT; i++) {
		u_int32_t db = mpt_rd_db(mpt);
		if (MPT_STATE(db) == state) {
			maxwait_state = i > maxwait_state ? i : maxwait_state;
			return (MPT_OK);
		}
		DELAY(100);
	}
	return (MPT_FAIL);
}


/* Issue the reset COMMAND to the IOC */
int
mpt_soft_reset(struct mpt_softc *mpt)
{
	DNPRINTF(10, "soft reset\n");

	/* Have to use hard reset if we are not in Running state */
	if (MPT_STATE(mpt_rd_db(mpt)) != MPT_DB_STATE_RUNNING) {
		mpt_prt(mpt, "soft reset failed: device not running");
		return MPT_FAIL;
	}

	/* If door bell is in use we don't have a chance of getting
	 * a word in since the IOC probably crashed in message
	 * processing. So don't waste our time.
	 */
	if (MPT_DB_IS_IN_USE(mpt_rd_db(mpt))) {
		mpt_prt(mpt, "soft reset failed: doorbell wedged");
		return MPT_FAIL;
	}

	/* Send the reset request to the IOC */
	mpt_write(mpt, MPT_OFFSET_DOORBELL,
	    MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET << MPI_DOORBELL_FUNCTION_SHIFT);
	if (mpt_wait_db_ack(mpt) != MPT_OK) {
		mpt_prt(mpt, "soft reset failed: ack timeout");
		return MPT_FAIL;
	}

	/* Wait for the IOC to reload and come out of reset state */
	if (mpt_wait_state(mpt, MPT_DB_STATE_READY) != MPT_OK) {
		mpt_prt(mpt, "soft reset failed: device did not start running");
		return MPT_FAIL;
	}

	return MPT_OK;
}

/* This is a magic diagnostic reset that resets all the ARM
 * processors in the chip.
 */
void
mpt_hard_reset(struct mpt_softc *mpt)
{
	u_int32_t	diag0;
	int		count;

	/* This extra read comes for the Linux source
	 * released by LSI. It's function is undocumented!
	 */
	DNPRINTF(10, "hard reset\n");
	mpt_read(mpt, MPT_OFFSET_FUBAR);

	/* Enable diagnostic registers */
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_1);
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_2);
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_3);
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_4);
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_5);

	/* Diag. port is now active so we can now hit the reset bit */
	mpt_write(mpt, MPT_OFFSET_DIAGNOSTIC, MPT_DIAG_RESET_IOC);

	DELAY(10000);

	/* Disable Diagnostic Register */
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, 0xFF);

	/* Restore the config register values */
	/*   Hard resets are known to screw up the BAR for diagnostic
	     memory accesses (Mem1). */
	mpt_set_config_regs(mpt);
	if (mpt->mpt2 != NULL) {
		mpt_set_config_regs(mpt->mpt2);
	}

	/* Note that if there is no valid firmware to run, the doorbell will
	   remain in the reset state (0x00000000) */

	/* try to download firmware, if available */
	if (mpt->fw) {
		/* wait for the adapter to finish the reset */
		for (count = 0; count < 30; count++) {
			diag0 = mpt_read(mpt, MPI_DIAGNOSTIC_OFFSET);
			mpt_prt(mpt, "diag0=%08x", diag0);
			if (!(diag0 & MPI_DIAG_RESET_ADAPTER)) {
				break;
			}

			/* wait 1 second */
			DELAY(1000);
		}
		count = mpt_downloadboot(mpt);
		if (count < 0) {
			panic("firmware downloadboot failure (%d)!", count);
		}
	}
}

/*
 * Reset the IOC when needed. Try software command first then if needed
 * poke at the magic diagnostic reset. Note that a hard reset resets
 * *both* IOCs on dual function chips (FC929 && LSI1030) as well as
 * fouls up the PCI configuration registers.
 */
int
mpt_reset(struct mpt_softc *mpt)
{
	int ret;

	/* Try a soft reset */
	if ((ret = mpt_soft_reset(mpt)) != MPT_OK) {
		/* Failed; do a hard reset */
		mpt_hard_reset(mpt);

		/* Wait for the IOC to reload and come out of reset state */
		ret = mpt_wait_state(mpt, MPT_DB_STATE_READY);
		if (ret != MPT_OK) {
			mpt_prt(mpt, "failed to reset device");
		}
	}

	return ret;
}

/* Return a command buffer to the free queue */
void
mpt_free_request(struct mpt_softc *mpt, struct req_entry *req)
{
	if (req == NULL || req != &mpt->request_pool[req->index]) {
		panic("mpt_free_request bad req ptr");
		return;
	}
	if (req->debug == REQ_FREE) {
		/*
		 * XXX MU this should not happen but do not corrupt the free
		 * list if it does
		 */
		mpt_prt(mpt, "request %d already free", req->index);
		return;
	}
	req->sequence = 0;
	req->xfer = NULL;
	req->debug = REQ_FREE;
	SLIST_INSERT_HEAD(&mpt->request_free_list, req, link);
}

/* Initialize command buffer */
void
mpt_init_request(struct mpt_softc *mpt, struct req_entry *req)
{
	if (req == NULL || req != &mpt->request_pool[req->index]) {
		panic("mpt_init_request bad req ptr");
		return;
	}
	req->sequence = 0;
	req->xfer = NULL;
	req->debug = REQ_FREE;
	SLIST_INSERT_HEAD(&mpt->request_free_list, req, link);
}
/* Get a command buffer from the free queue */
struct req_entry *
mpt_get_request(struct mpt_softc *mpt)
{
	struct req_entry *req;
	req = SLIST_FIRST(&mpt->request_free_list);
	if (req != NULL) {
		if (req != &mpt->request_pool[req->index]) {
			panic("mpt_get_request: corrupted request free list");
		}
		if (req->xfer != NULL) {
			panic("mpt_get_request: corrupted request free list "
			    "(xfer)");
		}
		SLIST_REMOVE_HEAD(&mpt->request_free_list, link);
		req->debug = REQ_IN_PROGRESS;
	}
	return req;
}

/* Pass the command to the IOC */
void
mpt_send_cmd(struct mpt_softc *mpt, struct req_entry *req)
{
	req->sequence = mpt->sequence++;
	u_int32_t *pReq;

	pReq = req->req_vbuf;
	DNPRINTF(50, "%s: Send Request %d (0x%x):\n",
	    DEVNAME(mpt), req->index, req->req_pbuf);
	DNPRINTF(50, "%s: %08x %08x %08x %08x\n",
	    DEVNAME(mpt), pReq[0], pReq[1], pReq[2], pReq[3]);
	DNPRINTF(50, "%s: %08x %08x %08x %08x\n",
	    DEVNAME(mpt), pReq[4], pReq[5], pReq[6], pReq[7]);
	DNPRINTF(50, "%s: %08x %08x %08x %08x\n",
	    DEVNAME(mpt), pReq[8], pReq[9], pReq[10], pReq[11]);
	DNPRINTF(50, "%s: %08x %08x %08x %08x\n",
	    DEVNAME(mpt), pReq[12], pReq[13], pReq[14], pReq[15]);

	MPT_SYNC_REQ(mpt, req, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	req->debug = REQ_ON_CHIP;
	mpt_write(mpt, MPT_OFFSET_REQUEST_Q, (u_int32_t) req->req_pbuf);
}

/*
 * Give the reply buffer back to the IOC after we have
 * finished processing it.
 */
void
mpt_free_reply(struct mpt_softc *mpt, u_int32_t ptr)
{
	mpt_write(mpt, MPT_OFFSET_REPLY_Q, ptr);
}

/* Get a reply from the IOC */
u_int32_t
mpt_pop_reply_queue(struct mpt_softc *mpt)
{
	return mpt_read(mpt, MPT_OFFSET_REPLY_Q);
}

/*
 * Send a command to the IOC via the handshake register.
 *
 * Only done at initialization time and for certain unusual
 * commands such as device/bus reset as specified by LSI.
 */
int
mpt_send_handshake_cmd(struct mpt_softc *mpt, size_t len, void *cmd)
{
	int i;
	u_int32_t data, *data32;

	/* Check condition of the IOC */
	data = mpt_rd_db(mpt);
	if (((MPT_STATE(data) != MPT_DB_STATE_READY)	&&
	    (MPT_STATE(data) != MPT_DB_STATE_RUNNING)	&&
	    (MPT_STATE(data) != MPT_DB_STATE_FAULT))	||
	    (MPT_DB_IS_IN_USE(data))) {
		mpt_prt(mpt, "handshake aborted due to invalid doorbell state");
#ifdef MPT_DEBUG
		mpt_print_db(data);
#endif /* MPT_DEBUG */
		return(EBUSY);
	}

	/* We move things in 32 bit chunks */
	len = (len + 3) >> 2;
	data32 = cmd;

	/* Clear any left over pending doorbell interrupts */
	if (MPT_DB_INTR(mpt_rd_intr(mpt)))
		mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	/*
	 * Tell the handshake reg. we are going to send a command
	 * and how long it is going to be.
	 */
	data = (MPI_FUNCTION_HANDSHAKE << MPI_DOORBELL_FUNCTION_SHIFT) |
	    (len << MPI_DOORBELL_ADD_DWORDS_SHIFT);
	mpt_write(mpt, MPT_OFFSET_DOORBELL, data);

	/* Wait for the chip to notice */
	if (mpt_wait_db_int(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_send_handshake_cmd timeout1");
		return ETIMEDOUT;
	}

	/* Clear the interrupt */
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	if (mpt_wait_db_ack(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_send_handshake_cmd timeout2");
		return ETIMEDOUT;
	}

	/* Send the command */
	for (i = 0; i < len; i++) {
		mpt_write(mpt, MPT_OFFSET_DOORBELL, *data32++);
		if (mpt_wait_db_ack(mpt) != MPT_OK) {
			mpt_prt(mpt,
			    "mpt_send_handshake_cmd timeout! index = %d", i);
			return ETIMEDOUT;
		}
	}
	return MPT_OK;
}

/* Get the response from the handshake register */
int
mpt_recv_handshake_reply(struct mpt_softc *mpt, size_t reply_len, void *reply)
{
	int left, reply_left;
	u_int16_t *data16;
	MSG_DEFAULT_REPLY *hdr;

	/* We move things out in 16 bit chunks */
	reply_len >>= 1;
	data16 = (u_int16_t *)reply;

	hdr = (MSG_DEFAULT_REPLY *)reply;

	/* Get first word */
	if (mpt_wait_db_int(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_recv_handshake_cmd timeout1");
		return ETIMEDOUT;
	}
	*data16++ = mpt_read(mpt, MPT_OFFSET_DOORBELL) & MPT_DB_DATA_MASK;
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	/* Get Second Word */
	if (mpt_wait_db_int(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_recv_handshake_cmd timeout2");
		return ETIMEDOUT;
	}
	*data16++ = mpt_read(mpt, MPT_OFFSET_DOORBELL) & MPT_DB_DATA_MASK;
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

#ifdef MPT_DEBUG
	/* With the second word, we can now look at the length */
	if ((reply_len >> 1) != hdr->MsgLength)
		/* XXX don't remove brackets! */
		DNPRINTF(50, "%s: reply length does not match message length: "
			"got 0x%02x, expected 0x%02x\n",
			DEVNAME(mpt), hdr->MsgLength << 2, reply_len << 1);
#endif /* MPT_DEBUG */

	/* Get rest of the reply; but don't overflow the provided buffer */
	left = (hdr->MsgLength << 1) - 2;
	reply_left =  reply_len - 2;
	while (left--) {
		u_int16_t datum;

		if (mpt_wait_db_int(mpt) != MPT_OK) {
			mpt_prt(mpt, "mpt_recv_handshake_cmd timeout3");
			return ETIMEDOUT;
		}
		datum = mpt_read(mpt, MPT_OFFSET_DOORBELL);

		if (reply_left-- > 0)
			*data16++ = datum & MPT_DB_DATA_MASK;

		mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);
	}

	/* One more wait & clear at the end */
	if (mpt_wait_db_int(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_recv_handshake_cmd timeout4");
		return ETIMEDOUT;
	}
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	if ((hdr->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
#ifdef MPT_DEBUG
		if (mpt_debug > 50)
			mpt_print_reply(hdr);
#endif
		return (MPT_FAIL | hdr->IOCStatus);
	}

	return (0);
}

int
mpt_get_iocfacts(struct mpt_softc *mpt, MSG_IOC_FACTS_REPLY *freplp)
{
	MSG_IOC_FACTS f_req;
	int error;

	bzero(&f_req, sizeof f_req);
	f_req.Function = MPI_FUNCTION_IOC_FACTS;
	f_req.MsgContext =  0x12071942;
	error = mpt_send_handshake_cmd(mpt, sizeof f_req, &f_req);
	if (error)
		return(error);
	error = mpt_recv_handshake_reply(mpt, sizeof (*freplp), freplp);
	return (error);
}

int
mpt_get_portfacts(struct mpt_softc *mpt, MSG_PORT_FACTS_REPLY *freplp)
{
	MSG_PORT_FACTS f_req;
	int error;

	/* XXX: Only getting PORT FACTS for Port 0 */
	bzero(&f_req, sizeof f_req);
	f_req.Function = MPI_FUNCTION_PORT_FACTS;
	f_req.MsgContext =  0x12071943;
	error = mpt_send_handshake_cmd(mpt, sizeof f_req, &f_req);
	if (error)
		return(error);
	error = mpt_recv_handshake_reply(mpt, sizeof (*freplp), freplp);
	return (error);
}

/*
 * Send the initialization request. This is where we specify how many
 * SCSI busses and how many devices per bus we wish to emulate.
 * This is also the command that specifies the max size of the reply
 * frames from the IOC that we will be allocating.
 */
int
mpt_send_ioc_init(struct mpt_softc *mpt, u_int32_t who)
{
	int error = 0;
	MSG_IOC_INIT init;
	MSG_IOC_INIT_REPLY reply;

	bzero(&init, sizeof init);
	init.WhoInit = who;
	init.Function = MPI_FUNCTION_IOC_INIT;
	if (mpt->is_fc) {
		init.MaxDevices = 255;
	} else {
		init.MaxDevices = 16;
	}
	init.MaxBuses = 1;
	init.ReplyFrameSize = MPT_REPLY_SIZE;
	init.MsgContext = 0x12071941;

	/*
	 * If we are in a recovery mode and we uploaded the FW image,
	 * then the fw pointer is not NULL. Skip the upload a second time
	 * Set this flag if fw set for IOC.
	 */
	mpt->upload_fw = 0;

	if (mpt->fw_download_boot) {
		if (mpt->fw) {
			init.Flags = MPI_IOCINIT_FLAGS_DISCARD_FW_IMAGE;
		}
		else {
			mpt->upload_fw = 1;
		}
	}

	DNPRINTF(10, "%s: flags %d, upload_fw %d\n", DEVNAME(mpt), init.Flags,
		mpt->upload_fw);

	if ((error = mpt_send_handshake_cmd(mpt, sizeof init, &init)) != 0) {
		return(error);
	}

	error = mpt_recv_handshake_reply(mpt, sizeof reply, &reply);
	return (error);
}


/*
 * Utiltity routine to read configuration headers and pages
 */

int
mpt_read_cfg_header(struct mpt_softc *mpt, int PageType, int PageNumber,
    int PageAddress, CONFIG_PAGE_HEADER *rslt)
{
	int count;
	struct req_entry *req;
	MSG_CONFIG *cfgp;
	MSG_CONFIG_REPLY *reply;

	req = mpt_get_request(mpt);

	cfgp = req->req_vbuf;
	bzero(cfgp, sizeof *cfgp);

	cfgp->Action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfgp->Function = MPI_FUNCTION_CONFIG;
	cfgp->Header.PageNumber = (U8) PageNumber;
	cfgp->Header.PageType = (U8) PageType;
	cfgp->PageAddress = PageAddress;
	MPI_pSGE_SET_FLAGS(((SGE_SIMPLE32 *) &cfgp->PageBufferSGE),
	    (MPI_SGE_FLAGS_LAST_ELEMENT | MPI_SGE_FLAGS_END_OF_BUFFER |
	    MPI_SGE_FLAGS_SIMPLE_ELEMENT | MPI_SGE_FLAGS_END_OF_LIST));
	cfgp->MsgContext = req->index | 0x80000000;

	mpt_check_doorbell(mpt);
	mpt_send_cmd(mpt, req);
	count = 0;
	do {
		DELAY(500);
		mpt_intr(mpt);
		if (++count == 1000) {
			mpt_prt(mpt, "read_cfg_header timed out");
			return (-1);
		}
	} while (req->debug == REQ_ON_CHIP);

	reply = (MSG_CONFIG_REPLY *) MPT_REPLY_PTOV(mpt, req->sequence);
	if ((reply->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		mpt_prt(mpt, "mpt_read_cfg_header: Config Info Status %x",
		    reply->IOCStatus);
		mpt_free_reply(mpt, (req->sequence << 1));
		return (-1);
	}
	bcopy(&reply->Header, rslt, sizeof (CONFIG_PAGE_HEADER));
	mpt_free_reply(mpt, (req->sequence << 1));
	mpt_free_request(mpt, req);
	return (0);
}

#define	CFG_DATA_OFF	128

int
mpt_read_cfg_page(struct mpt_softc *mpt, int PageAddress, CONFIG_PAGE_HEADER *hdr)
{
	int count;
	struct req_entry *req;
	SGE_SIMPLE32 *se;
	MSG_CONFIG *cfgp;
	size_t amt;
	MSG_CONFIG_REPLY *reply;

	req = mpt_get_request(mpt);

	cfgp = req->req_vbuf;
	bzero(cfgp, MPT_REQUEST_AREA);
	cfgp->Action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	cfgp->Function = MPI_FUNCTION_CONFIG;
	cfgp->Header = *hdr;
	amt = (cfgp->Header.PageLength * sizeof (u_int32_t));
	cfgp->Header.PageType &= MPI_CONFIG_PAGETYPE_MASK;
	cfgp->PageAddress = PageAddress;
	se = (SGE_SIMPLE32 *) &cfgp->PageBufferSGE;
	se->Address = req->req_pbuf + CFG_DATA_OFF;
	MPI_pSGE_SET_LENGTH(se, amt);
	MPI_pSGE_SET_FLAGS(se, (MPI_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI_SGE_FLAGS_LAST_ELEMENT | MPI_SGE_FLAGS_END_OF_BUFFER |
	    MPI_SGE_FLAGS_END_OF_LIST));

	cfgp->MsgContext = req->index | 0x80000000;

	mpt_check_doorbell(mpt);
	mpt_send_cmd(mpt, req);
	count = 0;
	do {
		DELAY(500);
		mpt_intr(mpt);
		if (++count == 1000) {
			mpt_prt(mpt, "read_cfg_page timed out");
			return (-1);
		}
	} while (req->debug == REQ_ON_CHIP);

	reply = (MSG_CONFIG_REPLY *) MPT_REPLY_PTOV(mpt, req->sequence);
	if ((reply->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		mpt_prt(mpt, "mpt_read_cfg_page: Config Info Status %x",
		    reply->IOCStatus);
		mpt_free_reply(mpt, (req->sequence << 1));
		return (-1);
	}
	mpt_free_reply(mpt, (req->sequence << 1));
#if 0 /* XXXJRT */
	bus_dmamap_sync(mpt->request_dmat, mpt->request_dmap,
	    BUS_DMASYNC_POSTREAD);
#endif
	if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (CONFIG_PAGE_SCSI_PORT_0);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 1) {
		amt = sizeof (CONFIG_PAGE_SCSI_PORT_1);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 2) {
		amt = sizeof (CONFIG_PAGE_SCSI_PORT_2);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_DEVICE  &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (CONFIG_PAGE_SCSI_DEVICE_0);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_DEVICE  &&
	    cfgp->Header.PageNumber == 1) {
		amt = sizeof (CONFIG_PAGE_SCSI_DEVICE_1);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_MANUFACTURING &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (CONFIG_PAGE_MANUFACTURING_0);
	} else if (cfgp->Header.PageType ==  MPI_CONFIG_PAGETYPE_IOC &&
	    cfgp->Header.PageNumber == 2) {
		amt = sizeof (CONFIG_PAGE_IOC_2);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_RAID_VOLUME &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (CONFIG_PAGE_RAID_VOL_0);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_RAID_PHYSDISK &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (CONFIG_PAGE_RAID_PHYS_DISK_0);
	}

	bcopy(((caddr_t)req->req_vbuf)+CFG_DATA_OFF, hdr, amt);
	mpt_free_request(mpt, req);
	return (0);
}

int
mpt_write_cfg_page(struct mpt_softc *mpt, int PageAddress, CONFIG_PAGE_HEADER *hdr)
{
	int count, hdr_attr;
	struct req_entry *req;
	SGE_SIMPLE32 *se;
	MSG_CONFIG *cfgp;
	size_t amt;
	MSG_CONFIG_REPLY *reply;

	req = mpt_get_request(mpt);

	cfgp = req->req_vbuf;
	bzero(cfgp, sizeof *cfgp);

	hdr_attr = hdr->PageType & MPI_CONFIG_PAGEATTR_MASK;
	if (hdr_attr != MPI_CONFIG_PAGEATTR_CHANGEABLE &&
	    hdr_attr != MPI_CONFIG_PAGEATTR_PERSISTENT) {
		mpt_prt(mpt, "page type 0x%x not changeable",
		    hdr->PageType & MPI_CONFIG_PAGETYPE_MASK);
		return (-1);
	}
	hdr->PageType &= MPI_CONFIG_PAGETYPE_MASK;

	cfgp->Action = MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	cfgp->Function = MPI_FUNCTION_CONFIG;
	cfgp->Header = *hdr;
	amt = (cfgp->Header.PageLength * sizeof (u_int32_t));
	cfgp->PageAddress = PageAddress;

	se = (SGE_SIMPLE32 *) &cfgp->PageBufferSGE;
	se->Address = req->req_pbuf + CFG_DATA_OFF;
	MPI_pSGE_SET_LENGTH(se, amt);
	MPI_pSGE_SET_FLAGS(se, (MPI_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI_SGE_FLAGS_LAST_ELEMENT | MPI_SGE_FLAGS_END_OF_BUFFER |
	    MPI_SGE_FLAGS_END_OF_LIST | MPI_SGE_FLAGS_HOST_TO_IOC));

	cfgp->MsgContext = req->index | 0x80000000;

	if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (CONFIG_PAGE_SCSI_PORT_0);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 1) {
		amt = sizeof (CONFIG_PAGE_SCSI_PORT_1);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 2) {
		amt = sizeof (CONFIG_PAGE_SCSI_PORT_2);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_DEVICE  &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (CONFIG_PAGE_SCSI_DEVICE_0);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_DEVICE  &&
	    cfgp->Header.PageNumber == 1) {
		amt = sizeof (CONFIG_PAGE_SCSI_DEVICE_1);
	}
	bcopy(hdr, ((caddr_t)req->req_vbuf)+CFG_DATA_OFF, amt);
	/* Restore stripped out attributes */
	hdr->PageType |= hdr_attr;

	mpt_check_doorbell(mpt);
	mpt_send_cmd(mpt, req);
	count = 0;
	do {
		DELAY(500);
		mpt_intr(mpt);
		if (++count == 1000) {
			hdr->PageType |= hdr_attr;
			mpt_prt(mpt, "mpt_write_cfg_page timed out");
			return (-1);
		}
	} while (req->debug == REQ_ON_CHIP);

	reply = (MSG_CONFIG_REPLY *) MPT_REPLY_PTOV(mpt, req->sequence);
	if ((reply->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		mpt_prt(mpt, "mpt_write_cfg_page: Config Info Status %x",
		    reply->IOCStatus);
		mpt_free_reply(mpt, (req->sequence << 1));
		return (-1);
	}
	mpt_free_reply(mpt, (req->sequence << 1));

	mpt_free_request(mpt, req);
	return (0);
}

void
mpt_print_header(struct mpt_softc *mpt, char *s, CONFIG_PAGE_HEADER *phdr)
{
	DNPRINTF(10,"%s: %s %x: %x %x %x %x\n",
	    DEVNAME(mpt),
	    s,
	    phdr->PageNumber,
	    phdr->PageType,
	    phdr->PageNumber,
	    phdr->PageLength,
	    phdr->PageVersion);
}

/*
 * Read manufacturing configuration information
 */
int
mpt_read_config_info_mfg(struct mpt_softc *mpt)
{
	int rv, i;
	CONFIG_PAGE_HEADER *phdr[5] = {
		phdr[0] = &mpt->mpt_mfg_page0.Header,
		phdr[1] = &mpt->mpt_mfg_page1.Header,
		phdr[2] = &mpt->mpt_mfg_page2.Header,
		phdr[3] = &mpt->mpt_mfg_page3.Header,
		phdr[4] = &mpt->mpt_mfg_page4.Header
	};

	for (i = 0; i < 5 /* 5 pages total */; i++) {
		/* retrieve MFG headers */
		rv = mpt_read_cfg_header(mpt,
		    MPI_CONFIG_PAGETYPE_MANUFACTURING, i, 0, phdr[i]);
		if (rv) {
			mpt_prt(mpt, "Could not retrieve Manufacturing Page "
			    "%i Header.", i);
			return (-1);
		} else
			mpt_print_header(mpt, "Manufacturing Header Page",
			    phdr[i]);

		/* retrieve MFG config pages using retrieved headers */
		rv = mpt_read_cfg_page(mpt, i, phdr[i]);
		if (rv) {
			mpt_prt(mpt, "Could not retrieve manufacturing Page"
			    " %i", i);
			return (-1);
		}
	}

#ifdef MPT_DEBUG
	if (mpt_debug > 10) {
		mpt_prt(mpt, "Manufacturing Page 0 data: %s %s %s %s %s",
		    mpt->mpt_mfg_page0.ChipName,
		    mpt->mpt_mfg_page0.ChipRevision,
		    mpt->mpt_mfg_page0.BoardName,
		    mpt->mpt_mfg_page0.BoardAssembly,
		    mpt->mpt_mfg_page0.BoardTracerNumber);

		mpt_prt(mpt, "Manufacturing Page 1 data:");
		for (i = 0;
		    i < ((mpt->mpt_mfg_page1.Header.PageLength - 1)<< 2); i++) {
			printf("%02x ", mpt->mpt_mfg_page1.VPD[i]);
		}
		printf("\n");

		mpt_prt(mpt, "Manufacturing Page 2 data: %x %x",
		    mpt->mpt_mfg_page2.ChipId.PCIRevisionID,
		    mpt->mpt_mfg_page2.ChipId.DeviceID);
		for (i = 0;
		    i < (mpt->mpt_mfg_page2.Header.PageLength - 2); i++) {
			printf("%08x ", mpt->mpt_mfg_page2.HwSettings[i]);
		}
		printf("\n");

		mpt_prt(mpt, "Manufacturing Page 3 data: %x %x",
		    mpt->mpt_mfg_page3.ChipId.PCIRevisionID,
		    mpt->mpt_mfg_page3.ChipId.DeviceID);
		for (i = 0;
		    i < (mpt->mpt_mfg_page3.Header.PageLength - 2); i++) {
			printf("%08x ", mpt->mpt_mfg_page3.Info[i]);
		}
		printf("\n");

		mpt_prt(mpt, "Manufacturing Page 4 data: %x %x %x %x %x",
		    mpt->mpt_mfg_page4.InfoSize1,
		    mpt->mpt_mfg_page4.InfoOffset1,
		    mpt->mpt_mfg_page4.InfoSize0,
		    mpt->mpt_mfg_page4.InfoOffset0,
		    mpt->mpt_mfg_page4.InquirySize,
		    mpt->mpt_mfg_page4.ISVolumeSettings,
		    mpt->mpt_mfg_page4.IMEVolumeSettings,
		    mpt->mpt_mfg_page4.IMVolumeSettings);
		for (i = 0; i < sizeof(mpt->mpt_mfg_page4.InquiryData); i++) {
			printf("%02x ", mpt->mpt_mfg_page4.InquiryData[i]);
		}
		printf("\n");
	}
#endif /* MPT_DEBUG */

	return (0);
}

/*
 * Read IO Unit configuration information
 */
int
mpt_read_config_info_iou(struct mpt_softc *mpt)
{
	int rv, i;

	CONFIG_PAGE_HEADER *phdr[4] = {
		phdr[0] = &mpt->mpt_iou_page0.Header,
		phdr[1] = &mpt->mpt_iou_page1.Header,
		phdr[2] = &mpt->mpt_iou_page2.Header,
		phdr[3] = &mpt->mpt_iou_page2.Header,
	};

	for (i = 0; i < 4 /* 4 pages total */; i++) {
		/* retrieve IO Unit headers */
		rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_IO_UNIT, i,
		    0, phdr[i]);
		if (rv) {
			mpt_prt(mpt, "Could not retrieve IO Unit Page %i "
			    "header.", i);
			return (-1);
		} else
			mpt_print_header(mpt, "IO Unit Header Page", phdr[i]);

		/* retrieve IO Unit config pages using retrieved headers */
		rv = mpt_read_cfg_page(mpt, i, phdr[i]);
		if (rv) {
			mpt_prt(mpt, "Could not retrieve IO Unit Page %i", i);
			return (-1);
		}

		if (i == 0 &&
		    mpt->mpt_iou_page0.UniqueValue.High == 0xdeadbeef &&
		    mpt->mpt_iou_page0.UniqueValue.Low == 0xdeadbeef)
		{
			/* vmware returns a lot of dead beef in page 0 */
			mpt->vmware = 1;
		}
	}

	DNPRINTF(10, "%s: IO Unit Page 0 data: %llx\n",
	    DEVNAME(mpt),
	    mpt->mpt_iou_page0.UniqueValue);
	DNPRINTF(10, "%s: IO Unit Page 1 data: %x\n",
	    DEVNAME(mpt),
	    mpt->mpt_iou_page1.Flags);
	DNPRINTF(10, "%s: IO Unit Page 2 data: %x %x %x %x %x %x\n",
	    DEVNAME(mpt),
	    mpt->mpt_iou_page2.Flags,
	    mpt->mpt_iou_page2.BiosVersion,
	    mpt->mpt_iou_page2.AdapterOrder[0],
	    mpt->mpt_iou_page2.AdapterOrder[1],
	    mpt->mpt_iou_page2.AdapterOrder[2],
	    mpt->mpt_iou_page2.AdapterOrder[3]);

	return (0);
}

/*
 * Read IOC configuration information
 */
int
mpt_read_config_info_ioc(struct mpt_softc *mpt)
{
	int rv, i;
	CONFIG_PAGE_HEADER *phdr[5] = {
		phdr[0] = &mpt->mpt_ioc_page0.Header,
		phdr[1] = &mpt->mpt_ioc_page1.Header,
		phdr[2] = &mpt->mpt_ioc_page2.Header,
		phdr[3] = &mpt->mpt_ioc_page3.Header,
		phdr[4] = &mpt->mpt_ioc_page4.Header
	};

	for (i = 0; i < 5 /* 5 pages total */; i++) {
		/* retrieve IOC headers */
		rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_IOC, i,
		    0, phdr[i]);
		if (rv) {
			mpt_prt(mpt, "Could not retrieve IOC Page %i header.",
			   i);
			return (-1);
		} else
			mpt_print_header(mpt, "IOC Header Page", phdr[i]);

		/* retrieve IOC config pages using retrieved headers */
		rv = mpt_read_cfg_page(mpt, i, phdr[i]);
		if (rv) {
			mpt_prt(mpt, "Could not retrieve IOC Page %i", i);
			return (-1);
		}
	}

#ifdef MPT_DEBUG
	if (mpt_debug > 10) {
		mpt_prt(mpt, "IOC Page 0 data: %x %x %x %x %x %x %x %x",
		    mpt->mpt_ioc_page0.TotalNVStore,
		    mpt->mpt_ioc_page0.FreeNVStore,
		    mpt->mpt_ioc_page0.DeviceID,
		    mpt->mpt_ioc_page0.VendorID,
		    mpt->mpt_ioc_page0.RevisionID,
		    mpt->mpt_ioc_page0.ClassCode,
		    mpt->mpt_ioc_page0.SubsystemID,
		    mpt->mpt_ioc_page0.SubsystemVendorID
		);

		mpt_prt(mpt, "IOC Page 1 data: %x %x %x",
		    mpt->mpt_ioc_page1.Flags,
		    mpt->mpt_ioc_page1.CoalescingTimeout,
		    mpt->mpt_ioc_page1.CoalescingDepth);

		mpt_prt(mpt, "IOC Page 2 data: %x %x %x %x %x",
		    mpt->mpt_ioc_page2.CapabilitiesFlags,
		    mpt->mpt_ioc_page2.MaxPhysDisks,
		    mpt->mpt_ioc_page2.NumActivePhysDisks,
		    mpt->mpt_ioc_page2.MaxVolumes,
		    mpt->mpt_ioc_page2.NumActiveVolumes);

		/* FIXME: move this to attach */
		if (mpt->mpt_ioc_page2.MaxVolumes >
		    MPI_IOC_PAGE_2_RAID_VOLUME_MAX) {
		    /* complain */
		}
		for (i = 0; i < mpt->mpt_ioc_page2.MaxVolumes; i++) {
			mpt_prt(mpt, "IOC Page 2 RAID Volume %x %x %x %x %x",
			mpt->mpt_ioc_page2.RaidVolume[i].VolumeType,
			mpt->mpt_ioc_page2.RaidVolume[i].VolumePageNumber,
			mpt->mpt_ioc_page2.RaidVolume[i].VolumeIOC,
			mpt->mpt_ioc_page2.RaidVolume[i].VolumeBus,
			mpt->mpt_ioc_page2.RaidVolume[i].VolumeID);
		}

		mpt_prt(mpt, "IOC Page 3 data: %x ",
			mpt->mpt_ioc_page3.NumPhysDisks);

		for (i = 0; i < mpt->mpt_ioc_page3.NumPhysDisks; i++) {
			mpt_prt(mpt, "IOC Page 3 Physical Disk: %x %x %x %x",
			    mpt->mpt_ioc_page3.PhysDisk[i].PhysDiskNum,
			    mpt->mpt_ioc_page3.PhysDisk[i].PhysDiskIOC,
			    mpt->mpt_ioc_page3.PhysDisk[i].PhysDiskBus,
			    mpt->mpt_ioc_page3.PhysDisk[i].PhysDiskID);
		}

		mpt_prt(mpt, "IOC Page 4 data: %x %x",
			mpt->mpt_ioc_page4.MaxSEP,
			mpt->mpt_ioc_page4.ActiveSEP);

		for (i = 0; i < mpt->mpt_ioc_page4.MaxSEP; i++) {
			mpt_prt(mpt, "IOC Page 4 SEP: %x %x",
			    mpt->mpt_ioc_page4.SEP[i].SEPTargetID,
			    mpt->mpt_ioc_page4.SEP[i].SEPBus);
		}
	}
#endif /* MPT_DEBUG */

	return (0);
}

/*
 * Read RAID Volume pages
 */
int
mpt_read_config_info_raid(struct mpt_softc *mpt)
{
	int rv;

	/* retrieve raid volume headers */
	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_RAID_VOLUME, 0,
	    0, &mpt->mpt_raidvol_page0.Header);
	if (rv) {
		mpt_prt(mpt, "Could not retrieve RAID Volume Page 0 Header");
		return (-1);
	} else
		mpt_print_header(mpt, "RAID Volume Header Page",
		    &mpt->mpt_raidvol_page0.Header);

	/* retrieve raid volume page using retrieved headers */
	rv = mpt_read_cfg_page(mpt, 0, &mpt->mpt_raidvol_page0.Header);
	if (rv) {
		mpt_prt(mpt, "Could not retrieve RAID Volume Page 0");
		return (-1);
	}

	/* retrieve raid physical disk header */
	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_RAID_PHYSDISK, 0,
	    0, &mpt->mpt_raidphys_page0.Header);
	if (rv) {
		mpt_prt(mpt, "Could not retrieve RAID Phys Disk Page 0 Header");
		return (-1);
	} else
		mpt_print_header(mpt, "RAID Volume Physical Disk Page",
		    &mpt->mpt_raidphys_page0.Header);

	/* retrieve raid physical disk page using retrieved headers */
	rv = mpt_read_cfg_page(mpt, 0, &mpt->mpt_raidphys_page0.Header);
	if (rv) {
		mpt_prt(mpt, "Could not retrieve RAID Phys Disk Page 0");
		return (-1);
	}

#ifdef MPT_DEBUG
	int i;
	if (mpt_debug > 10) {
		mpt_prt(mpt, "RAID Volume Page 0 data: %x %x %x %x %x"
		    "%x %x %x %x",
		    mpt->mpt_raidvol_page0.VolumeType,
		    mpt->mpt_raidvol_page0.VolumeIOC,
		    mpt->mpt_raidvol_page0.VolumeBus,
		    mpt->mpt_raidvol_page0.VolumeID,
		    mpt->mpt_raidvol_page0.VolumeStatus,
		    mpt->mpt_raidvol_page0.VolumeSettings,
		    mpt->mpt_raidvol_page0.MaxLBA,
		    mpt->mpt_raidvol_page0.StripeSize,
		    mpt->mpt_raidvol_page0.NumPhysDisks);

		for (i = 0; i < mpt->mpt_raidvol_page0.NumPhysDisks; i++) {
			mpt_prt(mpt, "RAID Volume Page 0 Physical Disk: %x %x",
			    mpt->mpt_raidvol_page0.PhysDisk[i].PhysDiskNum,
			    mpt->mpt_raidvol_page0.PhysDisk[i].PhysDiskMap);
		}
		printf("\n");

		mpt_prt(mpt, "RAID Phyical Disk Page 0 data: %x %x %x %x %x"
		    "%x %x %x",
		    mpt->mpt_raidphys_page0.PhysDiskNum,
		    mpt->mpt_raidphys_page0.PhysDiskIOC,
		    mpt->mpt_raidphys_page0.PhysDiskBus,
		    mpt->mpt_raidphys_page0.PhysDiskID,
		    mpt->mpt_raidphys_page0.PhysDiskSettings.SepID,
		    mpt->mpt_raidphys_page0.PhysDiskSettings.SepBus,
		    mpt->mpt_raidphys_page0.PhysDiskSettings.HotSparePool,
		    mpt->mpt_raidphys_page0.PhysDiskSettings.PhysDiskSettings);

		for (i = 0;
		    i < sizeof(mpt->mpt_raidphys_page0.DiskIdentifier); i++) {
			printf("%02x ",
			    mpt->mpt_raidphys_page0.DiskIdentifier[i]);
		}

		/* does them all */
		printf("\n");
		mpt_prt(mpt, "RAID Phyical Disk Page 0 data: %s",
		    mpt->mpt_raidphys_page0.InquiryData.VendorID);

		for (i = 0;
		    i < sizeof(mpt->mpt_raidphys_page0.InquiryData.Info); i++) {
			printf("%02x ",
			    mpt->mpt_raidphys_page0.InquiryData.Info[i]);
		}

		printf("\n");
		mpt_prt(mpt, "RAID Phyical Disk Page 0 data: %x %x %x"
		    "%x %x %x %x %x %x %x %x",
		    mpt->mpt_raidphys_page0.PhysDiskStatus.Flags,
		    mpt->mpt_raidphys_page0.PhysDiskStatus.State,
		    mpt->mpt_raidphys_page0.MaxLBA,
		    mpt->mpt_raidphys_page0.ErrorData.ErrorSenseKey,
		    mpt->mpt_raidphys_page0.ErrorData.ErrorCdbByte,
		    mpt->mpt_raidphys_page0.ErrorData.ErrorASCQ,
		    mpt->mpt_raidphys_page0.ErrorData.ErrorASC,
		    mpt->mpt_raidphys_page0.ErrorData.ErrorCount,
		    mpt->mpt_raidphys_page0.ErrorData.SmartASCQ,
		    mpt->mpt_raidphys_page0.ErrorData.SmartASC,
		    mpt->mpt_raidphys_page0.ErrorData.SmartCount
		    );
	}
#endif /* MPT_DEBUG */

	return (0);
}

/*
 * Read SCSI configuration information
 */
int
mpt_read_config_info_spi(struct mpt_softc *mpt)
{
	int rv, i;

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_PORT, 0,
	    0, &mpt->mpt_port_page0.Header);
	if (rv)
		return (-1);

	DNPRINTF(10, "%s: SPI Port Page 0 Header: %x %x %x %x\n",
	    DEVNAME(mpt),
	    mpt->mpt_port_page0.Header.PageVersion,
	    mpt->mpt_port_page0.Header.PageLength,
	    mpt->mpt_port_page0.Header.PageNumber,
	    mpt->mpt_port_page0.Header.PageType);

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_PORT, 1,
	    0, &mpt->mpt_port_page1.Header);
	if (rv)
		return (-1);

	DNPRINTF(10, "%s: SPI Port Page 1 Header: %x %x %x %x\n",
	    DEVNAME(mpt),
	    mpt->mpt_port_page1.Header.PageVersion,
	    mpt->mpt_port_page1.Header.PageLength,
	    mpt->mpt_port_page1.Header.PageNumber,
	    mpt->mpt_port_page1.Header.PageType);

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_PORT, 2,
	    0, &mpt->mpt_port_page2.Header);
	if (rv)
		return (-1);

	DNPRINTF(10, "%s: SPI Port Page 2 Header: %x %x %x %x\n",
	    DEVNAME(mpt),
	    mpt->mpt_port_page1.Header.PageVersion,
	    mpt->mpt_port_page1.Header.PageLength,
	    mpt->mpt_port_page1.Header.PageNumber,
	    mpt->mpt_port_page1.Header.PageType);

	for (i = 0; i < 16; i++) {
		rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_DEVICE,
		    0, i, &mpt->mpt_dev_page0[i].Header);
		if (rv) {
			return (-1);
		}
		DNPRINTF(10,
		    "%s: SPI Target %d Device Page 0 Header: %x %x %x %x\n",
		    DEVNAME(mpt),
		    i, mpt->mpt_dev_page0[i].Header.PageVersion,
		    mpt->mpt_dev_page0[i].Header.PageLength,
		    mpt->mpt_dev_page0[i].Header.PageNumber,
		    mpt->mpt_dev_page0[i].Header.PageType);

		rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_DEVICE,
		    1, i, &mpt->mpt_dev_page1[i].Header);
		if (rv) {
			return (-1);
		}
		DNPRINTF(10,
		    "%s: SPI Target %d Device Page 1 Header: %x %x %x %x\n",
		    DEVNAME(mpt),
		    i, mpt->mpt_dev_page1[i].Header.PageVersion,
		    mpt->mpt_dev_page1[i].Header.PageLength,
		    mpt->mpt_dev_page1[i].Header.PageNumber,
		    mpt->mpt_dev_page1[i].Header.PageType);
	}

	/*
	 * At this point, we don't *have* to fail. As long as we have
	 * valid config header information, we can (barely) lurch
	 * along.
	 */

	rv = mpt_read_cfg_page(mpt, 0, &mpt->mpt_port_page0.Header);
	if (rv) {
		mpt_prt(mpt, "failed to read SPI Port Page 0");
	} else
		DNPRINTF(10,
		    "%s: SPI Port Page 0: Capabilities %x PhysicalInterface %x\n",
		    DEVNAME(mpt),
		    mpt->mpt_port_page0.Capabilities,
		    mpt->mpt_port_page0.PhysicalInterface);

	rv = mpt_read_cfg_page(mpt, 0, &mpt->mpt_port_page1.Header);
	if (rv) {
		mpt_prt(mpt, "failed to read SPI Port Page 1");
	} else
		DNPRINTF(10,
		    "%s: SPI Port Page 1: Configuration %x OnBusTimerValue %x\n",
		    DEVNAME(mpt),
		    mpt->mpt_port_page1.Configuration,
		    mpt->mpt_port_page1.OnBusTimerValue);

	rv = mpt_read_cfg_page(mpt, 0, &mpt->mpt_port_page2.Header);
	if (rv) {
		mpt_prt(mpt, "failed to read SPI Port Page 2");
	} else {
		DNPRINTF(10,
		    "%s: SPI Port Page 2: Flags %x Settings %x\n",
		    DEVNAME(mpt),
		    mpt->mpt_port_page2.PortFlags,
		    mpt->mpt_port_page2.PortSettings);
		for (i = 0; i < 16; i++)
			DNPRINTF(10,
			    "%s: SPI Port Page 2 Tgt %d: timo %x SF %x Flags %x\n",
			    DEVNAME(mpt),
			    i, mpt->mpt_port_page2.DeviceSettings[i].Timeout,
			    mpt->mpt_port_page2.DeviceSettings[i].SyncFactor,
			    mpt->mpt_port_page2.DeviceSettings[i].DeviceFlags);
	}

	for (i = 0; i < 16; i++) {
		rv = mpt_read_cfg_page(mpt, i, &mpt->mpt_dev_page0[i].Header);
		if (rv) {
			mpt_prt(mpt, "cannot read SPI Tgt %d Device Page 0", i);
			continue;
		}
		DNPRINTF(10,
		    "%s: SPI Tgt %d Page 0: NParms %x Information %x\n",
		    DEVNAME(mpt),
		    i, mpt->mpt_dev_page0[i].NegotiatedParameters,
		    mpt->mpt_dev_page0[i].Information);

		rv = mpt_read_cfg_page(mpt, i, &mpt->mpt_dev_page1[i].Header);
		if (rv) {
			mpt_prt(mpt, "cannot read SPI Tgt %d Device Page 1", i);
			continue;
		}

		DNPRINTF(10,
		    "%s: SPI Tgt %d Page 1: RParms %x Configuration %x\n",
		    DEVNAME(mpt),
		    i, mpt->mpt_dev_page1[i].RequestedParameters,
		    mpt->mpt_dev_page1[i].Configuration);
	}
	return (0);
}

/*
 * Validate SPI configuration information.
 *
 * In particular, validate SPI Port Page 1.
 */
int
mpt_set_initial_config_spi(struct mpt_softc *mpt)
{
	int i, pp1val = ((1 << mpt->mpt_ini_id) << 16) | mpt->mpt_ini_id;

	mpt->mpt_disc_enable = 0xff;
	mpt->mpt_tag_enable = 0;

	if (mpt->mpt_port_page1.Configuration != pp1val) {
		CONFIG_PAGE_SCSI_PORT_1 tmp;
		mpt_prt(mpt,
		    "SPI Port Page 1 Config value bad (%x)- should be %x",
		    mpt->mpt_port_page1.Configuration, pp1val);
		tmp = mpt->mpt_port_page1;
		tmp.Configuration = pp1val;
		if (mpt_write_cfg_page(mpt, 0, &tmp.Header)) {
			return (-1);
		}
		if (mpt_read_cfg_page(mpt, 0, &tmp.Header)) {
			return (-1);
		}
		if (tmp.Configuration != pp1val) {
			mpt_prt(mpt,
			    "failed to reset SPI Port Page 1 Config value");
			return (-1);
		}
		mpt->mpt_port_page1 = tmp;
	}

	for (i = 0; i < 16; i++) {
		CONFIG_PAGE_SCSI_DEVICE_1 tmp;
		tmp = mpt->mpt_dev_page1[i];
		tmp.RequestedParameters = 0;
		tmp.Configuration = 0;
		DNPRINTF(10,
		    "%s: Set Tgt %d SPI DevicePage 1 values to %x 0 %x\n",
		    DEVNAME(mpt),
		    i, tmp.RequestedParameters, tmp.Configuration);

		if (mpt_write_cfg_page(mpt, i, &tmp.Header))
			return (-1);

		if (mpt_read_cfg_page(mpt, i, &tmp.Header))
			return (-1);

		mpt->mpt_dev_page1[i] = tmp;
		DNPRINTF(10,
		    "%s: SPI Tgt %d Page 1: RParm %x Configuration %x\n",
		    DEVNAME(mpt),
		    i,
		    mpt->mpt_dev_page1[i].RequestedParameters,
		    mpt->mpt_dev_page1[i].Configuration);
	}
	return (0);
}

/*
 * Enable IOC port
 */
int
mpt_send_port_enable(struct mpt_softc *mpt, int port)
{
	int count;
	struct req_entry *req;
	MSG_PORT_ENABLE *enable_req;

	req = mpt_get_request(mpt);

	enable_req = req->req_vbuf;
	bzero(enable_req, sizeof *enable_req);

	enable_req->Function   = MPI_FUNCTION_PORT_ENABLE;
	enable_req->MsgContext = req->index | 0x80000000;
	enable_req->PortNumber = port;

	mpt_check_doorbell(mpt);
	DNPRINTF(10, "%s: enabling port %d\n", DEVNAME(mpt), port);

	mpt_send_cmd(mpt, req);

	count = 0;
	do {
		DELAY(500);
		mpt_intr(mpt);
		if (++count == 100000) {
			mpt_prt(mpt, "port enable timed out");
			return (-1);
		}
	} while (req->debug == REQ_ON_CHIP);
	mpt_free_request(mpt, req);
	return (0);
}

/*
 * Enable/Disable asynchronous event reporting.
 *
 * NB: this is the first command we send via shared memory
 * instead of the handshake register.
 */
int
mpt_send_event_request(struct mpt_softc *mpt, int onoff)
{
	struct req_entry *req;
	MSG_EVENT_NOTIFY *enable_req;

	req = mpt_get_request(mpt);

	enable_req = req->req_vbuf;
	bzero(enable_req, sizeof *enable_req);

	enable_req->Function   = MPI_FUNCTION_EVENT_NOTIFICATION;
	enable_req->MsgContext = req->index | 0x80000000;
	enable_req->Switch     = onoff;

	mpt_check_doorbell(mpt);
	DNPRINTF(10, "%s: %sabling async events\n",
	    DEVNAME(mpt), onoff? "en" : "dis");
	mpt_send_cmd(mpt, req);

	return (0);
}

/*
 * Un-mask the interrupts on the chip.
 */
void
mpt_enable_ints(struct mpt_softc *mpt)
{
	/* Unmask every thing except door bell int */
	mpt_write(mpt, MPT_OFFSET_INTR_MASK, MPT_INTR_DB_MASK);
}

/*
 * Mask the interrupts on the chip.
 */
void
mpt_disable_ints(struct mpt_softc *mpt)
{
	/* Mask all interrupts */
	mpt_write(mpt, MPT_OFFSET_INTR_MASK,
	    MPT_INTR_REPLY_MASK | MPT_INTR_DB_MASK);
}

/* (Re)Initialize the chip for use */
int
mpt_init(struct mpt_softc *mpt, u_int32_t who)
{
	int try;
	MSG_IOC_FACTS_REPLY facts;
	MSG_PORT_FACTS_REPLY pfp;
	u_int32_t pptr;
	int val;

	/* Put all request buffers (back) on the free list */
	SLIST_INIT(&mpt->request_free_list);
	for (val = 0; val < MPT_MAX_REQUESTS(mpt); val++) {
		mpt_init_request(mpt, &mpt->request_pool[val]);
	}

	DNPRINTF(10, "%s: doorbell req = %s\n",
	    DEVNAME(mpt),
	    mpt_ioc_diag(mpt_read(mpt, MPT_OFFSET_DOORBELL)));

	/*
	 * Start by making sure we're not at FAULT or RESET state
	 */
	switch (mpt_rd_db(mpt) & MPT_DB_STATE_MASK) {
	case MPT_DB_STATE_RESET:
	case MPT_DB_STATE_FAULT:
		if (mpt_reset(mpt) != MPT_OK) {
			return (EIO);
		}
	default:
		break;
	}

	for (try = 0; try < MPT_MAX_TRIES; try++) {
		/*
		 * No need to reset if the IOC is already in the READY state.
		 *
		 * Force reset if initialization failed previously.
		 * Note that a hard_reset of the second channel of a '929
		 * will stop operation of the first channel.  Hopefully, if the
		 * first channel is ok, the second will not require a hard
		 * reset.
		 */
		if ((mpt_rd_db(mpt) & MPT_DB_STATE_MASK) !=
		    MPT_DB_STATE_READY) {
			if (mpt_reset(mpt) != MPT_OK) {
				DELAY(10000);
				continue;
			}
		}

		if (mpt_get_iocfacts(mpt, &facts) != MPT_OK) {
			mpt_prt(mpt, "mpt_get_iocfacts failed");
			continue;
		}

		DNPRINTF(10,
		    "%s: IOCFACTS: GlobalCredits=%d BlockSize=%u "
		    "Request Frame Size %u\n",
		    DEVNAME(mpt), facts.GlobalCredits,
		    facts.BlockSize, facts.RequestFrameSize);

		mpt->mpt_global_credits = facts.GlobalCredits;
		mpt->request_frame_size = facts.RequestFrameSize;

		/* save the firmware upload required flag */
		mpt->fw_download_boot = facts.Flags
			& MPI_IOCFACTS_FLAGS_FW_DOWNLOAD_BOOT;

		mpt->fw_image_size = facts.FWImageSize;

		if (mpt_get_portfacts(mpt, &pfp) != MPT_OK) {
			mpt_prt(mpt, "mpt_get_portfacts failed");
			continue;
		}

		DNPRINTF(10,
		    "%s: PORTFACTS: Type %x PFlags %x IID %d MaxDev %d\n",
		    DEVNAME(mpt),
		    pfp.PortType, pfp.ProtocolFlags, pfp.PortSCSIID,
		    pfp.MaxDevices);

		if (pfp.PortType != MPI_PORTFACTS_PORTTYPE_SCSI &&
		    pfp.PortType != MPI_PORTFACTS_PORTTYPE_FC) {
			mpt_prt(mpt, "Unsupported Port Type (%x)",
			    pfp.PortType);
			return (ENXIO);
		}
		if (!(pfp.ProtocolFlags & MPI_PORTFACTS_PROTOCOL_INITIATOR)) {
			mpt_prt(mpt, "initiator role unsupported");
			return (ENXIO);
		}
		if (pfp.PortType == MPI_PORTFACTS_PORTTYPE_FC) {
			mpt->is_fc = 1;
		} else {
			mpt->is_fc = 0;
		}
		mpt->mpt_ini_id = pfp.PortSCSIID;

		if (mpt_send_ioc_init(mpt, who) != MPT_OK) {
			mpt_prt(mpt, "mpt_send_ioc_init failed");
			continue;
		}

		DNPRINTF(10, "%s: mpt_send_ioc_init ok\n", DEVNAME(mpt));

		if (mpt_wait_state(mpt, MPT_DB_STATE_RUNNING) != MPT_OK) {
			mpt_prt(mpt, "IOC failed to go to run state");
			continue;
		}

		DNPRINTF(10, "%s: IOC now at RUNSTATE\n", DEVNAME(mpt));

		/*
		 * Give it reply buffers
		 *
		 * Do *not* exceed global credits.
		 */
		for (val = 0, pptr = mpt->reply_phys;
		    (pptr + MPT_REPLY_SIZE) < (mpt->reply_phys + PAGE_SIZE);
		    pptr += MPT_REPLY_SIZE) {
			mpt_free_reply(mpt, pptr);
			if (++val == mpt->mpt_global_credits - 1)
				break;
		}

		/* XXX MU correct place the call to fw_upload? */
		if (mpt->upload_fw) {
			DNPRINTF(10, "%s: firmware upload required\n",
			    DEVNAME(mpt));

			if (mpt_do_upload(mpt))
				/* XXX MP should we panic? */
				mpt_prt(mpt, "firmware upload failure");
		}
		else
			DNPRINTF(10, "%sfirmware upload not required\n",
			    DEVNAME(mpt));

		/*
		 * Enable asynchronous event reporting
		 */
		mpt_send_event_request(mpt, 1);


		/*
		 * Read set up initial configuration information
		 * (SPI only for now)
		 */

		if (mpt->is_fc == 0) {
			if (mpt_read_config_info_spi(mpt))
				return (EIO);

			if (mpt_set_initial_config_spi(mpt))
				return (EIO);
		}

		/*
		 * Read IO Unit pages
		 */
		if (mpt_read_config_info_iou(mpt)) {
			mpt_prt(mpt, "could not retrieve IO Unit pages");
			return (EIO);
		}

		/*
		 * if the vmware flag is set skip page retrival since it does
		 * not support all of them.
		 */
		if (mpt->vmware)
			DNPRINTF(10, "%s: running in vmware, skipping page "
			    "retrieval\n", DEVNAME(mpt));
		else {
			/*
			 * Read manufacturing pages
			 */
			if (mpt_read_config_info_mfg(mpt)) {
				mpt_prt(mpt, "could not retrieve manufacturing"
				    "pages");
				return (EIO);
			}

			/*
			 * Read IOC pages
			 */
			if (mpt_read_config_info_ioc(mpt)) {
				mpt_prt(mpt, "could not retrieve IOC pages");
				return (EIO);
			}
			mpt->im_support = mpt->mpt_ioc_page2.CapabilitiesFlags &
				(MPI_IOCPAGE2_CAP_FLAGS_IS_SUPPORT |
				 MPI_IOCPAGE2_CAP_FLAGS_IME_SUPPORT |
				 MPI_IOCPAGE2_CAP_FLAGS_IM_SUPPORT);

			/*
			 * Read RAID pages if we have IM/IME/IS volumes
			 */
			if (mpt->mpt_ioc_page2.NumActiveVolumes) {
				if (mpt_read_config_info_raid(mpt)) {
					mpt_prt(mpt, "could not retrieve RAID"
					    "pages");
					return (EIO);
				}
			}
		}

		/*
		 * Now enable the port
		 */
		if (mpt_send_port_enable(mpt, 0) != MPT_OK) {
			mpt_prt(mpt, "failed to enable port 0");
			continue;
		}

		DNPRINTF(10, "%s: enabled port 0\n", DEVNAME(mpt));

		/* Everything worked */
		break;
	}

	if (try >= MPT_MAX_TRIES) {
		mpt_prt(mpt, "failed to initialize IOC");
		return (EIO);
	}

	DNPRINTF(10, "%s: enabling interrupts\n", DEVNAME(mpt));

	mpt_enable_ints(mpt);
	return (0);
}

/*
 * mpt_do_upload - create and send FWUpload request to MPT adapter port.
 *
 * Returns 0 for success, error for failure
 */
int
mpt_do_upload(struct mpt_softc *mpt)
{
	u_int8_t        request[MPT_RQSL(mpt)];
	FWUploadReply_t reply;
	FWUpload_t      *prequest;
	FWUploadReply_t *preply;
	FWUploadTCSGE_t *ptcsge = NULL;
	SGE_SIMPLE32    *se;
	int             maxsgl;
	int             sgeoffset;
	int             i, error;
	uint32_t        flags;

	if (mpt->fw_image_size == 0 || mpt->fw != NULL) {
		return 0;
	}

	/* compute the maximum number of elements in the SG list */
	maxsgl = (MPT_RQSL(mpt) - sizeof(MSG_FW_UPLOAD) +
		sizeof(SGE_MPI_UNION) - sizeof(FWUploadTCSGE_t))
		/ sizeof(SGE_SIMPLE32);

	error = mpt_alloc_fw_mem(mpt, maxsgl);
	if (error) {
		mpt_prt(mpt,"mpt_alloc_fw_mem error: %d", error);
		return error;
	}

	if (mpt->fw_dmap->dm_nsegs > maxsgl) {
		mpt_prt(mpt,"nsegs > maxsgl");
		return 1; /* XXX */
	}

	prequest = (FWUpload_t *)&request;
	preply = (FWUploadReply_t *)&reply;

	memset(prequest, 0, MPT_RQSL(mpt));
	memset(preply, 0, sizeof(reply));

	prequest->ImageType = MPI_FW_UPLOAD_ITYPE_FW_IOC_MEM;
	prequest->Function = MPI_FUNCTION_FW_UPLOAD;
	prequest->MsgContext = 0;

	ptcsge = (FWUploadTCSGE_t *) &prequest->SGL;
	ptcsge->Reserved = 0;
	ptcsge->ContextSize = 0;
	ptcsge->DetailsLength = 12;
	ptcsge->Flags = MPI_SGE_FLAGS_TRANSACTION_ELEMENT;
	ptcsge->Reserved1 = 0;
	ptcsge->ImageOffset = 0;
	ptcsge->ImageSize = mpt->fw_image_size; /* XXX MU check endianess */

	sgeoffset = sizeof(FWUpload_t) - sizeof(SGE_MPI_UNION) +
		sizeof(FWUploadTCSGE_t);

	se = (SGE_SIMPLE32 *) &request[sgeoffset];

	flags = MPI_SGE_FLAGS_SIMPLE_ELEMENT;

	DNPRINTF(20, "%s: assembling SG list (%d entries)\n",
	    DEVNAME(mpt), mpt->fw_dmap->dm_nsegs);

	for (i = 0; i < mpt->fw_dmap->dm_nsegs; i++, se++) {
		if (i == mpt->fw_dmap->dm_nsegs - 1) {
			/* XXX MU okay? */
			flags |= MPI_SGE_FLAGS_LAST_ELEMENT |
				MPI_SGE_FLAGS_END_OF_BUFFER |
				MPI_SGE_FLAGS_END_OF_LIST;
		}

		se->Address = mpt->fw_dmap->dm_segs[i].ds_addr;
		MPI_pSGE_SET_LENGTH(se, mpt->fw_dmap->dm_segs[i].ds_len);
		MPI_pSGE_SET_FLAGS(se, flags);
		sgeoffset += sizeof(*se);
	}

	DNPRINTF(10, "%s: sending FW Upload request to IOC (size: %d, "
		"img size: %d)\n", DEVNAME(mpt), sgeoffset, mpt->fw_image_size);

	if ((error = mpt_send_handshake_cmd(mpt, sgeoffset, prequest)) != 0) {
		return(error);
	}

	error = mpt_recv_handshake_reply(mpt, sizeof(reply), &reply);

	if (error == 0) {
		/*
		 * Handshake transfer was complete and successfull.
		 * Check the Reply Frame
		 */
		int status, transfer_sz;

		status = preply->IOCStatus;
		DNPRINTF(20, "%s: fw_upload reply status %d\n",
		    DEVNAME(mpt), status);

		if (status == MPI_IOCSTATUS_SUCCESS) {
			transfer_sz = preply->ActualImageSize;
			if (transfer_sz != mpt->fw_image_size)
				error = EFAULT;
		}
		else
			error = EFAULT;
	}

	if (error == 0) {
		mpt->upload_fw = 0;
	}
	else {
		mpt_prt(mpt, "freeing image memory");
		mpt_free_fw_mem(mpt);
		mpt->fw = NULL;
	}

	return error;
}

/*
 * mpt_downloadboot - DownloadBoot code
 * Returns 0 for success, <0 for failure
 */
int
mpt_downloadboot(struct mpt_softc *mpt)
{
	MpiFwHeader_t		*fwhdr = NULL;
	MpiExtImageHeader_t	*exthdr = NULL;
	int			fw_size;
	u_int32_t		diag0;
#ifdef MPT_DEBUG
	u_int32_t		diag1;
#endif
	int			count = 0;
	u_int32_t		*ptr = NULL;
	u_int32_t		nextimg;
	u_int32_t		load_addr;
	u_int32_t		diagrw_data;

#ifdef MPT_DEBUG
	diag0 = mpt_read(mpt, MPT_OFFSET_DIAGNOSTIC);
	if (mpt->mpt2)
		diag1 = mpt_read(mpt->mpt2, MPT_OFFSET_DIAGNOSTIC);
	mpt_prt(mpt, "diag0=%08x, diag1=%08x", diag0, diag1);
#endif
	mpt_prt(mpt, "fw size 0x%x, ioc FW ptr %p", mpt->fw_image_size,
	    mpt->fw);
	if (mpt->mpt2)
		mpt_prt(mpt->mpt2, "ioc FW ptr %p", mpt->mpt2->fw);

	fw_size = mpt->fw_image_size;

	if (fw_size == 0)
		return -1;

	mpt_prt(mpt, "FW Image @ %p", mpt->fw);

	if (!mpt->fw)
		return -2;

	/*
	 * Write magic sequence to WriteSequence register
	 * until enter diagnostic mode
	 */
	diag0 = mpt_read(mpt, MPT_OFFSET_DIAGNOSTIC);
	while ((diag0 & MPI_DIAG_DRWE) == 0) {
		mpt_write(mpt, MPT_OFFSET_SEQUENCE, 0xFF);
		mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_1);
		mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_2);
		mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_3);
		mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_4);
		mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_5);

		/* wait 100msec */
		DELAY(100);

		count++;
		if (count > 20) {
			mpt_prt(mpt, "enable diagnostic mode FAILED! (%02xh)",
			    diag0);
			return -EFAULT;
		}

		diag0 = mpt_read(mpt, MPT_OFFSET_DIAGNOSTIC);
#ifdef MPT_DEBUG
		if (mpt->mpt2)
			diag1 = mpt_read(mpt->mpt2, MPT_OFFSET_DIAGNOSTIC);
		mpt_prt(mpt, "diag0=%08x, diag1=%08x", diag0, diag1);
#endif
		mpt_prt(mpt, "wrote magic DiagWriteEn sequence (%x)", diag0);
	}

	/* Set the DiagRwEn and Disable ARM bits */
	diag0 |= (MPI_DIAG_RW_ENABLE | MPI_DIAG_DISABLE_ARM);
	mpt_write(mpt, MPT_OFFSET_DIAGNOSTIC, diag0);

#ifdef MPT_DEBUG
	if (mpt->mpt2)
		diag1 = mpt_read(mpt->mpt2, MPT_OFFSET_DIAGNOSTIC);
	mpt_prt(mpt, "diag0=%08x, diag1=%08x", diag0, diag1);
#endif

	fwhdr = (MpiFwHeader_t *) mpt->fw;
	ptr = (u_int32_t *) fwhdr;
	count = (fwhdr->ImageSize + 3)/4;
	nextimg = fwhdr->NextImageHeaderOffset;

	/*
	 * write the LoadStartAddress to the DiagRw Address Register
	 * XXX linux is using programmed IO for the RWADDR and RWDATA
	 */
	mpt_write(mpt, MPT_OFFSET_RWADDR, fwhdr->LoadStartAddress);

	mpt_prt(mpt, "LoadStart addr written 0x%x", fwhdr->LoadStartAddress);
	mpt_prt(mpt, "writing file image: 0x%x u32's @ %p", count, ptr);

	while (count--) {
		mpt_write(mpt, MPT_OFFSET_RWDATA, *ptr);
		ptr++;
	}

	while (nextimg) {
		ptr = (u_int32_t *) (mpt->fw + nextimg);
		exthdr = (MpiExtImageHeader_t *) ptr;
		count = (exthdr->ImageSize +3)/4;
		nextimg = exthdr->NextImageHeaderOffset;
		load_addr = exthdr->LoadStartAddress;

		mpt_prt(mpt, "write ext image: 0x%x u32's @ %p", count, ptr);

		mpt_write(mpt, MPT_OFFSET_RWADDR, load_addr);

		while (count--) {
			mpt_write(mpt, MPT_OFFSET_RWDATA, *ptr);
			ptr++;
		}
	}

	/* write the IopResetVectorRegAddr */
	mpt_prt(mpt, "write IopResetVector addr!");
	mpt_write(mpt, MPT_OFFSET_RWADDR, fwhdr->IopResetRegAddr);

	/* write the IopResetVectorValue */
	mpt_prt(mpt, "write IopResetVector value!");
	mpt_write(mpt, MPT_OFFSET_RWDATA, fwhdr->IopResetVectorValue);

	/*
	 * clear the internal flash bad bit - autoincrementing register,
	 * so must do two writes.
	 */
	mpt_write(mpt, MPT_OFFSET_RWADDR, 0x3F000000);
	diagrw_data = mpt_read(mpt, MPT_OFFSET_RWDATA);
	diagrw_data |= 0x4000000;
	mpt_write(mpt, MPT_OFFSET_RWADDR, 0x3F000000);
	mpt_write(mpt, MPT_OFFSET_RWDATA, diagrw_data);

	/* clear the RW enable and DISARM bits */
	diag0 = mpt_read(mpt, MPT_OFFSET_DIAGNOSTIC);
	diag0 &= ~(MPI_DIAG_DISABLE_ARM | MPI_DIAG_RW_ENABLE |
	    MPI_DIAG_FLASH_BAD_SIG);
	mpt_write(mpt, MPT_OFFSET_DIAGNOSTIC, diag0);

	/* write 0xFF to reset the sequencer */
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, 0xFF);

	return 0;
}
