/*	$NetBSD: sci.c,v 1.15 1996/01/07 22:01:56 thorpej Exp $	*/

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
 *	@(#)scsi.c	7.5 (Berkeley) 5/4/91
 */

/*
 * AMIGA NCR 5380 scsi adaptor driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <machine/pmap.h>
#include <machine/cpu.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/scireg.h>
#include <amiga/dev/scivar.h>

/*
 * SCSI delays
 * In u-seconds, primarily for state changes on the SPC.
 */
#define	SCI_CMD_WAIT	50000	/* wait per step of 'immediate' cmds */
#define	SCI_DATA_WAIT	50000	/* wait per data in/out step */
#define	SCI_INIT_WAIT	50000	/* wait per step (both) during init */

#define	b_cylin		b_resid

int  sciicmd __P((struct sci_softc *, int, void *, int, void *, int,u_char));
int  scigo __P((struct sci_softc *, struct scsi_xfer *));
int  scigetsense __P((struct sci_softc *, struct scsi_xfer *));
int  sciselectbus __P((struct sci_softc *, u_char, u_char));
void sciabort __P((struct sci_softc *, char *));
void scierror __P((struct sci_softc *, u_char));
void scireset __P((struct sci_softc *));
void scisetdelay __P((int));
void sci_scsidone __P((struct sci_softc *, int));
void sci_donextcmd __P((struct sci_softc *));

int sci_cmd_wait = SCI_CMD_WAIT;
int sci_data_wait = SCI_DATA_WAIT;
int sci_init_wait = SCI_INIT_WAIT;

int sci_no_dma = 0;

#ifdef DEBUG
#define QPRINTF(a) if (sci_debug > 1) printf a
int	sci_debug = 0;
#else
#define QPRINTF
#endif

/*
 * default minphys routine for sci based controllers
 */
void
sci_minphys(bp)
	struct buf *bp;
{

	/*
	 * No max transfer at this level.
	 */
	minphys(bp);
}

/*
 * used by specific sci controller
 *
 * it appears that the higher level code does nothing with LUN's
 * so I will too.  I could plug it in, however so could they
 * in scsi_scsi_cmd().
 */
int
sci_scsicmd(xs)
	struct scsi_xfer *xs;
{
	struct sci_pending *pendp;
	struct sci_softc *dev;
	struct scsi_link *slp;
	int flags, s;

	slp = xs->sc_link;
	dev = slp->adapter_softc;
	flags = xs->flags;

	if (flags & SCSI_DATA_UIO)
		panic("sci: scsi data uio requested");

	if (dev->sc_xs && flags & SCSI_POLL)
		panic("sci_scsicmd: busy");

	s = splbio();
	pendp = &dev->sc_xsstore[slp->target][slp->lun];
	if (pendp->xs) {
		splx(s);
		return(TRY_AGAIN_LATER);
	}

	if (dev->sc_xs) {
		pendp->xs = xs;
		TAILQ_INSERT_TAIL(&dev->sc_xslist, pendp, link);
		splx(s);
		return(SUCCESSFULLY_QUEUED);
	}
	pendp->xs = NULL;
	dev->sc_xs = xs;
	splx(s);

	/*
	 * nothing is pending do it now.
	 */
	sci_donextcmd(dev);

	if (flags & SCSI_POLL)
		return(COMPLETE);
	return(SUCCESSFULLY_QUEUED);
}

/*
 * entered with dev->sc_xs pointing to the next xfer to perform
 */
void
sci_donextcmd(dev)
	struct sci_softc *dev;
{
	struct scsi_xfer *xs;
	struct scsi_link *slp;
	int flags, phase, stat;

	xs = dev->sc_xs;
	slp = xs->sc_link;
	flags = xs->flags;

	if (flags & SCSI_DATA_IN)
		phase = DATA_IN_PHASE;
	else if (flags & SCSI_DATA_OUT)
		phase = DATA_OUT_PHASE;
	else
		phase = STATUS_PHASE;

	if (flags & SCSI_RESET)
		scireset(dev);

	dev->sc_stat[0] = -1;
	xs->cmd->bytes[0] |= slp->lun << 5;
	if (phase == STATUS_PHASE || flags & SCSI_POLL)
		stat = sciicmd(dev, slp->target, xs->cmd, xs->cmdlen,
		    xs->data, xs->datalen, phase);
	else if (scigo(dev, xs) == 0)
		return;
	else
		stat = dev->sc_stat[0];

	sci_scsidone(dev, stat);
}

void
sci_scsidone(dev, stat)
	struct sci_softc *dev;
	int stat;
{
	struct sci_pending *pendp;
	struct scsi_xfer *xs;
	int s, donext;

	xs = dev->sc_xs;
#ifdef DIAGNOSTIC
	if (xs == NULL)
		panic("sci_scsidone");
#endif
	/*
	 * XXX Support old-style instrumentation for now.
	 * IS THIS REALLY THE RIGHT PLACE FOR THIS?  --thorpej
	 */
	if (xs->sc_link->device_softc &&
	    ((struct device *)(xs->sc_link->device_softc))->dv_unit < dk_ndrive)
		++dk_xfer[((struct device *)(xs->sc_link->device_softc))->dv_unit];
	/*
	 * is this right?
	 */
	xs->status = stat;

	if (stat == 0)
		xs->resid = 0;
	else {
		switch(stat) {
		case SCSI_CHECK:
			if (stat = scigetsense(dev, xs))
				goto bad_sense;
			xs->error = XS_SENSE;
			break;
		case SCSI_BUSY:
			xs->error = XS_BUSY;
			break;
		bad_sense:
		default:
			xs->error = XS_DRIVER_STUFFUP;
			QPRINTF(("sci_scsicmd() bad %x\n", stat));
			break;
		}
	}
	xs->flags |= ITSDONE;

	/*
	 * grab next command before scsi_done()
	 * this way no single device can hog scsi resources.
	 */
	s = splbio();
	pendp = dev->sc_xslist.tqh_first;
	if (pendp == NULL) {
		donext = 0;
		dev->sc_xs = NULL;
	} else {
		donext = 1;
		TAILQ_REMOVE(&dev->sc_xslist, pendp, link);
		dev->sc_xs = pendp->xs;
		pendp->xs = NULL;
	}
	splx(s);
	scsi_done(xs);

	if (donext)
		sci_donextcmd(dev);
}

int
scigetsense(dev, xs)
	struct sci_softc *dev;
	struct scsi_xfer *xs;
{
	struct scsi_sense rqs;
	struct scsi_link *slp;
	int stat;

	slp = xs->sc_link;

	rqs.opcode = REQUEST_SENSE;
	rqs.byte2 = slp->lun << 5;
#ifdef not_yet
	rqs.length = xs->req_sense_length ? xs->req_sense_length :
	    sizeof(xs->sense);
#else
	rqs.length = sizeof(xs->sense);
#endif

	rqs.unused[0] = rqs.unused[1] = rqs.control = 0;

	return(sciicmd(dev, slp->target, &rqs, sizeof(rqs), &xs->sense,
	    rqs.length, DATA_IN_PHASE));
}

void
sciabort(dev, where)
	struct sci_softc *dev;
	char *where;
{
	printf ("%s: abort %s: csr = 0x%02x, bus = 0x%02x\n",
	  dev->sc_dev.dv_xname, where, *dev->sci_csr, *dev->sci_bus_csr);

	if (dev->sc_flags & SCI_SELECTED) {

		/* lets just hope it worked.. */
		dev->sc_flags &= ~SCI_SELECTED;
		/* XXX */
		scireset (dev);
	}
}

/*
 * XXX Set/reset long delays.
 *
 * if delay == 0, reset default delays
 * if delay < 0,  set both delays to default long initialization values
 * if delay > 0,  set both delays to this value
 *
 * Used when a devices is expected to respond slowly (e.g. during
 * initialization).
 */
void
scisetdelay(del)
	int del;
{
	static int saved_cmd_wait, saved_data_wait;

	if (del) {
		saved_cmd_wait = sci_cmd_wait;
		saved_data_wait = sci_data_wait;
		if (del > 0)
			sci_cmd_wait = sci_data_wait = del;
		else
			sci_cmd_wait = sci_data_wait = sci_init_wait;
	} else {
		sci_cmd_wait = saved_cmd_wait;
		sci_data_wait = saved_data_wait;
	}
}

void
scireset(dev)
	struct sci_softc *dev;
{
	u_int i, s;
	u_char my_id, csr;

	dev->sc_flags &= ~SCI_SELECTED;
	if (dev->sc_flags & SCI_ALIVE)
		sciabort(dev, "reset");

	printf("%s: ", dev->sc_dev.dv_xname);

	s = splbio();
	/* preserve our ID for now */
	my_id = 7;

	/*
	 * Reset the chip
	 */
	*dev->sci_icmd = SCI_ICMD_TEST;
	*dev->sci_icmd = SCI_ICMD_TEST | SCI_ICMD_RST;
	delay (25);
	*dev->sci_icmd = 0;

	/*
	 * Set up various chip parameters
	 */
	*dev->sci_icmd = 0;
	*dev->sci_tcmd = 0;
	*dev->sci_sel_enb = 0;

	/* anything else was zeroed by reset */

	splx (s);

	printf("sci id %d\n", my_id);
	dev->sc_flags |= SCI_ALIVE;
}

void
scierror(dev, csr)
	struct sci_softc *dev;
	u_char csr;
{
	struct scsi_xfer *xs;

	xs = dev->sc_xs;

#ifdef DIAGNOSTIC
	if (xs == NULL)
		panic("scierror");
#endif
	if (xs->flags & SCSI_SILENT)
		return;

	printf("%s: ", dev->sc_dev.dv_xname);
	printf("csr == 0x%02i\n", csr);	/* XXX */
}

/*
 * select the bus, return when selected or error.
 */
int
sciselectbus(dev, target, our_addr)
        struct sci_softc *dev;
	u_char target, our_addr;
{
	register int timeo = 2500;

	QPRINTF (("sciselectbus %d\n", target));

	/* if we're already selected, return */
	if (dev->sc_flags & SCI_SELECTED)	/* XXXX */
		return 1;

	if ((*dev->sci_bus_csr & (SCI_BUS_BSY|SCI_BUS_SEL)) &&
	    (*dev->sci_bus_csr & (SCI_BUS_BSY|SCI_BUS_SEL)) &&
	    (*dev->sci_bus_csr & (SCI_BUS_BSY|SCI_BUS_SEL)))
		return 1;

	*dev->sci_tcmd = 0;
	*dev->sci_odata = 0x80 + (1 << target);
	*dev->sci_icmd = SCI_ICMD_DATA|SCI_ICMD_SEL;
	while ((*dev->sci_bus_csr & SCI_BUS_BSY) == 0) {
		if (--timeo > 0) {
			delay(100);
		} else {
			break;
		}
	}
	if (timeo) {
		*dev->sci_icmd = 0;
		dev->sc_flags |= SCI_SELECTED;
		return (0);
	}
	*dev->sci_icmd = 0;
	return (1);
}

int
sci_ixfer_out(dev, len, buf, phase)
	register struct sci_softc *dev;
	int len;
	register u_char *buf;
	int phase;
{
	register int wait = sci_data_wait;
	u_char csr;

	QPRINTF(("sci_ixfer_out {%d} %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
	  len, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
	  buf[6], buf[7], buf[8], buf[9]));

	*dev->sci_tcmd = phase;
	*dev->sci_icmd = SCI_ICMD_DATA;
	for (;len > 0; len--) {
		csr = *dev->sci_bus_csr;
		while (!(csr & SCI_BUS_REQ)) {
			if ((csr & SCI_BUS_BSY) == 0 || --wait < 0) {
#ifdef DEBUG
				if (sci_debug)
					printf("sci_ixfer_out fail: l%d i%x w%d\n",
					  len, csr, wait);
#endif
				return (len);
			}
			delay(1);
			csr = *dev->sci_bus_csr;
		}

		if (!(*dev->sci_csr & SCI_CSR_PHASE_MATCH))
			break;
		*dev->sci_odata = *buf;
		*dev->sci_icmd = SCI_ICMD_DATA|SCI_ICMD_ACK;
		buf++;
		while (*dev->sci_bus_csr & SCI_BUS_REQ);
		*dev->sci_icmd = SCI_ICMD_DATA;
	}

	QPRINTF(("sci_ixfer_out done\n"));
	return (0);
}

void
sci_ixfer_in(dev, len, buf, phase)
	struct sci_softc *dev;
	int len;
	register u_char *buf;
	int phase;
{
	int wait = sci_data_wait;
	u_char *obp = buf;
	u_char csr;
	volatile register u_char *sci_bus_csr = dev->sci_bus_csr;
	volatile register u_char *sci_data = dev->sci_data;
	volatile register u_char *sci_icmd = dev->sci_icmd;

	csr = *sci_bus_csr;

	QPRINTF(("sci_ixfer_in %d, csr=%02x\n", len, csr));

	*dev->sci_tcmd = phase;
	*sci_icmd = 0;
	for (;len > 0; len--) {
		csr = *sci_bus_csr;
		while (!(csr & SCI_BUS_REQ)) {
			if (!(csr & SCI_BUS_BSY) || --wait < 0) {
#ifdef DEBUG
				if (sci_debug)
					printf("sci_ixfer_in fail: l%d i%x w%d\n",
					len, csr, wait);
#endif
				return;
			}

			delay(1);
			csr = *sci_bus_csr;
		}

		if (!(*dev->sci_csr & SCI_CSR_PHASE_MATCH))
			break;
		*buf = *sci_data;
		*sci_icmd = SCI_ICMD_ACK;
		buf++;
		while (*sci_bus_csr & SCI_BUS_REQ);
		*sci_icmd = 0;
	}

	QPRINTF(("sci_ixfer_in {%d} %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
	  len, obp[0], obp[1], obp[2], obp[3], obp[4], obp[5],
	  obp[6], obp[7], obp[8], obp[9]));
}

/*
 * SCSI 'immediate' command:  issue a command to some SCSI device
 * and get back an 'immediate' response (i.e., do programmed xfer
 * to get the response data).  'cbuf' is a buffer containing a scsi
 * command of length clen bytes.  'buf' is a buffer of length 'len'
 * bytes for data.  The transfer direction is determined by the device
 * (i.e., by the scsi bus data xfer phase).  If 'len' is zero, the
 * command must supply no data.  'xferphase' is the bus phase the
 * caller expects to happen after the command is issued.  It should
 * be one of DATA_IN_PHASE, DATA_OUT_PHASE or STATUS_PHASE.
 */
int
sciicmd(dev, target, cbuf, clen, buf, len, xferphase)
	struct sci_softc *dev;
	void *cbuf, *buf;
	int clen, len;
	u_char xferphase;
{
	u_char phase, csr, asr;
	register int wait;

	/* select the SCSI bus (it's an error if bus isn't free) */
	if (sciselectbus (dev, target, dev->sc_scsi_addr))
		return -1;
	/*
	 * Wait for a phase change (or error) then let the device
	 * sequence us through the various SCSI phases.
	 */
	dev->sc_stat[0] = 0xff;
	dev->sc_msg[0] = 0xff;
	phase = CMD_PHASE;
	while (1) {
		wait = sci_cmd_wait;

		while ((*dev->sci_bus_csr & (SCI_BUS_REQ|SCI_BUS_BSY)) == SCI_BUS_BSY);

		QPRINTF((">CSR:%02x<", *dev->sci_bus_csr));
		if ((*dev->sci_bus_csr & SCI_BUS_REQ) == 0) {
			return -1;
		}
		phase = SCI_PHASE(*dev->sci_bus_csr);

		switch (phase) {
		case CMD_PHASE:
			if (sci_ixfer_out (dev, clen, cbuf, phase))
				goto abort;
			phase = xferphase;
			break;

		case DATA_IN_PHASE:
			if (len <= 0)
				goto abort;
			wait = sci_data_wait;
			sci_ixfer_in (dev, len, buf, phase);
			phase = STATUS_PHASE;
			break;

		case DATA_OUT_PHASE:
			if (len <= 0)
				goto abort;
			wait = sci_data_wait;
			if (sci_ixfer_out (dev, len, buf, phase))
				goto abort;
			phase = STATUS_PHASE;
			break;

		case MESG_IN_PHASE:
			dev->sc_msg[0] = 0xff;
			sci_ixfer_in (dev, 1, dev->sc_msg,phase);
			dev->sc_flags &= ~SCI_SELECTED;
			while (*dev->sci_bus_csr & SCI_BUS_BSY);
			goto out;
			break;

		case MESG_OUT_PHASE:
			phase = STATUS_PHASE;
			break;

		case STATUS_PHASE:
			sci_ixfer_in (dev, 1, dev->sc_stat, phase);
			phase = MESG_IN_PHASE;
			break;

		case BUS_FREE_PHASE:
			goto out;

		default:
		printf("sci: unexpected phase %d in icmd from %d\n",
		  phase, target);
		goto abort;
		}
#if 0
		if (wait <= 0)
			goto abort;
#endif
	}

abort:
	sciabort(dev, "icmd");
out:
	QPRINTF(("=STS:%02x=", dev->sc_stat[0]));
	return (dev->sc_stat[0]);
}

int
scigo(dev, xs)
	struct sci_softc *dev;
	struct scsi_xfer *xs;
{
	int i, count, target;
	u_char phase, csr, asr, cmd, *addr;
	int wait;

	target = xs->sc_link->target;
	count = xs->datalen;
	addr = xs->data;

	if (sci_no_dma)	{
		sciicmd (dev, target, (u_char *) xs->cmd, xs->cmdlen,
		  addr, count,
		  xs->flags & SCSI_DATA_IN ? DATA_IN_PHASE : DATA_OUT_PHASE);

		return (1);
	}

	/* select the SCSI bus (it's an error if bus isn't free) */
	if (sciselectbus (dev, target, dev->sc_scsi_addr))
		return -1;
	/*
	 * Wait for a phase change (or error) then let the device
	 * sequence us through the various SCSI phases.
	 */
	dev->sc_stat[0] = 0xff;
	dev->sc_msg[0] = 0xff;
	phase = CMD_PHASE;
	while (1) {
		while ((*dev->sci_bus_csr & (SCI_BUS_REQ|SCI_BUS_BSY)) ==
		  SCI_BUS_BSY);

		QPRINTF((">CSR:%02x<", *dev->sci_bus_csr));
		if ((*dev->sci_bus_csr & SCI_BUS_REQ) == 0) {
			goto abort;
		}
		phase = SCI_PHASE(*dev->sci_bus_csr);

		switch (phase) {
		case CMD_PHASE:
			if (sci_ixfer_out (dev, xs->cmdlen, xs->cmd, phase))
				goto abort;
			phase = xs->flags & SCSI_DATA_IN ? DATA_IN_PHASE : DATA_OUT_PHASE;
			break;

		case DATA_IN_PHASE:
			if (count <= 0)
				goto abort;
			/* XXX use psuedo DMA if available */
			if (count >= 128 && dev->dma_xfer_in)
				(*dev->dma_xfer_in)(dev, count, addr, phase);
			else
				sci_ixfer_in (dev, count, addr, phase);
			phase = STATUS_PHASE;
			break;

		case DATA_OUT_PHASE:
			if (count <= 0)
				goto abort;
			/* XXX use psuedo DMA if available */
			if (count >= 128 && dev->dma_xfer_out)
				(*dev->dma_xfer_out)(dev, count, addr, phase);
			else
				if (sci_ixfer_out (dev, count, addr, phase))
					goto abort;
			phase = STATUS_PHASE;
			break;

		case MESG_IN_PHASE:
			dev->sc_msg[0] = 0xff;
			sci_ixfer_in (dev, 1, dev->sc_msg,phase);
			dev->sc_flags &= ~SCI_SELECTED;
			while (*dev->sci_bus_csr & SCI_BUS_BSY);
			goto out;
			break;

		case MESG_OUT_PHASE:
			phase = STATUS_PHASE;
			break;

		case STATUS_PHASE:
			sci_ixfer_in (dev, 1, dev->sc_stat, phase);
			phase = MESG_IN_PHASE;
			break;

		case BUS_FREE_PHASE:
			goto out;

		default:
		printf("sci: unexpected phase %d in icmd from %d\n",
		  phase, target);
		goto abort;
		}
	}

abort:
	sciabort(dev, "go");
out:
	QPRINTF(("=STS:%02x=", dev->sc_stat[0]));
	return (1);
}
