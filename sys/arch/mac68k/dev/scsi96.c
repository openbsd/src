/*	$NetBSD: scsi96.c,v 1.21 1996/10/13 03:21:29 christos Exp $	*/

/*
 * Copyright (C) 1994	Allen K. Briggs
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

#include <machine/scsi96reg.h>
#include <machine/viareg.h>

/* Support for the NCR 53C96 SCSI processor--primarily for '040 Macs. */

#ifndef DDB
#define Debugger() panic("Should call Debugger here (mac/dev/scsi96.c).")
#endif

extern vm_offset_t SCSIBase;
static volatile unsigned char *ncr53c96base;

struct ncr53c96_softc {
	struct device sc_dev;

	void   *reg_base;
	int     adapter_target;
	struct scsi_link sc_link;
};
#define WAIT_FOR(reg, val) { \
	int	timeo=10000; \
	while (!(reg & val)) { \
		delay(100); \
		if (!(--timeo)) { \
			printf("scsi96: WAIT_FOR timeout.\n"); \
			goto have_error; \
		} \
	} \
}

static void ncr53c96_minphys(struct buf * bp);
static int ncr53c96_scsi_cmd(struct scsi_xfer * xs);

/* static int ncr53c96_show_scsi_cmd(struct scsi_xfer * xs); */
static int ncr53c96_reset_target(int adapter, int target);
static int ncr53c96_poll(int adapter, int timeout);
static int ncr53c96_send_cmd(struct scsi_xfer * xs);
static int scsiprint __P((void *, const char *));
static void resetchip __P((void));

struct scsi_adapter ncr53c96_switch = {
	ncr53c96_scsi_cmd,	/* scsi_cmd()		 */
	ncr53c96_minphys,	/* scsi_minphys()	 */
	0,			/* open_target_lu()	 */
	0,			/* close_target_lu()	 */
};
/* This is copied from julian's bt driver */
/* "so we have a default dev struct for our link struct." */
struct scsi_device ncr53c96_dev = {
	NULL,			/* Use default error handler.	    */
	NULL,			/* have a queue, served by this (?) */
	NULL,			/* have no async handler.	    */
	NULL,			/* Use default "done" routine.	    */
};

static int	ncr96probe __P((struct device *, void *, void *));
static void	ncr96attach __P((struct device *, struct device *, void *));

struct cfattach ncr96scsi_ca = {
	sizeof(struct ncr53c96_softc), ncr96probe, ncr96attach
};

struct cfdriver ncr96scsi_cd = {
	NULL, "ncr96scsi", DV_DULL, NULL, 0
};

static int
ncr96probe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	static int probed = 0;

	if (!mac68k_machine.scsi96) {
		return 0;
	}

	if (!probed) {
		probed = 1;
		ncr53c96base = (volatile u_char *)SCSIBase;
	}
	return 1;
}

static int
scsiprint(auxp, name)
	void		*auxp;
	const char	*name;
{
	if (name == NULL)
		return (UNCONF);
	return (QUIET);
}

static void
ncr96attach(parent, dev, aux)
	struct device *parent, *dev;
	void   *aux;
{
	struct ncr53c96_softc *ncr53c96;

	ncr53c96 = (struct ncr53c96_softc *) dev;

	ncr53c96->sc_link.adapter_softc = ncr53c96;
	ncr53c96->sc_link.adapter_target = 7;
	ncr53c96->sc_link.adapter = &ncr53c96_switch;
	ncr53c96->sc_link.device = &ncr53c96_dev;
	ncr53c96->sc_link.openings = 1;
#ifdef SCSIDEBUG
	ncr53c96->sc_link.flags = 0 /* SDEV_DB1 | SDEV_DB2 | SDEV_DB3 | SDEV_DB4 */ ;
#else
	ncr53c96->sc_link.flags = 0;
#endif

	resetchip();

	printf("\n");

	config_found(dev, &(ncr53c96->sc_link), scsiprint);

	/*
	 * Enable IRQ and DRQ interrupts.
	via_reg(VIA2, vIER) = (V2IF_IRQ | V2IF_SCSIDRQ | V2IF_SCSIIRQ);
	 */
}

#define MIN_PHYS	65536	/* BARF!!!! */
static void
ncr53c96_minphys(struct buf * bp)
{
	if (bp->b_bcount > MIN_PHYS) {
		printf("Uh-oh...  ncr53c96_minphys setting bp->b_bcount "
		    "= %x.\n", MIN_PHYS);
		bp->b_bcount = MIN_PHYS;
	}
	minphys(bp);
}
#undef MIN_PHYS

static int
ncr53c96_scsi_cmd(struct scsi_xfer * xs)
{
	int     flags, s, r;

	flags = xs->flags;
	if (xs->bp)
		flags |= (SCSI_NOSLEEP);
	if (flags & ITSDONE) {
		printf("Already done?");
		xs->flags &= ~ITSDONE;
	}
	if (!(flags & INUSE)) {
		printf("Not in use?");
		xs->flags |= INUSE;
	}
	if (flags & SCSI_RESET) {
		printf("flags & SCSIRESET.\n");
		if (!(flags & SCSI_NOSLEEP)) {
			s = splbio();
			ncr53c96_reset_target(xs->sc_link->scsibus,
			    xs->sc_link->target);
			splx(s);
			return (SUCCESSFULLY_QUEUED);
		} else {
			ncr53c96_reset_target(xs->sc_link->scsibus,
			    xs->sc_link->target);
			if (ncr53c96_poll(xs->sc_link->scsibus, xs->timeout)) {
				return (COMPLETE);
			}
			return (COMPLETE);
		}
	}
	/*
	 * OK.  Now that that's over with, let's pack up that
	 * SCSI puppy and send it off.  If we can, we'll just
	 * queue and go; otherwise, we'll wait for the command
	 * to finish.
	if ( ! ( flags & SCSI_NOSLEEP ) ) {
		s = splbio();
		ncr53c96_send_cmd(xs);
		splx(s);
		return(SUCCESSFULLY_QUEUED);
	}
	 */

	r = ncr53c96_send_cmd(xs);
	xs->flags |= ITSDONE;
	scsi_done(xs);
	switch (r) {
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
/*
	do {
		if (ncr53c96_poll(xs->sc_link->scsibus, xs->timeout)) {
			if ( ! ( xs->flags & SCSI_SILENT ) )
				printf("cmd fail.\n");
			cmd_cleanup
			xs->error = XS_DRIVER_STUFFUP;
			splx(s);
		}
	} while ( ! ( xs->flags & ITSDONE ) );
*/
}

#if 0
static int
ncr53c96_show_scsi_cmd(struct scsi_xfer * xs)
{
	u_char *b = (u_char *) xs->cmd;
	int     i = 0;

	if (!(xs->flags & SCSI_RESET)) {
		printf("ncr53c96(%d:%d:%d)-",
		    xs->sc_link->scsibus, xs->sc_link->target,
		    xs->sc_link->lun);
		while (i < xs->cmdlen) {
			if (i)
				printf(",");
			printf("%x", b[i++]);
		}
		printf("-\n");
	} else {
		printf("ncr53c96(%d:%d:%d)-RESET-\n",
		    xs->sc_link->scsibus, xs->sc_link->target,
		    xs->sc_link->lun);
	}
	return 0;
}
#endif

/*
 * Actual chip control.
 */

extern void	ncr53c96_intr __P((int));

extern void
ncr53c96_intr(int adapter)
{
}

extern int	ncr53c96_irq_intr __P((void));

extern int
ncr53c96_irq_intr(void)
{
	printf("irq\n");
	return 1;
}

extern int	ncr53c96_drq_intr __P((void));
extern int
ncr53c96_drq_intr(void)
{
	printf("drq\n");
	return 1;
}

static int
ncr53c96_reset_target(int adapter, int target)
{
return 0;
}

static int
ncr53c96_poll(int adapter, int timeout)
{
return 0;
}

static void
resetchip(void)
{
	struct ncr53c96regs *ncr = (struct ncr53c96regs *) ncr53c96base;

	ncr->cmdreg = NCR96_CMD_RESETDEV;
	delay(25);
	ncr->cmdreg = NCR96_CMD_NOOP;
	delay(25);
	ncr->clkfactorreg = 5;	/* Is this right? */
	delay(25);

	ncr->soffsetreg = 0;    /* make sure we use async xfers */
	ncr->stimreg = 0x99;	/* Value corresponding to clkfactorreg of 5 */
	ncr->ctrlreg1 = 0x7;	/* Our ID */
	ncr->ctrlreg2 = 0;
	ncr->ctrlreg3 = NCR96_C3_LBTM;
}

static int
do_send_cmd(struct scsi_xfer * xs)
{
	struct ncr53c96regs *ncr = (struct ncr53c96regs *) ncr53c96base;
	u_char *cmd;
	int     i, stat, is, intr;
	int     msg, phase, ds=0;

	xs->resid = 0;
	i = (int) ncr->statreg;	/* clear interrupts */
	i = (int) ncr->instreg;
	ncr->cmdreg = NCR96_CMD_CLRFIFO;	/* and fifo */

	ncr->sdidreg = xs->sc_link->target;
	ncr->fifo = MSG_IDENTIFY(xs->sc_link->lun, 0);
	cmd = (u_char *) xs->cmd;
	for (i = 0; i < xs->cmdlen; i++)
		ncr->fifo = *cmd++;
	ncr->tcreg_lsb = xs->cmdlen + 1;
	ncr->tcreg_msb = 0;
	ncr->cmdreg = NCR96_CMD_SELATN;

	WAIT_FOR(ncr->statreg, NCR96_STAT_INT);

	stat = ncr->statreg;
	is = ncr->isreg;
	intr = ncr->instreg;
	if ((is & 0x07) != 0x4 || intr != 0x18) {
		if ((is & 0x7) == 2 && intr == 0x18) {
			/* Target selected, but invalid LUN */
			resetchip();
			xs->error = XS_SELTIMEOUT;
			return COMPLETE;
		} else if ((is & 0x7) != 0x0 || intr != 0x20) {
			printf("scsi96: stat = 0x%x, is = 0x%x, intr = 0x%x\n",
			    stat, is, intr);
			goto have_error;
		}
		xs->error = XS_SELTIMEOUT;
		return COMPLETE;
	}
	ds = 1;
/*
	printf("scsi96: before loop: stat = 0x%x, is = 0x%x, intr = 0x%x, "
	    "datalen = %d\n", stat, is, intr, xs->datalen);
*/
	phase = ncr->statreg & NCR96_STAT_PHASE;
	if (((phase == 0x01) || (phase == 0x00)) && xs->datalen) {
		/* printf("data = %p, datalen = 0x%x.\n", xs->data, xs->datalen); */
		stat = ncr->statreg;
		is = ncr->isreg;
		intr = ncr->instreg;
		/* printf("entering info xfer...stat = 0x%x, is = 0x%x, intr = 0x%x\n",
		    stat, is, intr); */
		ncr->tcreg_lsb = (xs->datalen & 0xff);
		ncr->tcreg_msb = (xs->datalen >> 8) & 0xff;
/*		ncr->cmdreg = 0x80 | NCR96_CMD_INFOXFER; */
		/* printf("rem... %d.\n", ncr->tcreg_lsb | (ncr->tcreg_msb << 8)); */
		i = 0;
		while (i < xs->datalen) {
			int     d, stat, newphase;

			ncr->cmdreg = NCR96_CMD_INFOXFER;

			if (phase == 1) {
				WAIT_FOR(ncr->statreg, NCR96_STAT_INT);

				stat = ncr->statreg;
				newphase = stat & NCR96_STAT_PHASE;

		        	d = ncr->fifostatereg & NCR96_CF_MASK;
			  
				while (d--) {
			  		xs->data[i++] = ncr->fifo;
				/* printf("0x%x,", xs->data[i - 1]);*/
				}
			} else {
				/* printf("out %02x ", xs->data[i]); */
				ncr->fifo = xs->data[i++];
				WAIT_FOR(ncr->statreg, NCR96_STAT_INT);

				stat = ncr->statreg;
				newphase = stat & NCR96_STAT_PHASE;
			}

			intr = ncr->instreg;

#if 0
			if (phase == 0)
				printf("in loop.  stat = 0x%x, intr = 0x%x\n",
				    stat, intr);
#endif

			if ((i != xs->datalen) && (phase != newphase)) {
				intr = ncr->instreg;
				if (phase == 0)
					printf("Unexpected phase change during data out from %d to %d, got %d of %d\n", phase, newphase, i, xs->datalen);
				break;
			}

		}
/*	} else {
		WAIT_FOR(ncr->statreg, NCR96_STAT_INT); */
	}
	stat = ncr->statreg;
	is = ncr->isreg;
	intr = ncr->instreg;
/*
	printf("past loop...stat = 0x%x, is = 0x%x, intr = 0x%x\n",
	    stat, is, intr);
*/

	phase = stat & NCR96_STAT_PHASE;
	if (phase != 3)
	  printf("Expected to be in phase 3, but in phase %d\n", phase);

	ncr->cmdreg = NCR96_CMD_ICCS;

	WAIT_FOR(ncr->statreg, NCR96_STAT_INT);

	stat = ncr->statreg;
	is = ncr->isreg;
	intr = ncr->instreg;

	xs->status = ncr->fifo;
	msg = ncr->fifo;

/*
	printf("got Status %d, Message %d, stat = 0x%x, is = 0x%x, intr = 0x%x\n", xs->status, msg, stat, is, intr);
*/

	ncr->cmdreg = NCR96_CMD_MSGACC;

	WAIT_FOR(ncr->statreg, NCR96_STAT_INT);

	stat = ncr->statreg;
	is = ncr->isreg;
	intr = ncr->instreg;
	if ((intr == 0x20) && (stat & 0x80)) {
		xs->error = XS_NOERROR;
		return COMPLETE;
	}

have_error:
	printf("Driver stuffup...stat=0x%x, is=0x%x, intr=0x%x, ds=%d\n",
	    stat, is, intr, ds);
	resetchip();
	xs->error = XS_DRIVER_STUFFUP;
	return COMPLETE;
}

static int
ncr53c96_send_cmd(struct scsi_xfer * xs)
{
	return do_send_cmd(xs);
}
