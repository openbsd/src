/*	$OpenBSD: ncr5380.c,v 1.4 2001/08/26 00:45:08 fgsch Exp $	*/
/*	$NetBSD: ncr5380.c,v 1.3 1995/09/26 21:04:27 pk Exp $	*/

/*
 * Copyright (C) 1994 Adam Glass, Gordon W. Ross
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NOTE: This is 99% Sun 3 `si' driver.  I've made the probe and attach
 * routines a little minimalistic, and adjusted them slightly for the Sun 4.
 * Also, fixed some logic in ncr5380_select_target().
 *	Jason R. Thorpe <thorpej@nas.nasa.gov>
 */

static int
ncr5380_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	int flags, s, r;

	flags = xs->flags;
	if (xs->bp) flags |= (SCSI_NOSLEEP);
	if ( flags & ITSDONE ) {
		printf("Already done?");
		xs->flags &= ~ITSDONE;
	}

	s = splbio();

	if ( flags & SCSI_RESET ) {
		printf("flags & SCSIRESET.\n");
		ncr5380_reset_scsibus(xs->sc_link->adapter_softc);
		r = COMPLETE;
	} else {
		r = ncr5380_send_cmd(xs);
		xs->flags |= ITSDONE;
		scsi_done(xs);
	}

	splx(s);

	switch(r) {
	case COMPLETE:
	case SUCCESSFULLY_QUEUED:
		r = SUCCESSFULLY_QUEUED;
		if (xs->flags & SCSI_POLL)
			r = COMPLETE;
		break;
	default:
		break;
	}
	return r;
}

#ifdef	DEBUG
static int
ncr5380_show_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	u_char	*b = (u_char *) xs->cmd;
	int	i  = 0;

	if ( ! ( xs->flags & SCSI_RESET ) ) {
		printf("si(%d:%d:%d)-",
			   xs->sc_link->scsibus,
			   xs->sc_link->target,
			   xs->sc_link->lun);
		while (i < xs->cmdlen) {
			if (i) printf(",");
			printf("%x",b[i++]);
		}
		printf("-\n");
	} else {
		printf("si(%d:%d:%d)-RESET-\n",
			   xs->sc_link->scsibus,
			   xs->sc_link->target,
			   xs->sc_link->lun);
	}
}
#endif

/*
 * Actual chip control.
 */

static void
ncr5380_sbc_intr(ncr5380)
	struct ncr5380_softc *ncr5380;
{
	volatile sci_regmap_t *regs = ncr5380->sc_regs;

	if ((regs->sci_csr & SCI_CSR_INT) == 0) {
#ifdef	DEBUG
		printf (" ncr5380_sbc_intr: spurious\n");
#endif
		return;
	}

	SCI_CLR_INTR(regs);
#ifdef	DEBUG
	printf (" ncr5380_sbc_intr\n");
#endif
}

static int
ncr5380_reset_scsibus(ncr5380)
	struct ncr5380_softc *ncr5380;
{
	volatile sci_regmap_t *regs = ncr5380->sc_regs;

#ifdef	DEBUG
	if (ncr5380_debug) {
		printf("ncr5380_reset_scsibus\n");
	}
#endif

	regs->sci_icmd = SCI_ICMD_RST;
	delay(100);
	regs->sci_icmd = 0;

	regs->sci_mode = 0;
	regs->sci_tcmd = SCI_PHASE_DISC;
	regs->sci_sel_enb = 0;

	SCI_CLR_INTR(regs);
	/* XXX - Need long delay here! */
}

static int
ncr5380_poll(adapter, timeout)
	int adapter, timeout;
{
}

static int
ncr5380_send_cmd(xs)
	struct scsi_xfer *xs;
{
	int	sense;

#ifdef	DEBUG
	if (ncr5380_debug & 2)
		ncr5380_show_scsi_cmd(xs);
#endif

	sense = ncr5380_generic( xs->sc_link->adapter_softc,
	    xs->sc_link->target, xs->sc_link->lun, xs->cmd,
	    xs->cmdlen, xs->data, xs->datalen );

	switch (sense) {
	case 0:	/* success */
		xs->resid = 0;
		xs->error = XS_NOERROR;
		break;

	case 0x02:	/* Check condition */
#ifdef	DEBUG
		if (ncr5380_debug)
			printf("check cond. target %d.\n",
				   xs->sc_link->target);
#endif
		delay(10);	/* Phil's fix for slow devices. */
		ncr5380_group0(xs->sc_link->adapter_softc,
				  xs->sc_link->target,
				  xs->sc_link->lun,
				  0x3, 0x0,
				  sizeof(struct scsi_sense_data),
				  0, (caddr_t) &(xs->sense),
				  sizeof(struct scsi_sense_data));
		xs->error = XS_SENSE;
		break;
	case 0x08:	/* Busy - common code will delay, retry. */
		xs->error = XS_BUSY;
		break;
	default:	/* Dead - tell common code to give up. */
		xs->error = XS_DRIVER_STUFFUP;
		break;

	}
	return (COMPLETE);
}

static int
ncr5380_select_target(regs, myid, tid, with_atn)
	register volatile sci_regmap_t *regs;
	u_char myid, tid;
	int with_atn;
{
	register u_char	bid, icmd;
	int		ret = SCSI_RET_RETRY;
	int 	arb_retries, arb_wait;
	int i;

	/* for our purposes.. */
	myid = 1 << myid;
	tid = 1 << tid;

	regs->sci_sel_enb = 0; /* we don't want any interrupts. */
	regs->sci_tcmd = 0;	/* get into a harmless state */

	arb_retries = ARBITRATION_RETRIES;

retry_arbitration:
	regs->sci_mode = 0;	/* get into a harmless state */
wait_for_bus_free:
	if (--arb_retries <= 0) {
#ifdef	DEBUG
		if (ncr5380_debug) {
			printf("ncr5380_select: arb_retries expended; resetting...\n");
		}
#endif
		ret = SCSI_RET_NEED_RESET;
		goto nosel;
	}

	icmd = regs->sci_icmd & ~(SCI_ICMD_DIFF|SCI_ICMD_TEST);

	if (regs->sci_bus_csr & (SCI_BUS_BSY|SCI_BUS_SEL)) {
		/* Something is sitting on the SCSI bus... */
#ifdef DEBUG
		/* Only complain once (the last time through). */
		if (ncr5380_debug && (arb_retries <= 1)) {
			printf("si_select_target: still BSY+SEL\n");
		}
#endif
		/* Give it a little time, then try again. */
		delay(10);
		goto wait_for_bus_free;
	}

	regs->sci_odata = myid;
	regs->sci_mode = SCI_MODE_ARB;
/*	regs->sci_mode |= SCI_MODE_ARB;	XXX? */

	/* AIP might not set if BSY went true after we checked */
	/* Wait up to about 100 usec. for it to appear. */
	arb_wait = 50;	/* X2 */
	do {
		if (regs->sci_icmd & SCI_ICMD_AIP)
			goto got_aip;
		delay(2);
	} while (--arb_wait > 0);
	/* XXX - Could have missed it? */
#ifdef	DEBUG
	if (ncr5380_debug)
		printf("ncr5380_select_target: API did not appear\n");
#endif
	goto retry_arbitration;

	got_aip:
#ifdef	DEBUG
	if (ncr5380_debug & 4) {
		printf("ncr5380_select_target: API after %d tries (last wait %d)\n",
			   ARBITRATION_RETRIES - arb_retries,
			   (50 - arb_wait));
	}
#endif

	delay(3);	/* 2.2 uSec. arbitration delay */

	if (regs->sci_icmd & SCI_ICMD_LST) {
#ifdef	DEBUG
		if (ncr5380_debug)
			printf ("lost 1\n");
#endif
		goto retry_arbitration;	/* XXX */
	}

	regs->sci_mode &= ~SCI_MODE_PAR_CHK;
	bid = regs->sci_data;

	if ((bid & ~myid) > myid) {
#ifdef	DEBUG
		if (ncr5380_debug)
			printf ("lost 2\n");
#endif
		/* Trying again will not help. */
		goto lost;
	}
	if (regs->sci_icmd & SCI_ICMD_LST) {
#ifdef	DEBUG
		if (ncr5380_debug)
			printf ("lost 3\n");
#endif
		goto lost;
	}

	/* Won arbitration, enter selection phase now */	
	icmd = regs->sci_icmd & ~(SCI_ICMD_DIFF|SCI_ICMD_TEST);
	icmd |= (with_atn ? (SCI_ICMD_SEL|SCI_ICMD_ATN) : SCI_ICMD_SEL);
	regs->sci_icmd = icmd;

	if (regs->sci_icmd & SCI_ICMD_LST) {
#ifdef	DEBUG
		if (ncr5380_debug)
			printf ("nosel\n");
#endif
		goto nosel;
	}

	/* XXX a target that violates specs might still drive the bus XXX */
	/* XXX should put our id out, and after the delay check nothi XXX */
	/* XXX ng else is out there.				      XXX */

	delay(3);

	regs->sci_sel_enb = 0;

	regs->sci_odata = myid | tid;

	icmd |= SCI_ICMD_BSY|SCI_ICMD_DATA;
	regs->sci_icmd = icmd;

/*	regs->sci_mode &= ~SCI_MODE_ARB;	 2 deskew delays, too */
	regs->sci_mode = 0;			/* 2 deskew delays, too */
	
	icmd &= ~SCI_ICMD_BSY;
	regs->sci_icmd = icmd;

	/* bus settle delay, 400ns */
	delay(3);

	regs->sci_mode |= SCI_MODE_PAR_CHK;

	{
		register int timeo  = 2500;/* 250 msecs in 100 usecs chunks */
		while ((regs->sci_bus_csr & SCI_BUS_BSY) == 0) {
			if (--timeo > 0) {
				delay(100);
			} else {
				/* This is the "normal" no-such-device select error. */
#ifdef	DEBUG
				if (ncr5380_debug)
					printf("ncr5380_select: not BSY (nothing there)\n");
#endif
				goto nodev;
			}
		}
	}

	icmd &= ~(SCI_ICMD_DATA|SCI_ICMD_SEL);
	regs->sci_icmd = icmd;
/*	regs->sci_sel_enb = myid;*/	/* looks like we should NOT have it */
	/* XXX - SCI_MODE_PAR_CHK ? */
	return SCSI_RET_SUCCESS;

nodev:
	ret = SCSI_RET_DEVICE_DOWN;
	regs->sci_sel_enb = myid;
nosel:
	regs->sci_icmd = 0;
	regs->sci_mode = 0;
	return ret;

lost:
	regs->sci_icmd = 0;
	regs->sci_mode = 0;
#ifdef	DEBUG
	if (ncr5380_debug) {
		printf("ncr5380_select: lost arbitration\n");
	}
#endif
	return ret;
}

sci_data_out(regs, phase, count, data)
	register volatile sci_regmap_t	*regs;
	unsigned char		*data;
{
	register unsigned char	icmd;
	register int		cnt=0;

	/* ..checks.. */

	icmd = regs->sci_icmd & ~(SCI_ICMD_DIFF|SCI_ICMD_TEST);
loop:
	/* SCSI bus phase not valid until REQ is true. */
	WAIT_FOR_REQ(regs);
	if (SCI_CUR_PHASE(regs->sci_bus_csr) != phase)
		return cnt;

	icmd |= SCI_ICMD_DATA;
	regs->sci_icmd = icmd;
	regs->sci_odata = *data++;
	icmd |= SCI_ICMD_ACK;
	regs->sci_icmd = icmd;

	icmd &= ~(SCI_ICMD_DATA|SCI_ICMD_ACK);
	WAIT_FOR_NOT_REQ(regs);
	regs->sci_icmd = icmd;
	++cnt;
	if (--count > 0)
		goto loop;
scsi_timeout_error:
	return cnt;
}

static int
sci_data_in(regs, phase, count, data)
	register volatile sci_regmap_t	*regs;
	unsigned char			*data;
{
	register unsigned char	icmd;
	register int		cnt=0;

	/* ..checks.. */

	icmd = regs->sci_icmd & ~(SCI_ICMD_DIFF|SCI_ICMD_TEST);

loop:
	/* SCSI bus phase not valid until REQ is true. */
	WAIT_FOR_REQ(regs);
	if (SCI_CUR_PHASE(regs->sci_bus_csr) != phase)
		return cnt;

	*data++ = regs->sci_data;
	icmd |= SCI_ICMD_ACK;
	regs->sci_icmd = icmd;

	icmd &= ~SCI_ICMD_ACK;
	WAIT_FOR_NOT_REQ(regs);
	regs->sci_icmd = icmd;
	++cnt;
	if (--count > 0)
		goto loop;

scsi_timeout_error:
	return cnt;
}

/*
 * Return -1 (error) or number of bytes sent (>=0).
 */
static int
ncr5380_command_transfer(regs, maxlen, data, status, msg)
	register volatile sci_regmap_t *regs;
	int maxlen;
	u_char *data;
	u_char *status;
	u_char *msg;
{
	int	xfer, phase;

	xfer = 0;
	regs->sci_icmd = 0;

	while (1) {

		WAIT_FOR_REQ(regs);

		phase = SCI_CUR_PHASE(regs->sci_bus_csr);

		switch (phase) {
			case SCSI_PHASE_CMD:
				SCI_ACK(regs,SCSI_PHASE_CMD);
				xfer += sci_data_out(regs, SCSI_PHASE_CMD,
						   	maxlen, data);
				goto out;

			case SCSI_PHASE_DATA_IN:
				printf("command_transfer: Data in phase?\n");
				goto err;

			case SCSI_PHASE_DATA_OUT:
				printf("command_transfer: Data out phase?\n");
				goto err;

			case SCSI_PHASE_STATUS:
				SCI_ACK(regs,SCSI_PHASE_STATUS);
				printf("command_transfer: status in...\n");
				sci_data_in(regs, SCSI_PHASE_STATUS,
					  	1, status);
				printf("command_transfer: status=0x%x\n", *status);
				goto err;

			case SCSI_PHASE_MESSAGE_IN:
				SCI_ACK(regs,SCSI_PHASE_MESSAGE_IN);
				printf("command_transfer: msg in?\n");
				sci_data_in(regs, SCSI_PHASE_MESSAGE_IN,
					  	1, msg);
				break;

			case SCSI_PHASE_MESSAGE_OUT:
				SCI_ACK(regs,SCSI_PHASE_MESSAGE_OUT);
				sci_data_out(regs, SCSI_PHASE_MESSAGE_OUT,
					  	1, msg);
				break;

			default:
				printf("command_transfer: Unexpected phase 0x%x\n", phase);
				goto err;
		}
	}
scsi_timeout_error:
err:
	xfer = -1;
out:
	return xfer;
}

static int
ncr5380_data_transfer(regs, maxlen, data, status, msg)
	register volatile sci_regmap_t *regs;
	int maxlen;
	u_char *data, *status, *msg;
{
	int	retlen = 0, xfer, phase;

	regs->sci_icmd = 0;

	*status = 0;

	while (1) {

		WAIT_FOR_REQ(regs);

		phase = SCI_CUR_PHASE(regs->sci_bus_csr);

		switch (phase) {
			case SCSI_PHASE_CMD:
				printf("Command phase in data_transfer().\n");
				return retlen;
			case SCSI_PHASE_DATA_IN:
				SCI_ACK(regs,SCSI_PHASE_DATA_IN);
#if PSEUDO_DMA
				xfer = sci_pdma_in(regs, SCSI_PHASE_DATA_IN,
						  	maxlen, data);
#else
				xfer = sci_data_in(regs, SCSI_PHASE_DATA_IN,
						  	maxlen, data);
#endif
				retlen += xfer;
				maxlen -= xfer;
				break;
			case SCSI_PHASE_DATA_OUT:
				SCI_ACK(regs,SCSI_PHASE_DATA_OUT);
#if PSEUDO_DMA
				xfer = sci_pdma_out(regs, SCSI_PHASE_DATA_OUT,
						   	maxlen, data);
#else
				xfer = sci_data_out(regs, SCSI_PHASE_DATA_OUT,
						   	maxlen, data);
#endif
				retlen += xfer;
				maxlen -= xfer;
				break;
			case SCSI_PHASE_STATUS:
				SCI_ACK(regs,SCSI_PHASE_STATUS);
				sci_data_in(regs, SCSI_PHASE_STATUS,
					  	1, status);
				break;
			case SCSI_PHASE_MESSAGE_IN:
				SCI_ACK(regs,SCSI_PHASE_MESSAGE_IN);
				sci_data_in(regs, SCSI_PHASE_MESSAGE_IN,
					  	1, msg);
				if (*msg == 0) {
					return retlen;
				} else {
					printf( "message 0x%x in "
						"data_transfer.\n", *msg);
				}
				break;
			case SCSI_PHASE_MESSAGE_OUT:
				SCI_ACK(regs,SCSI_PHASE_MESSAGE_OUT);
				sci_data_out(regs, SCSI_PHASE_MESSAGE_OUT,
					  	1, msg);
				break;
			default:
				printf( "Unexpected phase 0x%x in "
					"data_transfer().\n", phase);
scsi_timeout_error:
				return retlen;
				break;
		}
	}
}

/*
 * Returns 0 on success, -1 on internal error, or the status byte
 */
static int
ncr5380_dorequest(sc, target, lun, cmd, cmdlen, databuf, datalen, sent)
	struct ncr5380_softc *sc;
	int target, lun;
	u_char *cmd;
	int cmdlen;
	char *databuf;
	int datalen, *sent;
{
	register volatile sci_regmap_t *regs = sc->sc_regs;
	int	cmd_bytes_sent, r;
	u_char	stat, msg, c;

#ifdef	DEBUG
	if (ncr5380_debug) {
		printf("ncr5380_dorequest: target=%d, lun=%d\n", target, lun);
	}
#endif

	*sent = 0;

	if ( ( r = ncr5380_select_target(regs, 7, target, 1) ) != SCSI_RET_SUCCESS) {
#ifdef	DEBUG
		if (ncr5380_debug) {
			printf("ncr5380_dorequest: select returned %d\n", r);
		}
#endif

		SCI_CLR_INTR(regs);
		switch (r) {

		case SCSI_RET_NEED_RESET:
			printf("ncr5380_dorequest: target=%d, lun=%d, r=%d resetting...\n",
				   target, lun, r);
			reset_adapter(sc);
			ncr5380_reset_scsibus(sc);
			/* fall through */
		case SCSI_RET_RETRY:
			return 0x08;	/* Busy - tell common code to retry. */

		default:
			printf("ncr5380_dorequest: target=%d, lun=%d, error=%d.\n",
				target, lun, r);
			/* fall through */
		case SCSI_RET_DEVICE_DOWN:
			return -1;	/* Dead - tell common code to give up. */
		}
	}

	c = 0x80 | lun;

	if ((cmd_bytes_sent = ncr5380_command_transfer(regs, cmdlen,
				(u_char *) cmd, &stat, &c)) != cmdlen)
	{
		SCI_CLR_INTR(regs);
		if (cmd_bytes_sent >= 0) {
			printf("Data underrun sending CCB (%d bytes of %d, sent).\n",
				   cmd_bytes_sent, cmdlen);
		}
		return -1;
	}

	*sent = ncr5380_data_transfer(regs, datalen, (u_char *)databuf,
					&stat, &msg);
#ifdef	DEBUG
	if (ncr5380_debug) {
		printf("ncr5380_dorequest: data transferred = %d\n", *sent);
	}
#endif

	return stat;
}

static int
ncr5380_generic(adapter, id, lun, cmd, cmdlen, databuf, datalen)
	void *adapter;
	int id, lun;
	struct scsi_generic *cmd;
	int cmdlen;
	void *databuf;
	int datalen;
{
	register struct ncr5380_softc *sc = adapter;
	int i, j, sent;

	if (cmd->opcode == TEST_UNIT_READY)	/* XXX */
		cmd->bytes[0] = ((u_char) lun << 5);

	i = ncr5380_dorequest(sc, id, lun, (u_char *) cmd, cmdlen,
					 databuf, datalen, &sent);

	return i;
}

static int
ncr5380_group0(adapter, id, lun, opcode, addr, len, flags, databuf, datalen)
	void *adapter;
	int id, lun, opcode, addr, len, flags;
	caddr_t databuf;
	int datalen;
{
	register struct ncr5380_softc *sc = adapter;
	unsigned char cmd[6];
	int i, j, sent;

	cmd[0] = opcode;		/* Operation code */
	cmd[1] = (lun << 5) | ((addr >> 16) & 0x1F); /* Lun & MSB of addr */
	cmd[2] = (addr >> 8) & 0xFF;	/* addr */
	cmd[3] = addr & 0xFF;		/* LSB of addr */
	cmd[4] = len;			/* Allocation length */
	cmd[5] = flags;			/* Link/Flag */

	i = ncr5380_dorequest(sc, id, lun, cmd, 6, databuf, datalen, &sent);

	return i;
}
