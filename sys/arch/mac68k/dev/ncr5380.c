/*	$OpenBSD: ncr5380.c,v 1.42 2011/07/05 20:34:45 matthew Exp $	*/
/*	$NetBSD: ncr5380.c,v 1.38 1996/12/19 21:48:18 scottr Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
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
 * Bit mask of targets you want debugging to be shown
 */
u_char	dbg_target_mask = 0x7f;

/*
 * Set bit for target when parity checking must be disabled.
 * My (LWP) Maxtor 7245S  seems to generate parity errors on about 50%
 * of all transfers while the data is correct!?
 */
u_char	ncr5380_no_parchk = 0xff;

/*
 * Flags to allow binpatching of which devices to allow or disallow
 * issuance of linked commands.  disallowance is favored over allowance,
 * so if the same bit is set in either, the driver will not issue linked
 * commands to the corresponding target.
 * SCSI-2 devices can report whether or not they will accept linked
 * commands.  If that reports that a device supports linked commands,
 * then it is treated as if the corresponding bit in ncr5380_allow_linked
 * is set.
 */
u_char	ncr5380_allow_linked = 0x80;
u_char	ncr5380_disallow_linked = 0x80;

/*
 * This is the default sense-command we send.
 */
static	u_char	sense_cmd[] = {
		REQUEST_SENSE, 0, 0, 0, sizeof(struct scsi_sense_data), 0
};

/*
 * True if the main co-routine is running
 */
static volatile int	main_running = 0;

/*
 * Mask of targets selected
 */
static u_char	busy;

static void	ncr5380_minphys(struct buf *bp, struct scsi_link *sl);
static void	mac68k_ncr5380_scsi_cmd(struct scsi_xfer *xs);
static void	ncr5380_show_scsi_cmd(struct scsi_xfer *xs);

void	 ncr_sc_req_free(void *, void *);
void	*ncr_sc_req_alloc(void *);

struct scsi_adapter ncr5380_switch = {
	mac68k_ncr5380_scsi_cmd,	/* scsi_cmd() */
	ncr5380_minphys,		/* scsi_minphys() */
	NULL,				/* probe_dev() */
	NULL				/* free_dev() */
};


static SC_REQ	req_queue[NREQ];
static SC_REQ	*free_head = NULL;	/* Free request structures	*/

/*
 * Inline functions:
 */

/*
 * Determine the size of a SCSI command.
 */
extern __inline__ int command_size(opcode)
u_char	opcode;
{
	switch ((opcode >> 4) & 0xf) {
		case 0:
		case 1:
			return (6);
		case 2:
		case 3:
			return (10);
	}
	return (12);
}


/*
 * Wait for request-line to become active. When it doesn't return 0.
 * Otherwise return != 0.
 * The timeouts in the 'wait_req_*' functions are arbitrary and rather
 * large. In 99% of the invocations nearly no timeout is needed but in
 * some cases (especially when using my tapedrive, a Tandberg 3600) the
 * device is busy internally and the first SCSI-phase will be delayed.
 *
 * -- A sleeping Fujitsu M2512 is even worse; try 2.5 sec     -hf 20 Jun
 */
extern __inline__ int wait_req_true(void)
{
	int	timeout = 2500000;

	while (!(GET_5380_REG(NCR5380_IDSTAT) & SC_S_REQ) && --timeout)
		delay(1);
	return (GET_5380_REG(NCR5380_IDSTAT) & SC_S_REQ);
}

/*
 * Wait for request-line to become inactive. When it doesn't return 0.
 * Otherwise return != 0.
 */
extern __inline__ int wait_req_false(void)
{
	int	timeout = 2500000;

	while ((GET_5380_REG(NCR5380_IDSTAT) & SC_S_REQ) && --timeout)
		delay(1);
	return (!(GET_5380_REG(NCR5380_IDSTAT) & SC_S_REQ));
}

extern __inline__ void ack_message()
{
	SET_5380_REG(NCR5380_ICOM, 0);
}

extern __inline__ void nack_message(SC_REQ *reqp, u_char msg)
{
	SET_5380_REG(NCR5380_ICOM, SC_A_ATN);
	reqp->msgout = msg;
}

extern __inline__ void finish_req(SC_REQ *reqp)
{
	struct scsi_xfer	*xs = reqp->xs;

#ifdef REAL_DMA
	/*
	 * If we bounced, free the bounce buffer
	 */
	if (reqp->dr_flag & DRIVER_BOUNCING)
		free_bounceb(reqp->bounceb);
#endif /* REAL_DMA */
#ifdef DBG_REQ
	if (dbg_target_mask & (1 << reqp->targ_id))
		show_request(reqp, "DONE");
#endif
#ifdef DBG_ERR_RET
	if (reqp->xs->error != 0)
		show_request(reqp, "ERR_RET");
#endif
	scsi_done(xs);
}

/*
 * Auto config stuff....
 */
void	ncr_attach(struct device *, struct device *, void *);
int	ncr_match(struct device *, void *, void *);

/*
 * Tricks to make driver-name configurable
 */
#define CFNAME(n)	__CONCAT(n,_cd)
#define CANAME(n)	__CONCAT(n,_ca)
#define CFSTRING(n)	__STRING(n)

struct cfattach CANAME(DRNAME) = {
	sizeof(struct ncr_softc), ncr_match, ncr_attach
};

struct cfdriver CFNAME(DRNAME) = {
	NULL, CFSTRING(DRNAME), DV_DULL
};

void
ncr_sc_req_free(void *xsc, void *xsc_req)
{
	struct ncr_softc *sc = (struct ncr_softc *)xsc;
	SC_REQ *sc_req = (SC_REQ *)xsc_req;

	mtx_enter(&sc->sc_sc_req_mtx);
	sc_req->next = free_head;
	free_head = sc_req;
	mtx_leave(&sc->sc_sc_req_mtx);
}

void *
ncr_sc_req_alloc(void *xsc)
{
	struct ncr_softc *sc = (struct ncr_softc *)xsc;
	SC_REQ *sc_req = NULL;

	mtx_enter(&sc->sc_sc_req_mtx);
	if (free_head) {
		sc_req = free_head;
		free_head = free_head->next;
		sc_req->next = NULL;
	}
	mtx_leave(&sc->sc_sc_req_mtx);
	
	return (sc_req);
}

int
ncr_match(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	return (machine_match(parent,
				(struct cfdata *) cf, aux, &CFNAME(DRNAME)));
}

void
ncr_attach(pdp, dp, auxp)
struct device	*pdp, *dp;
void		*auxp;
{
	struct ncr_softc	*sc;
	struct scsibus_attach_args saa;
	int			i;

	sc = (struct ncr_softc *)dp;

	sc->sc_link.adapter_softc   = sc;
	sc->sc_link.adapter_target  = 7;
	sc->sc_link.adapter         = &ncr5380_switch;
	sc->sc_link.openings        = NREQ - 1;
	sc->sc_link.pool	    = &sc->sc_iopool;

	/*
	 * bitmasks
	 */
	sc->sc_noselatn = 0;
	sc->sc_selected = 0;

	/*
	 * Initialize machine-type specific things...
	 */
	scsi_mach_init(sc);
	printf("\n");

	/*
	 * Initialize request queue freelist.
	 */
	for (i = 0; i < NREQ; i++) {
		req_queue[i].next = free_head;
		free_head = &req_queue[i];
	}

	/*
	 * Initialize the iopool.
	 */
	mtx_init(&sc->sc_sc_req_mtx, IPL_BIO);
	scsi_iopool_init(&sc->sc_iopool, sc, ncr_sc_req_alloc, ncr_sc_req_free);

	/*
	 * Initialize the host adapter
	 */
	scsi_idisable();
	ENABLE_NCR5380(sc);
	SET_5380_REG(NCR5380_ICOM, 0);
	SET_5380_REG(NCR5380_MODE, IMODE_BASE);
	SET_5380_REG(NCR5380_TCOM, 0);
	SET_5380_REG(NCR5380_IDSTAT, 0);
	scsi_ienable();

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	/*
	 * attach all scsi units on us
	 */
	config_found(dp, &saa, scsiprint);
}

/*
 * End of auto config stuff....
 */

/*
 * Carry out a request from the high level driver.
 */
static void
mac68k_ncr5380_scsi_cmd(struct scsi_xfer *xs)
{
	int	sps;
	SC_REQ	*reqp, *link, *tmp;
	int	flags = xs->flags;

	/*
	 * We do not queue RESET commands
	 */
	if (flags & SCSI_RESET) {
		scsi_reset_verbose(xs->sc_link->adapter_softc,
				   "Got reset-command");
		return;
	}

	reqp = xs->io;

	/*
	 * Initialize our private fields
	 */
	reqp->dr_flag   = (flags & SCSI_POLL) ? DRIVER_NOINT : 0;
	reqp->phase     = NR_PHASE;
	reqp->msgout    = MSG_NOOP;
	reqp->status    = SCSGOOD;
	reqp->message   = 0xff;
	reqp->link      = NULL;
	reqp->xs        = xs;
	reqp->targ_id   = xs->sc_link->target;
	reqp->targ_lun  = xs->sc_link->lun;
	reqp->xdata_ptr = (u_char *)xs->data;
	reqp->xdata_len = xs->datalen;
	memcpy(&reqp->xcmd, xs->cmd, sizeof(struct scsi_generic));
	reqp->xcmd.bytes[0] |= reqp->targ_lun << 5;

#ifdef REAL_DMA
	/*
	 * Check if DMA can be used on this request
	 */
	if (scsi_dmaok(reqp))
		reqp->dr_flag |= DRIVER_DMAOK;
#endif /* REAL_DMA */

	/*
	 * Insert the command into the issue queue. Note that 'REQUEST SENSE'
	 * commands are inserted at the head of the queue since any command
	 * will clear the existing contingent allegience condition and the sense
	 * data is only valid while the condition exists.
	 * When possible, link the command to a previous command to the same
	 * target. This is not very sensible when AUTO_SENSE is not defined!
	 * Interrupts are disabled while we are fiddling with the issue-queue.
	 */
	sps = splbio();
	link = NULL;
	if ((issue_q == NULL) || (reqp->xcmd.opcode == REQUEST_SENSE)) {
		reqp->next = issue_q;
		issue_q    = reqp;
	}
	else {
		tmp  = issue_q;
		do {
		    if (!link && (tmp->targ_id == reqp->targ_id) && !tmp->link)
				link = tmp;
		} while (tmp->next && (tmp = tmp->next));
		tmp->next = reqp;
#ifdef AUTO_SENSE
		if (link && ((xs->sc_link->inqdata.flags & SID_Linked)
			     || ((1<<reqp->targ_id) & ncr5380_allow_linked))
			 && !((1<<reqp->targ_id) & ncr5380_disallow_linked)) {
			link->link = reqp;
			link->xcmd.bytes[link->xs->cmdlen-2] |= 1;
		}
#endif
	}
	splx(sps);

#ifdef DBG_REQ
	if (dbg_target_mask & (1 << reqp->targ_id))
		show_request(reqp, (reqp->xcmd.opcode == REQUEST_SENSE) ?
								"HEAD":"TAIL");
#endif

	run_main(xs->sc_link->adapter_softc);
}

static void
ncr5380_minphys(struct buf *bp, struct scsi_link *sl)
{
	if (bp->b_bcount > MIN_PHYS)
		bp->b_bcount = MIN_PHYS;
	minphys(bp);
}
#undef MIN_PHYS

static void
ncr5380_show_scsi_cmd(struct scsi_xfer *xs)
{
	u_char	*b = (u_char *) xs->cmd;
	int	i  = 0;

	if (!(xs->flags & SCSI_RESET)) {
		printf("(%d:%d:%d,0x%x)-", xs->sc_link->bus->sc_dev.dv_unit,
		    xs->sc_link->target, xs->sc_link->lun, xs->sc_link->flags);
		while (i < xs->cmdlen) {
			if (i)
				printf(",");
			printf("%x",b[i++]);
		}
		printf("-\n");
	}
	else {
		printf("(%d:%d:%d)-RESET-\n", xs->sc_link->bus->sc_dev.dv_unit,
		    xs->sc_link->target, xs->sc_link->lun);
	}
}

/*
 * The body of the driver.
 */
static void
scsi_main(sc)
struct ncr_softc *sc;
{
	SC_REQ	*req, *prev;
	int	itype;
	int	sps;

	/*
	 * While running in the driver SCSI-interrupts are disabled.
	 */
	scsi_idisable();
	ENABLE_NCR5380(sc);

	PID("scsi_main1");
	for (;;) {
	    sps = splbio();
	    if (!connected) {
		/*
		 * Check if it is fair keep any exclusive access to DMA
		 * claimed. If not, stop queueing new jobs so the discon_q
		 * will be eventually drained and DMA can be given up.
		 */
		if (!fair_to_keep_dma())
			goto main_exit;

		/*
		 * Search through the issue-queue for a command
		 * destined for a target that isn't busy.
		 */
		prev = NULL;
		for (req=issue_q; req != NULL; prev = req, req = req->next) {
			if (!(busy & (1 << req->targ_id))) {
				/*
				 * Found one, remove it from the issue queue
				 */
				if (prev == NULL)
					issue_q = req->next;
				else prev->next = req->next;
				req->next = NULL;
				break;
			}
		}

		/*
		 * When a request has just ended, we get here before an other
		 * device detects that the bus is free and that it can
		 * reconnect. The problem is that when this happens, we always
		 * baffle the device because our (initiator) id is higher. This
		 * can cause a sort of starvation on slow devices. So we check
		 * for a pending reselection here.
		 * Note that 'connected' will be non-null if the reselection
		 * succeeds.
		 */
		if ((GET_5380_REG(NCR5380_IDSTAT) & (SC_S_SEL|SC_S_IO))
						== (SC_S_SEL|SC_S_IO)){
			if (req != NULL) {
				req->next = issue_q;
				issue_q = req;
			}
			splx(sps);

			reselect(sc);
			scsi_clr_ipend();
			goto connected;
		}

		/*
		 * The host is not connected and there is no request
		 * pending, exit.
		 */
		if (req == NULL) {
			PID("scsi_main2");
			goto main_exit;
		}

		/*
		 * Re-enable interrupts before handling the request.
		 */
		splx(sps);

#ifdef DBG_REQ
		if (dbg_target_mask & (1 << req->targ_id))
			show_request(req, "TARGET");
#endif
		/*
		 * We found a request. Try to connect to the target. If the
		 * initiator fails arbitration, the command is put back in the
		 * issue queue.
		 */
		if (scsi_select(req, 0)) {
			sps = splbio();
			req->next = issue_q;
			issue_q = req;
			splx(sps);
#ifdef DBG_REQ
			if (dbg_target_mask & (1 << req->targ_id))
				ncr_tprint(req, "Select failed\n");
#endif
		}
	    }
	    else splx(sps);
connected:
	    if (connected) {
		/*
		 * If the host is currently connected but a 'real-dma' transfer
		 * is in progress, the 'end-of-dma' interrupt restarts main.
		 * So quit.
		 */
		sps = splbio();
		if (connected && (connected->dr_flag & DRIVER_IN_DMA)) {
			PID("scsi_main3");
			goto main_exit;
		}
		splx(sps);

		/*
		 * Let the target guide us through the bus-phases
		 */
		while (information_transfer(sc) == -1)
			;
	    }
	}
	/* NEVER TO REACH HERE */
	panic("ncr5380-SCSI: not designed to come here");

main_exit:
	/*
	 * We enter here with interrupts disabled. We are about to exit main
	 * so interrupts should be re-enabled. Because interrupts are edge
	 * triggered, we could already have missed the interrupt. Therefore
	 * we check the IRQ-line here and re-enter when we really missed a
	 * valid interrupt.
	 */
	PID("scsi_main4");
	scsi_ienable();

	/*
	 * If we're not currently connected, enable reselection
	 * interrupts.
	 */
	if (!connected)
		SET_5380_REG(NCR5380_IDSTAT, SC_HOST_ID);

	if (scsi_ipending()) {
		if ((itype = check_intr(sc)) != INTR_SPURIOUS) {
			scsi_idisable();
			splx(sps);

			if (itype == INTR_RESEL)
				reselect(sc);
#ifdef REAL_DMA
			else dma_ready();
#else
			else {
				if (pdma_ready())
					goto connected;
				panic("Got DMA interrupt without DMA");
			}
#endif
			scsi_clr_ipend();
			goto connected;
		}
	}
	reconsider_dma();

	main_running = 0;
	splx(sps);
	PID("scsi_main5");
}

#ifdef REAL_DMA
/*
 * The SCSI-DMA interrupt.
 * This interrupt can only be triggered when running in non-polled DMA
 * mode. When DMA is not active, it will be silently ignored, it is usually
 * too late because the EOP interrupt of the controller happens just a tiny
 * bit earlier. It might become useful when scatter/gather is implemented,
 * because in that case only part of the DATAIN/DATAOUT transfer is taken
 * out of a single buffer.
 */
static void
ncr_dma_intr(sc)
struct ncr_softc *sc;
{
	SC_REQ	*reqp;
	int	dma_done;

	PID("ncr_dma_intr");
	if ((reqp = connected) && (reqp->dr_flag & DRIVER_IN_DMA)) {
		scsi_idisable();
		if (!(dma_done = dma_ready())) {
			transfer_dma(reqp, reqp->phase, 0);
			return;
		}
		run_main(sc);
	}
}
#endif /* REAL_DMA */

/*
 * The SCSI-controller interrupt. This interrupt occurs on reselections and
 * at the end of non-polled DMA-interrupts. It is assumed to be called from
 * the machine-dependent hardware interrupt.
 */
static void
ncr_ctrl_intr(sc)
struct ncr_softc *sc;
{
	int	itype;

	while (scsi_ipending()) {
		scsi_idisable();
		if ((itype = check_intr(sc)) != INTR_SPURIOUS) {
			if (itype == INTR_RESEL)
				reselect(sc);
			else {
#ifdef REAL_DMA
			    int	dma_done;
			    if (!(dma_done = dma_ready())) {
				transfer_dma(connected, connected->phase, 0);
				return;
			    }
#else
			    if (pdma_ready())
				return;
			    panic("Got DMA interrupt without DMA");
#endif
			}
			scsi_clr_ipend();
		}
		run_main(sc);
		return;
	}
	PID("ncr_ctrl_intr1");
}

/*
 * Initiate a connection path between the host and the target. The function
 * first goes into arbitration for the SCSI-bus. When this succeeds, the target
 * is selected and an 'IDENTIFY' message is send.
 * Returns -1 when the arbitration failed. Otherwise 0 is returned. When
 * the target does not respond (to either selection or 'MESSAGE OUT') the
 * 'done' function is executed.
 * The result code given by the driver can be influenced by setting 'code'
 * to a non-zero value. This is the case when 'select' is called by abort.
 */
static int
scsi_select(reqp, code)
SC_REQ	*reqp;
int	code;
{
	u_char			tmp[1];
	u_char			phase;
	u_long			cnt;
	int			sps;
	u_int8_t		atn_flag;
	u_int8_t		targ_bit;
	struct ncr_softc	*sc;

	sc = reqp->xs->sc_link->adapter_softc;
	DBG_SELPRINT ("Starting arbitration\n", 0);
	PID("scsi_select1");

	sps = splbio();

	/*
	 * Prevent a race condition here. If a reslection interrupt occurred
	 * between the decision to pick a new request and the call to select,
	 * we abort the selection.
	 * Interrupts are lowered when the 5380 is setup to arbitrate for the
	 * bus.
	 */
	if (connected) {
		splx(sps);
		PID("scsi_select2");
		return (-1);
	}

	/*
	 * Set phase bits to 0, otherwise the 5380 won't drive the bus during
	 * selection.
	 */
	SET_5380_REG(NCR5380_TCOM, 0);
	SET_5380_REG(NCR5380_ICOM, 0);

	/*
	 * Arbitrate for the bus.
	 */
	SET_5380_REG(NCR5380_DATA, SC_HOST_ID);
	SET_5380_REG(NCR5380_MODE, SC_ARBIT);

	splx(sps);

	cnt = 10;
	while (!(GET_5380_REG(NCR5380_ICOM) & SC_AIP) && --cnt)
		delay(1);

	if (!(GET_5380_REG(NCR5380_ICOM) & SC_AIP)) {
		SET_5380_REG(NCR5380_MODE, IMODE_BASE);
		SET_5380_REG(NCR5380_ICOM, 0);
		DBG_SELPRINT ("Arbitration lost, bus not free\n",0);
		PID("scsi_select3");
		return (-1);
	}

	/* The arbitration delay is 2.2 usecs */
	delay(3);

	/*
	 * Check the result of the arbitration. If we failed, return -1.
	 */
	if (GET_5380_REG(NCR5380_ICOM) & SC_LA) {
		SET_5380_REG(NCR5380_MODE, IMODE_BASE);
		SET_5380_REG(NCR5380_ICOM, 0);
		PID("scsi_select4");
		return (-1);
	}

	/*
	 * The spec requires that we should read the data register to
	 * check for higher id's and check the SC_LA again.
	 */
	tmp[0] = GET_5380_REG(NCR5380_DATA);
	if (tmp[0] & ~((SC_HOST_ID << 1) - 1)) {
		SET_5380_REG(NCR5380_MODE, IMODE_BASE);
		SET_5380_REG(NCR5380_ICOM, 0);
		DBG_SELPRINT ("Arbitration lost, higher id present\n",0);
		PID("scsi_select5");
		return (-1);
	}
	if (GET_5380_REG(NCR5380_ICOM) & SC_LA) {
		SET_5380_REG(NCR5380_MODE, IMODE_BASE);
		SET_5380_REG(NCR5380_ICOM, 0);
		DBG_SELPRINT ("Arbitration lost,deassert SC_ARBIT\n",0);
		PID("scsi_select6");
		return (-1);
	}
	SET_5380_REG(NCR5380_ICOM, SC_A_SEL | SC_A_BSY);
	if (GET_5380_REG(NCR5380_ICOM) & SC_LA) {
		SET_5380_REG(NCR5380_MODE, IMODE_BASE);
		SET_5380_REG(NCR5380_ICOM, 0);
		DBG_SELPRINT ("Arbitration lost, deassert SC_A_SEL\n", 0);
		PID("scsi_select7");
		return (-1);
	}
	/* Bus settle delay + Bus clear delay = 1.2 usecs */
	delay(2);
	DBG_SELPRINT ("Arbitration complete\n", 0);

	/*
	 * Now that we won the arbitration, start the selection.
	 */
	targ_bit = 1 << reqp->targ_id;
	SET_5380_REG(NCR5380_DATA, SC_HOST_ID | targ_bit);

	if (sc->sc_noselatn & targ_bit)
		atn_flag = 0;
	else
		atn_flag = SC_A_ATN;

	/*
	 * Raise ATN while SEL is true before BSY goes false from arbitration,
	 * since this is the only way to guarantee that we'll get a MESSAGE OUT
	 * phase immediately after the selection.
	 */
	SET_5380_REG(NCR5380_ICOM, SC_A_BSY | SC_A_SEL | atn_flag | SC_ADTB);
	SET_5380_REG(NCR5380_MODE, IMODE_BASE);

	/*
	 * Turn off reselection interrupts
	 */
	SET_5380_REG(NCR5380_IDSTAT, 0);

	/*
	 * Reset BSY. The delay following it, surpresses a glitch in the
	 * 5380 which causes us to see our own BSY signal instead of that of
	 * the target.
	 */
	SET_5380_REG(NCR5380_ICOM, SC_A_SEL | atn_flag | SC_ADTB);
	delay(1);

	/*
	 * Wait for the target to react, the specs call for a timeout of
	 * 250 ms.
	 */
	cnt = 25000;
	while (!(GET_5380_REG(NCR5380_IDSTAT) & SC_S_BSY) && --cnt)
		delay(10);

	if (!(GET_5380_REG(NCR5380_IDSTAT) & SC_S_BSY)) {
		/*
		 * There is no reaction from the target, start the selection
		 * timeout procedure. We release the databus but keep SEL
		 * asserted. After that we wait a 'selection abort time' (200
		 * usecs) and 2 deskew delays (90 ns) and check BSY again.
		 * When BSY is asserted, we assume the selection succeeded,
		 * otherwise we release the bus.
		 */
		SET_5380_REG(NCR5380_ICOM, SC_A_SEL | atn_flag);
		delay(201);
		if (!(GET_5380_REG(NCR5380_IDSTAT) & SC_S_BSY)) {
			SET_5380_REG(NCR5380_ICOM, 0);
			reqp->xs->error      = code ? code : XS_SELTIMEOUT;
			DBG_SELPRINT ("Target %d not responding to sel\n",
								reqp->targ_id);
			finish_req(reqp);
			PID("scsi_select8");
			return (0);
		}
	}
	SET_5380_REG(NCR5380_ICOM, atn_flag);

	DBG_SELPRINT ("Target %d responding to select.\n", reqp->targ_id);

	/*
	 * The SCSI-interrupts are disabled while a request is being handled.
	 */
	scsi_idisable();

	/*
	 * If we did not request ATN, then don't try to send IDENTIFY.
	 */
	if (atn_flag == 0) {
		reqp->phase = PH_CMD;
		goto identify_failed;
	}

	/*
	 * Here we prepare to send an 'IDENTIFY' message.
	 * Allow disconnect only when interrupts are allowed.
	 */
	tmp[0] = MSG_IDENTIFY(reqp->targ_lun,
			(reqp->dr_flag & DRIVER_NOINT) ? 0 : 1);
	cnt    = 1;
	phase  = PH_MSGOUT;

	/*
	 * Since we followed the SCSI-spec and raised ATN while SEL was true
	 * but before BSY was false during the selection, a 'MESSAGE OUT'
	 * phase should follow.  Unfortunately, this does not happen on
	 * all targets (Asante ethernet devices, for example), so we must
	 * check the actual mode if the message transfer fails--if the
	 * new phase is PH_CMD and has never been successfully selected
	 * w/ATN in the past, then we assume that it is an old device
	 * that doesn't support select w/ATN.
	 */
	if (transfer_pio(&phase, tmp, &cnt, 0) || cnt) {

		if ((phase == PH_CMD) && !(sc->sc_selected & targ_bit)) {
			DBG_SELPRINT ("Target %d: not responding to ATN.\n",
							reqp->targ_id);
			sc->sc_noselatn |= targ_bit;
			reqp->phase = PH_CMD;
			goto identify_failed;
		}

		DBG_SELPRINT ("Target %d: failed to send identify\n",
							reqp->targ_id);
		/*
		 * Try to disconnect from the target.  We cannot leave
		 * it just hanging here.
		 */
		if (!reach_msg_out(sc, sizeof(struct scsi_generic))) {
			u_long	len   = 1;
			u_char	phase = PH_MSGOUT;
			u_char	msg   = MSG_ABORT;

			transfer_pio(&phase, &msg, &len, 0);
		}
		else scsi_reset_verbose(sc, "Connected to unidentified target");

		SET_5380_REG(NCR5380_ICOM, 0);
		reqp->xs->error = code ? code : XS_DRIVER_STUFFUP;
		finish_req(reqp);
		PID("scsi_select9");
		return (0);
	}
	reqp->phase = PH_MSGOUT;

identify_failed:
	sc->sc_selected |= targ_bit;

#ifdef notyet /* LWP: Do we need timeouts in the driver? */
	/*
	 * Command is connected, start timer ticking.
	 */
	ccb_p->xtimeout = ccb_p->timeout + Lbolt;
#endif

	connected  = reqp;
	busy      |= targ_bit;
	PID("scsi_select10");
	return (0);
}

/*
 * Return codes:
 *	 0: Job has finished or disconnected, find something else
 *	-1: keep on calling information_transfer() from scsi_main()
 */
static int
information_transfer(sc)
struct ncr_softc *sc;
{
	SC_REQ	*reqp = connected;
	u_char	tmp, phase;
	u_long	len;

	PID("info_transf1");
	/*
	 * Clear pending interrupts from 5380-chip.
	 */
	scsi_clr_ipend();

	/*
	 * The SCSI-spec requires BSY to be true while connected to a target,
	 * loosing it means we lost the target...
	 * Also REQ needs to be asserted here to indicate that the bus-phase
	 * is valid. When the target does not supply REQ within a 'reasonable'
	 * amount of time, it's probably lost in its own maze of twisting
	 * passages, we have to reset the bus to free it.
	 */
	if (GET_5380_REG(NCR5380_IDSTAT) & SC_S_BSY)
		wait_req_true();
	tmp = GET_5380_REG(NCR5380_IDSTAT);


	if ((tmp & (SC_S_BSY|SC_S_REQ)) != (SC_S_BSY|SC_S_REQ)) {
		busy           &= ~(1 << reqp->targ_id);
		connected       = NULL;
		reqp->xs->error = XS_TIMEOUT;
		finish_req(reqp);
		if (!(tmp & SC_S_REQ))
			scsi_reset_verbose(sc,
					   "Timeout waiting for phase-change");
		PID("info_transf2");
		return (0);
	}

	phase = (tmp >> 2) & 7;
	if (phase != reqp->phase) {
		reqp->phase = phase;
		DBG_INFPRINT(show_phase, reqp, phase);
	}
	else {
		/*
		 * Same data-phase. If same error give up
		 */
		if ((reqp->msgout == MSG_ABORT)
		     && ((phase == PH_DATAOUT) || (phase == PH_DATAIN))) {
			busy     &= ~(1 << reqp->targ_id);
			connected = NULL;
			finish_req(reqp);
			scsi_reset_verbose(sc, "Failure to abort command");
			return (0);
		}
	}

	switch (phase) {
	    case PH_DATAOUT:
#ifdef DBG_NOWRITE
		ncr_tprint(reqp, "NOWRITE set -- write attempt aborted.");
		reqp->msgout = MSG_ABORT;
		SET_5380_REG(NCR5380_ICOM, SC_A_ATN);
		return (-1);
#endif /* DBG_NOWRITE */
		/*
		 * If this is the first write using DMA, fill
		 * the bounce buffer.
		 */
		if (reqp->xdata_ptr == reqp->xs->data) { /* XXX */
		    if (reqp->dr_flag & DRIVER_BOUNCING)
			bcopy(reqp->xdata_ptr, reqp->bounceb, reqp->xdata_len);
		}

	   case PH_DATAIN:
		if (reqp->xdata_len <= 0) {
			/*
			 * Target keeps requesting data. Try to get into
			 * message-out phase by feeding/taking 100 byte.
			 */
			ncr_tprint(reqp, "Target requests too much data\n");
			reqp->msgout = MSG_ABORT;
			SET_5380_REG(NCR5380_ICOM, SC_A_ATN);
			reach_msg_out(sc, 100);
			return (-1);
		}
#ifdef REAL_DMA
		if (reqp->dr_flag & DRIVER_DMAOK) {
			int poll = REAL_DMA_POLL|(reqp->dr_flag & DRIVER_NOINT);
			transfer_dma(reqp, phase, poll);
			if (!poll)
				return (0);
		}
		else
#endif
		{
			PID("info_transf3");
			len = reqp->xdata_len;
#ifdef USE_PDMA
			if (transfer_pdma(&phase, reqp->xdata_ptr, &len) == 0)
				return (0);
#else
			transfer_pio(&phase, reqp->xdata_ptr, &len, 0);
#endif
			reqp->xdata_ptr += reqp->xdata_len - len;
			reqp->xdata_len  = len;
		}
		return (-1);
	   case PH_MSGIN:
		/*
		 * We only expect single byte messages here.
		 */
		len = 1;
		transfer_pio(&phase, &tmp, &len, 1);
		reqp->message = tmp;
		return (handle_message(reqp, tmp));
	   case PH_MSGOUT:
		len = 1;
		transfer_pio(&phase, &reqp->msgout, &len, 0);
		if (reqp->msgout == MSG_ABORT) {
			busy     &= ~(1 << reqp->targ_id);
			connected = NULL;
			if (!reqp->xs->error)
				reqp->xs->error = XS_DRIVER_STUFFUP;
			finish_req(reqp);
			PID("info_transf4");
			return (0);
		}
		reqp->msgout = MSG_NOOP;
		return (-1);
	   case PH_CMD :
		len = command_size(reqp->xcmd.opcode);
		transfer_pio(&phase, (u_char *)&reqp->xcmd, &len, 0);
		PID("info_transf5");
		return (-1);
	   case PH_STATUS:
		len = 1;
		transfer_pio(&phase, &tmp, &len, 0);
		reqp->status = tmp;
		PID("info_transf6");
		return (-1);
	   default :
		ncr_tprint(reqp, "Unknown phase\n");
	}
	PID("info_transf7");
	return (-1);
}

/*
 * Handle the message 'msg' send to us by the target.
 * Return values:
 *	 0 : The current command has completed.
 *	-1 : Get on to the next phase.
 */
static int
handle_message(reqp, msg)
SC_REQ	*reqp;
u_int	msg;
{
	int	sps;
	SC_REQ	*prev, *req;

	PID("hmessage1");
	switch (msg) {
		/*
		 * Linking lets us reduce the time required to get
		 * the next command to the device, skipping the arbitration
		 * and selection time. In the current implementation,
		 * we merely have to start the next command pointed
		 * to by 'next_link'.
		 */
		case MSG_LINK_CMD_COMPLETE:
		case MSG_LINK_CMD_COMPLETEF:
			if (reqp->link == NULL) {
				ncr_tprint(reqp, "No link for linked command");
				nack_message(reqp, MSG_ABORT);
				PID("hmessage2");
				return (-1);
			}
			ack_message();
			if (!(reqp->dr_flag & DRIVER_AUTOSEN)) {
				reqp->xs->resid = reqp->xdata_len;
				reqp->xs->error = 0;
			}

#ifdef AUTO_SENSE
			if (check_autosense(reqp, 1) == -1)
				return (-1);
#endif /* AUTO_SENSE */

#ifdef DBG_REQ
			if (dbg_target_mask & (1 << reqp->targ_id))
				show_request(reqp->link, "LINK");
#endif
			connected = reqp->link;

			/*
			 * Unlink the 'linked' request from the issue_q
			 */
			sps  = splbio();
			prev = NULL;
			req  = issue_q;
			for (; req != NULL; prev = req, req = req->next) {
				if (req == connected)
					break;
			}
			if (req == NULL)
				panic("Inconsistent issue_q");
			if (prev == NULL)
				issue_q = req->next;
			else prev->next = req->next;
			req->next = NULL;
			splx(sps);

			finish_req(reqp);
			PID("hmessage3");
			return (-1);
		case MSG_ABORT:
		case MSG_CMDCOMPLETE:
			ack_message();
			connected = NULL;	
			busy     &= ~(1 << reqp->targ_id);
			if (!(reqp->dr_flag & DRIVER_AUTOSEN)) {
				reqp->xs->resid = reqp->xdata_len;
				reqp->xs->error = 0;
			}

#ifdef AUTO_SENSE
			if (check_autosense(reqp, 0) == -1) {
				PID("hmessage4");
				return (0);
			}
#endif /* AUTO_SENSE */

			finish_req(reqp);
			PID("hmessage5");
			return (0);
		case MSG_MESSAGE_REJECT:
			ack_message();
			PID("hmessage6");
			return (-1);
		case MSG_DISCONNECT:
			ack_message();
#ifdef DBG_REQ
			if (dbg_target_mask & (1 << reqp->targ_id))
				show_request(reqp, "DISCON");
#endif
			sps = splbio();
			connected  = NULL;
			reqp->next = discon_q;
			discon_q   = reqp;
			splx(sps);
			PID("hmessage7");
			return (0);
		case MSG_SAVEDATAPOINTER:
		case MSG_RESTOREPOINTERS:
			/*
			 * We save pointers implicitely at disconnect.
			 * So we can ignore these messages.
			 */
			ack_message();
			PID("hmessage8");
			return (-1);
		case MSG_EXTENDED:
			nack_message(reqp, MSG_MESSAGE_REJECT);
			PID("hmessage9");
			return (-1);
		default:
			if ((msg & 0x80) && !(msg & 0x18)) {	/* IDENTIFY */
				PID("hmessage10");
				ack_message();
				return (0);
			} else {
				ncr_tprint(reqp,
					   "Unknown message %x.  Rejecting.\n",
					   msg);
				nack_message(reqp, MSG_MESSAGE_REJECT);
			}
			return (-1);
	}
	PID("hmessage11");
	return (-1);
}

/*
 * Handle reselection. If a valid reconnection occurs, connected
 * points at the reconnected command. The command is removed from the
 * disconnected queue.
 */
static void
reselect(sc)
struct ncr_softc *sc;
{
	u_char	phase;
	u_long	len;
	u_char	msg;
	u_char	target_mask;
	int	abort = 0;
	SC_REQ	*tmp, *prev;

	PID("reselect1");
	target_mask = GET_5380_REG(NCR5380_DATA) & ~SC_HOST_ID;

	/*
	 * At this point, we have detected that our SCSI-id is on the bus,
	 * SEL is true and BSY was false for at least one bus settle
	 * delay (400 ns.).
	 * We must assert BSY ourselves, until the target drops the SEL signal.
	 * The SCSI-spec specifies no maximum time for this, so we have to
	 * choose something long enough to suit all targets.
	 */
	SET_5380_REG(NCR5380_ICOM, SC_A_BSY);
	len = 250000;
	while ((GET_5380_REG(NCR5380_IDSTAT) & SC_S_SEL) && (len > 0)) {
		delay(1);
		len--;
	}
	if (GET_5380_REG(NCR5380_IDSTAT) & SC_S_SEL) {
		/* Damn SEL isn't dropping */
		scsi_reset_verbose(sc, "Target won't drop SEL during Reselect");
		return;
	}

	SET_5380_REG(NCR5380_ICOM, 0);
	
	/*
	 * Check if the reselection is still valid. Check twice because
	 * of possible line glitches - cheaper than delay(1) and we need
	 * only a few nanoseconds.
	 */
	if (!(GET_5380_REG(NCR5380_IDSTAT) & SC_S_BSY)) {
	    if (!(GET_5380_REG(NCR5380_IDSTAT) & SC_S_BSY)) {
		ncr_aprint(sc, "Stepped into the reselection timeout\n");
		return;
	    }
	}

	/*
	 * Get the expected identify message.
	 */
	phase = PH_MSGIN;
	len   = 1;
	transfer_pio(&phase, &msg, &len, 0);
	if (len || !MSG_ISIDENTIFY(msg)) {
		ncr_aprint(sc, "Expecting IDENTIFY, got 0x%x\n", msg);
		abort = 1;
		tmp = NULL;
	}
	else {
	    /*
	     * Find the command reconnecting
	     */
	    for (tmp = discon_q, prev = NULL; tmp; prev = tmp, tmp = tmp->next){
		if (target_mask == (1 << tmp->targ_id)) {
			if (prev)
				prev->next = tmp->next;
			else discon_q = tmp->next;
			tmp->next = NULL;
			break;
		}
	    }
	    if (tmp == NULL) {
		ncr_aprint(sc, "No disconnected job for targetmask %x\n",
								target_mask);
		abort = 1;
	    }
	}
	if (abort) {
		msg   = MSG_ABORT;
		len   = 1;
		phase = PH_MSGOUT;

		SET_5380_REG(NCR5380_ICOM, SC_A_ATN);
		if (transfer_pio(&phase, &msg, &len, 0) || len)
			scsi_reset_verbose(sc, "Failure to abort reselection");
	}
	else {
		connected = tmp;
#ifdef DBG_REQ
		if (dbg_target_mask & (1 << tmp->targ_id))
			show_request(tmp, "RECON");
#endif
	}
	PID("reselect2");
}

/*
 * Transfer data in a given phase using programmed I/O.
 * Returns -1 when a different phase is entered without transferring the
 * maximum number of bytes, 0 if all bytes transferred or exit is in the same
 * phase.
 */
static int
transfer_pio(phase, data, len, dont_drop_ack)
u_char	*phase;
u_char	*data;
u_long	*len;
int	dont_drop_ack;
{
	u_int	cnt = *len;
	u_char	ph  = *phase;
	u_char	tmp, new_icom;

	DBG_PIOPRINT ("SCSI: transfer_pio start: phase: %d, len: %d\n", ph,cnt);
	PID("tpio1");
	SET_5380_REG(NCR5380_TCOM, ph);
	do {
	    if (!wait_req_true()) {
		DBG_PIOPRINT ("SCSI: transfer_pio: missing REQ\n", 0, 0);
		break;
	    }
	    if (((GET_5380_REG(NCR5380_IDSTAT) >> 2) & 7) != ph) {
		DBG_PIOPRINT ("SCSI: transfer_pio: phase mismatch\n", 0, 0);
		break;
	    }
	    if (PH_IN(ph)) {
		*data++ = GET_5380_REG(NCR5380_DATA);
		SET_5380_REG(NCR5380_ICOM, SC_A_ACK);
		if ((cnt == 1) && dont_drop_ack)
			new_icom = SC_A_ACK;
		else new_icom = 0;
	    }
	    else {
		SET_5380_REG(NCR5380_DATA, *data++);

		/*
		 * The SCSI-standard suggests that in the 'MESSAGE OUT' phase,
		 * the initiator should drop ATN on the last byte of the
		 * message phase after REQ has been asserted for the handshake
		 * but before the initiator raises ACK.
		 */
		if (!( (ph == PH_MSGOUT) && (cnt > 1) )) {
			SET_5380_REG(NCR5380_ICOM, SC_ADTB);
			SET_5380_REG(NCR5380_ICOM, SC_ADTB | SC_A_ACK);
			new_icom = 0;
		}
		else {
			SET_5380_REG(NCR5380_ICOM, SC_ADTB | SC_A_ATN);
			SET_5380_REG(NCR5380_ICOM, SC_ADTB|SC_A_ATN|SC_A_ACK);
			new_icom = SC_A_ATN;
		}
	    }
	    if (!wait_req_false()) {
		DBG_PIOPRINT ("SCSI: transfer_pio - REQ not dropping\n", 0, 0);
		break;
	    }
	    SET_5380_REG(NCR5380_ICOM, new_icom);

	} while (--cnt);

	if ((tmp = GET_5380_REG(NCR5380_IDSTAT)) & SC_S_REQ)
		*phase = (tmp >> 2) & 7;
	else *phase = NR_PHASE;
	*len = cnt;
	DBG_PIOPRINT ("SCSI: transfer_pio done: phase: %d, len: %d\n",
								*phase, cnt);
	PID("tpio2");
	if (!cnt || (*phase == ph))
		return (0);
	return (-1);
}

#ifdef REAL_DMA
/*
 * Start a DMA-transfer on the device using the current pointers.
 * If 'poll' is true, the function busy-waits until DMA has completed.
 */
static void
transfer_dma(reqp, phase, poll)
SC_REQ	*reqp;
u_int	phase;
int	poll;
{
	int	dma_done;
	u_char	mbase = 0;
	int	sps;

again:
	PID("tdma1");

	/*
	 * We should be in phase, otherwise we are not allowed to
	 * drive the bus.
	 */
	SET_5380_REG(NCR5380_TCOM, phase);

	/*
	 * Defer interrupts until DMA is fully running.
	 */
	sps = splbio();

	/*
	 * Clear pending interrupts and parity errors.
	 */
	scsi_clr_ipend();

	if (!poll) {
		/*
		 * Enable SCSI interrupts and set IN_DMA flag, set 'mbase'
		 * to the interrupts we want enabled.
		 */
		scsi_ienable();
		reqp->dr_flag |= DRIVER_IN_DMA;
		mbase = SC_E_EOPI | SC_MON_BSY;
	}
	else scsi_idisable();
	mbase |=  IMODE_BASE | SC_M_DMA;
	scsi_dma_setup(reqp, phase, mbase);

	splx(sps);

	if (poll) {
		/*
		 * On polled-dma transfers, we wait here until the
		 * 'end-of-dma' condition occurs.
		 */
		poll_edma(reqp);
		if (!(dma_done = dma_ready()))
			goto again;
	}
	PID("tdma2");
}

/*
 * Check results of a DMA data-transfer.
 */
static int
dma_ready()
{
	SC_REQ	*reqp = connected;
	int	dmstat, is_edma;
	long	bytes_left, bytes_done;

	is_edma = get_dma_result(reqp, &bytes_left);
	dmstat  = GET_5380_REG(NCR5380_DMSTAT);

	/*
	 * Check if the call is sensible and not caused by any spurious
	 * interrupt.
	 */
	if (!is_edma && !(dmstat & (SC_END_DMA|SC_BSY_ERR))
		     && (dmstat & SC_PHS_MTCH) ) {
		ncr_tprint(reqp, "dma_ready: spurious call "
				 "(dm:%x,last_hit: %s)\n",
#ifdef DBG_PID
					dmstat, last_hit[DBG_PID-1]);
#else
					dmstat, "unknown");
#endif
		return (0);
	}

	/*
	 * Clear all (pending) interrupts.
	 */
	scsi_clr_ipend();

	/*
	 * Update various transfer-pointers/lengths
	 */
	bytes_done = reqp->dm_cur->dm_count - bytes_left;

	if ((reqp->dr_flag & DRIVER_BOUNCING) && (PH_IN(reqp->phase))) {
		/*
		 * Copy the bytes read until now from the bounce buffer
		 * to the 'real' destination. Flush the data-cache
		 * before copying.
		 */
		PCIA();
		bcopy(reqp->bouncerp, reqp->xdata_ptr, bytes_done);
		reqp->bouncerp += bytes_done;
	}

	reqp->xdata_ptr  = &reqp->xdata_ptr[bytes_done];	/* XXX */
	reqp->xdata_len -= bytes_done;				/* XXX */
	if ((reqp->dm_cur->dm_count -= bytes_done) == 0)
		reqp->dm_cur++;
	else reqp->dm_cur->dm_addr += bytes_done;

	if (PH_IN(reqp->phase) && (dmstat & SC_PAR_ERR)) {
		if (!(ncr5380_no_parchk & (1 << reqp->targ_id))) {
			ncr_tprint(reqp, "parity error in data-phase\n");
			reqp->xs->error = XS_TIMEOUT;
		}
	}

	/*
	 * DMA mode should always be reset even when we will continue with the
	 * next chain. It is also essential to clear the MON_BUSY because
	 * when LOST_BUSY is unexpectedly set, we will not be able to drive
	 * the bus....
	 */
	SET_5380_REG(NCR5380_MODE, IMODE_BASE);


	if ((dmstat & SC_BSY_ERR) || !(dmstat & SC_PHS_MTCH)
		 || (reqp->dm_cur > reqp->dm_last) || (reqp->xs->error)) {

		/*
		 * Tell interrupt functions DMA mode has ended.
		 */
		reqp->dr_flag &= ~DRIVER_IN_DMA;

		/*
		 * Clear mode and icom
		 */
		SET_5380_REG(NCR5380_MODE, IMODE_BASE);
		SET_5380_REG(NCR5380_ICOM, 0);

		if (dmstat & SC_BSY_ERR) {
			if (!reqp->xs->error)
				reqp->xs->error = XS_TIMEOUT;
			finish_req(reqp);
			PID("dma_ready1");
			return (1);
		}

		if (reqp->xs->error != 0) {
ncr_tprint(reqp, "dma-ready: code = %d\n", reqp->xs->error); /* LWP */
			reqp->msgout = MSG_ABORT;
			SET_5380_REG(NCR5380_ICOM, SC_A_ATN);
		}
		PID("dma_ready2");
		return (1);
	}
	return (0);
}
#endif /* REAL_DMA */

static int
check_autosense(reqp, linked)
SC_REQ	*reqp;
int	linked;
{
	int	sps;

	/*
	 * If we not executing an auto-sense and the status code
	 * is request-sense, we automatically issue a request
	 * sense command.
	 */
	PID("cautos1");
	if (!(reqp->dr_flag & DRIVER_AUTOSEN)) {
		switch (reqp->status & SCSMASK) {
		    case SCSCHKC:
			bcopy(sense_cmd, &reqp->xcmd, sizeof(sense_cmd));
			reqp->xdata_ptr = (u_char *)&reqp->xs->sense;
			reqp->xdata_len = sizeof(reqp->xs->sense);
			reqp->dr_flag  |= DRIVER_AUTOSEN;
			reqp->dr_flag  &= ~DRIVER_DMAOK;
			if (!linked) {
				sps = splbio();
				reqp->next = issue_q;
				issue_q    = reqp;
				splx(sps);
			}
			else reqp->xcmd.bytes[sizeof(sense_cmd)-2] |= 1;

#ifdef DBG_REQ
			bzero(reqp->xdata_ptr, reqp->xdata_len);
			if (dbg_target_mask & (1 << reqp->targ_id))
				show_request(reqp, "AUTO-SENSE");
#endif
			PID("cautos2");
			return (-1);
		    case SCSBUSY:
			reqp->xs->error = XS_BUSY;
			return (0);
		}
	}
	else {
		/*
		 * An auto-sense has finished
		 */
		if ((reqp->status & SCSMASK) != SCSGOOD)
			reqp->xs->error = XS_DRIVER_STUFFUP; /* SC_E_AUTOSEN; */
		else reqp->xs->error = XS_SENSE;
		reqp->status = SCSCHKC;
	}
	PID("cautos3");
	return (0);
}

static int
reach_msg_out(sc, len)
struct ncr_softc *sc;
u_long		 len;
{
	u_char	phase;
	u_char	data;
	u_long	n = len;

	ncr_aprint(sc, "Trying to reach Message-out phase\n");
	if ((phase = GET_5380_REG(NCR5380_IDSTAT)) & SC_S_REQ)
		phase = (phase >> 2) & 7;
	else return (-1);
	ncr_aprint(sc, "Trying to reach Message-out phase, now: %d\n", phase);
	if (phase == PH_MSGOUT)
		return (0);

	SET_5380_REG(NCR5380_TCOM, phase);

	do {
		if (!wait_req_true())
			break;
		if (((GET_5380_REG(NCR5380_IDSTAT) >> 2) & 7) != phase)
			break;
		if (PH_IN(phase)) {
			data = GET_5380_REG(NCR5380_DATA);
			SET_5380_REG(NCR5380_ICOM, SC_A_ACK | SC_A_ATN);
		}
		else {
			SET_5380_REG(NCR5380_DATA, 0);
			SET_5380_REG(NCR5380_ICOM, SC_ADTB|SC_A_ACK|SC_A_ATN);
		}
		if (!wait_req_false())
			break;
		SET_5380_REG(NCR5380_ICOM, SC_A_ATN);
	} while (--n);

	if ((phase = GET_5380_REG(NCR5380_IDSTAT)) & SC_S_REQ) {
		phase = (phase >> 2) & 7;
		if (phase == PH_MSGOUT) {
			ncr_aprint(sc, "Message-out phase reached after "
					"%ld bytes.\n", len - n);
			return (0);
		}
	}
	return (-1);
}

void
scsi_reset()
{
	SC_REQ	*tmp, *next;
	int	sps;

	PID("scsi_reset1");
	sps = splbio();
	SET_5380_REG(NCR5380_ICOM, SC_A_RST);
	delay(100);
	SET_5380_REG(NCR5380_ICOM, 0);
	scsi_clr_ipend();

	/*
	 * None of the jobs in the discon_q will ever be reconnected,
	 * notify this to the higher level code.
	 */
	for (tmp = discon_q; tmp ;) {
		next = tmp->next;
		tmp->next = NULL;
		tmp->xs->error = XS_TIMEOUT;
		busy &= ~(1 << tmp->targ_id);
		finish_req(tmp);
		tmp = next;
	}
	discon_q = NULL;

	/*
	 * The current job will never finish either.
	 * The problem is that we can't finish the job because an instance
	 * of main is running on it. Our best guess is that the job is currently
	 * doing REAL-DMA. In that case 'dma_ready()' should correctly finish
	 * the job because it detects BSY-loss.
	 */
	if ((tmp = connected) != NULL) {
		if (tmp->dr_flag & DRIVER_IN_DMA) {
			tmp->xs->error = XS_DRIVER_STUFFUP;
#ifdef REAL_DMA
			dma_ready();
#endif
		}
	}
	splx(sps);
	PID("scsi_reset2");

	/*
	 * Give the attached devices some time to handle the reset. This
	 * value is arbitrary but should be relatively long.
	 */
	delay(100000);
}

static void
scsi_reset_verbose(sc, why)
struct ncr_softc *sc;
const char	 *why;
{
	ncr_aprint(sc, "Resetting SCSI-bus (%s)\n", why);

	scsi_reset();
}

/*
 * Check validity of the IRQ set by the 5380. If the interrupt is valid,
 * the appropriate action is carried out (reselection or DMA ready) and
 * INTR_RESEL or INTR_DMA is returned. Otherwise a console notice is written
 * and INTR_SPURIOUS is returned.
 */
static int
check_intr(sc)
struct ncr_softc *sc;
{
	SC_REQ	*reqp;

	if ((GET_5380_REG(NCR5380_IDSTAT) & (SC_S_SEL|SC_S_IO))
							==(SC_S_SEL|SC_S_IO))
		return (INTR_RESEL);
	else {
		if ((reqp = connected) && (reqp->dr_flag & DRIVER_IN_DMA)){
			reqp->dr_flag &= ~DRIVER_IN_DMA;
			return (INTR_DMA);
		}
	}
	scsi_clr_ipend();
	printf("-->");
	scsi_show();
	ncr_aprint(sc, "Spurious interrupt.\n");
	return (INTR_SPURIOUS);
}

#ifdef REAL_DMA
/*
 * Check if DMA can be used for this request. This function also builds
 * the dma-chain.
 */
static int
scsi_dmaok(reqp)
SC_REQ	*reqp;
{
	u_long			phy_buf;
	u_long			phy_len;
	void			*req_addr;
	u_long			req_len;
	struct dma_chain	*dm;

	/*
	 * Initialize locals and requests' DMA-chain.
	 */
	req_len        = reqp->xdata_len;
	req_addr       = (void *)reqp->xdata_ptr;
	dm             = reqp->dm_cur = reqp->dm_last = reqp->dm_chain;
	dm->dm_count   = dm->dm_addr = 0;
	reqp->dr_flag &= ~DRIVER_BOUNCING;

	/*
	 * Do not accept zero length DMA.
	 */
	if (req_len == 0)
		return (0);

	/*
	 * LWP: I think that this restriction is not strictly necessary.
	 */
	if ((req_len & 0x1) || ((u_int)req_addr & 0x3))
		return (0);

	/*
	 * Build the DMA-chain.
	 */
	dm->dm_addr = phy_buf = kvtop(req_addr);
	while (req_len) {
		if (req_len < (phy_len = NBPG - m68k_page_offset(req_addr)))
			phy_len = req_len;

		req_addr     += phy_len;
		req_len      -= phy_len;
		dm->dm_count += phy_len;

		if (req_len) {
			u_long	tmp = kvtop(req_addr);

			if ((phy_buf + phy_len) != tmp) {
			    if (wrong_dma_range(reqp, dm)) {
				if (reqp->dr_flag & DRIVER_BOUNCING)
					goto bounceit;
				return (0);
			    }

			    if (++dm >= &reqp->dm_chain[MAXDMAIO]) {
				ncr_tprint(reqp,"dmaok: DMA chain too long!\n");
				return (0);
			    }
			    dm->dm_count = 0;
			    dm->dm_addr  = tmp;
			}
			phy_buf = tmp;
		}
	}
        if (wrong_dma_range(reqp, dm)) {
		if (reqp->dr_flag & DRIVER_BOUNCING)
			goto bounceit;
		return (0);
	}
	reqp->dm_last = dm;
	return (1);

bounceit:
	if ((reqp->bounceb = alloc_bounceb(reqp->xdata_len)) == NULL) {
		/*
		 * If we can't get a bounce buffer, forget DMA
		 */
		reqp->dr_flag &= ~DRIVER_BOUNCING;
		return(0);
	}
	/*
	 * Initialize a single DMA-range containing the bounced request
	 */
	dm = reqp->dm_cur = reqp->dm_last = reqp->dm_chain;
	dm->dm_addr    = kvtop(reqp->bounceb);
	dm->dm_count   = reqp->xdata_len;
	reqp->bouncerp = reqp->bounceb;

	return (1);
}
#endif /* REAL_DMA */

static void
run_main(sc)
struct ncr_softc *sc;
{
	int	sps = splbio();

	if (!main_running) {
		/*
		 * If shared resources are required, claim them
		 * before entering 'scsi_main'. If we can't get them
		 * now, assume 'run_main' will be called when the resource
		 * becomes available.
		 */
		if (!claimed_dma()) {
			splx(sps);
			return;
		}
		main_running = 1;
		splx(sps);
		scsi_main(sc);
	}
	else splx(sps);
}

/*
 * Prefix message with full target info.
 */
static void
ncr_tprint(SC_REQ *reqp, char *fmt, ...)
{
	va_list	ap;

	sc_print_addr(reqp->xs->sc_link);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

/*
 * Prefix message with adapter info.
 */
static void
ncr_aprint(struct ncr_softc *sc, char *fmt, ...)
{
	va_list	ap;

	printf("%s: ", sc->sc_dev.dv_xname);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}
/****************************************************************************
 *		Start Debugging Functions				    *
 ****************************************************************************/
static void
show_data_sense(xs)
struct scsi_xfer	*xs;
{
	u_char	*p1, *p2;
	int	i;
	int	sz;

	p1 = (u_char *) xs->cmd;
	p2 = (u_char *)&xs->sense;
	if(*p2 == 0)
		return;	/* No(n)sense */
	printf("cmd[%d,%d]: ", xs->cmdlen, sz = command_size(*p1));
	for (i = 0; i < sz; i++)
		printf("%x ", p1[i]);
	printf("\nsense: ");
	for (i = 0; i < sizeof(xs->sense); i++)
		printf("%x ", p2[i]);
	printf("\n");
}

static void
show_request(reqp, qtxt)
SC_REQ	*reqp;
char	*qtxt;
{
	printf("REQ-%s: %d %p[%ld] cmd[0]=%x S=%x M=%x R=%x resid=%d dr_flag=%x %s\n",
			qtxt, reqp->targ_id, reqp->xdata_ptr, reqp->xdata_len,
			reqp->xcmd.opcode, reqp->status, reqp->message,
			reqp->xs->error, reqp->xs->resid, reqp->dr_flag,
			reqp->link ? "L":"");
	if (reqp->status == SCSCHKC)
		show_data_sense(reqp->xs);
}

static char *sig_names[] = {
	"PAR", "SEL", "I/O", "C/D", "MSG", "REQ", "BSY", "RST",
	"ACK", "ATN", "LBSY", "PMATCH", "IRQ", "EPAR", "DREQ", "EDMA"
};

static void
show_signals(dmstat, idstat)
u_char	dmstat, idstat;
{
	u_short	tmp, mask;
	int	j, need_pipe;

	tmp = idstat | ((dmstat & 3) << 8);
	printf("Bus signals (%02x/%02x): ", idstat, dmstat & 3);
	for (mask = 1, j = need_pipe = 0; mask <= tmp; mask <<= 1, j++) {
		if (tmp & mask)
			printf("%s%s", need_pipe++ ? "|" : "", sig_names[j]);
	}
	printf("\nDma status (%02x): ", dmstat);
	for (mask = 4, j = 10, need_pipe = 0; mask <= dmstat; mask <<= 1, j++) {
		if (dmstat & mask)
			printf("%s%s", need_pipe++ ? "|" : "", sig_names[j]);
	}
	printf("\n");
}

void
scsi_show()
{
	SC_REQ	*tmp;
	int	sps = splhigh();
	u_char	idstat, dmstat;
#ifdef	DBG_PID
	int	i;
#endif

	printf("scsi_show: scsi_main is%s running\n",
		main_running ? "" : " not");
	for (tmp = issue_q; tmp; tmp = tmp->next)
		show_request(tmp, "ISSUED");
	for (tmp = discon_q; tmp; tmp = tmp->next)
		show_request(tmp, "DISCONNECTED");
	if (connected)
		show_request(connected, "CONNECTED");
	idstat = GET_5380_REG(NCR5380_IDSTAT);
	dmstat = GET_5380_REG(NCR5380_DMSTAT);
	show_signals(dmstat, idstat);
	if (connected)
		printf("phase = %d, ", connected->phase);
	printf("busy:%x, spl:%04x\n", busy, sps);
#ifdef	DBG_PID
	for (i=0; i<DBG_PID; i++)
		printf("\t%d\t%s\n", i, last_hit[i]);
#endif

	splx(sps);
}
