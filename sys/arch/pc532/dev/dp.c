/*	$NetBSD: dp.c,v 1.8 1995/08/12 20:31:11 mycroft Exp $	*/

/* Written by Phil Nelson for the pc532.  Used source with the following
 * copyrights as a model.
 *
 *	dp.c:  A NCR DP8490 driver for the pc532.
 */

/*
 * (Mostly) Written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 */

/*
 * a FEW lines in this driver come from a MACH adaptec-disk driver
 * so the copyright below is included:
 *
 * Copyright 1990 by Open Software Foundation,
 * Grenoble, FRANCE
 *
 * 		All Rights Reserved
 *
 *   Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OSF or Open Software
 * Foundation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.
 *
 *   OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <machine/stdarg.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/dkbad.h>
#include <sys/disklabel.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/frame.h>
#include <machine/icu.h>

#include "device.h"
#include "dpreg.h"

#define DP_DEBUG 0

/* Some constants (may need to be changed!) */
#define DP_NSEG		16

int dpprobe(struct pc532_device *);
int dpattach(struct pc532_device *);
int dp_scsi_cmd(struct scsi_xfer *);
void dpminphys(struct buf *);
long int dp_adapter_info(int);
void dp_intr(void);
void dp_intr_work(void);

struct scsidevs *
scsi_probe(int masunit, struct scsi_switch *sw, int physid, int type, int want);

struct	pc532_driver dpdriver = {
	dpprobe, dpattach, "dp",
};

struct scsi_switch dp_switch = {
	"dp",
	dp_scsi_cmd,
	dpminphys,
	0,
	0,
	dp_adapter_info,
	0, 0, 0
};

/* Sense command. */
static u_char sense_cmd[] = { 3, 0, 0, 0, 0, 0};

/* Do we need to initialize. */
int	dp_needs_init = 1;

/* Do we need a reset . */
int	dp_needs_reset = 1;

/* SCSI phase we are currently in . . . */
int	dp_scsi_phase;

/* SCSI Driver state */
int	dp_dvr_state = DP_DVR_READY;

/* For polled error reporting. */
int	dp_intr_retval;

/* For counting the retries. */
int	dp_try_count;

/* Give the interrupt routine access to the current scsi_xfer info block. */
struct scsi_xfer *cur_xs = NULL;

/* names of phases for debug printouts */
const char *dp_phase_names[] = {
    "DATA OUT",
    "DATA IN",
    "CMD",
    "STATUS",
    "PHASE 4",
    "PHASE 5",
    "MSG OUT",
    "MSG IN",
};

/* Initial probe for a device.  If it is using the dp controller,
   just say yes so that attach can be the one to find the real drive. */

int dpprobe(struct pc532_device *dvp)
{
  /* If we call this, we need to add SPL_DP to the bio mask! */
  PL_bio |= SPL_DP;
  PL_zero |= PL_bio;

  if (dp_needs_init)
     dp_initialize();

  if (dp_needs_reset)
     dp_reset();

  /* All pc532s should have one, so we don't check ! :) */
  return (1);
}


int dpattach(struct pc532_device *dvp)
{
	int r;

	r = scsi_attach(0, 7, &dp_switch,
		&dvp->pd_drive, &dvp->pd_unit, dvp->pd_flags);

	return(r);
}

void dpminphys(struct buf *bp)
{

	if(bp->b_bcount > ((DP_NSEG - 1) * NBPG))
		bp->b_bcount = ((DP_NSEG - 1) * NBPG);
	minphys(bp);
}

long int dp_adapter_info(int unit)
{
	return (1);    /* only 1 outstanding request. */
}

#if DP_DEBUG
void
dp_print_stat1(u_char stat1)
{
    printf("stat1=");
    if ( stat1 & 0x80 ) printf(" /RST");
    if ( stat1 & 0x40 ) printf(" /BSY");
    if ( stat1 & 0x20 ) printf(" /REQ");
    if ( stat1 & 0x10 ) printf(" /MSG");
    if ( stat1 & 0x08 ) printf(" /CD");
    if ( stat1 & 0x04 ) printf(" /IO");
    if ( stat1 & 0x02 ) printf(" /SEL");
    if ( stat1 & 0x01 ) printf(" /DBP");
    printf("\n");
}

void
dp_print_stat2(u_char stat2)
{
    printf("stat2=");
    if ( stat2 & 0x80 ) printf(" EDMA");
    if ( stat2 & 0x40 ) printf(" DRQ");
    if ( stat2 & 0x20 ) printf(" SPER");
    if ( stat2 & 0x10 ) printf(" INT");
    if ( stat2 & 0x08 ) printf(" PHSM");
    if ( stat2 & 0x04 ) printf(" BSY");
    if ( stat2 & 0x02 ) printf(" /ATN");
    if ( stat2 & 0x01 ) printf(" /ACK");
    printf("\n");
}
#endif

#if DP_DEBUG
void
dp_print_regs()
{
    u_char stat1 = RD_ADR(u_char, DP_STAT1);
    u_char stat2 = RD_ADR(u_char, DP_STAT2);
    dp_print_stat1(stat1);
    dp_print_stat2(stat2);
}
#endif

/* Do a scsi command. */

int dp_scsi_cmd(struct scsi_xfer *xs)
{
	struct	iovec	 *iovp;
	int	flags;
	int	retval;		/* Return values from functions. */
	int	x;		/* for splbio()  & splhigh() */
        int	dvr_state;
	int	ti_val;		/* For timeouts. */

#if DP_DEBUG
	printf("\n");
#endif
	x = splhigh();
	if (dp_dvr_state == DP_DVR_READY)
		dp_dvr_state = DP_DVR_STARTED;
	dvr_state = dp_dvr_state;
	splx(x);

	if (dvr_state != DP_DVR_STARTED)
	  return (TRY_AGAIN_LATER);

	cur_xs = xs;

	/* Some initial checks. */
	flags = xs->flags;
	if (!(flags & INUSE)) {
#if DP_DEBUG
		printf("dp xs not in use!\n");
#endif
		xs->flags |= INUSE;
	}
	if (flags & ITSDONE) {
#if DP_DEBUG
		printf("dp xs already done!\n");
#endif
		xs->flags &= ~ITSDONE;
	}
	if (dp_needs_reset || (xs->flags & SCSI_RESET))
		dp_reset();
#if DP_DEBUG
	printf ("scsi_cmd: flags=0x%x, targ=%d, lu=%d, cmdlen=%d, cmd=%x\n",
		xs->flags, xs->targ, xs->lu, xs->cmdlen, xs->cmd->opcode);
#endif
#if 1
	/* we don't always get NOMASK passed in, so this hack fakes it. */
	{
		if ( !initproc )
			xs->flags |= SCSI_NOMASK;
	}
#endif
	if (!(xs->flags & SCSI_NOMASK))  {
		x = splbio();
		retval = dp_start_cmd(xs);
		splx(x);
		return retval;
	}
	/* No interrupts available! */
	retval = dp_start_cmd(xs);
	if (retval != SUCCESSFULLY_QUEUED)
		return retval;
#if DP_DEBUG > 1
	printf("polling for interrupts\n");
#endif
	ti_val = WAIT_MUL * xs->timeout;
	while (dp_dvr_state != DP_DVR_READY) {
		if (RD_ADR(u_char, DP_STAT2) & DP_S_IRQ) {
			dp_intr_work();
			ti_val = WAIT_MUL * xs->timeout;
			retval = dp_intr_retval;
		}
		if (--ti_val == 0) {
			/* Software Timeout! */
			xs->error = XS_SWTIMEOUT;
			dp_dvr_state = DP_DVR_READY;
			retval = HAD_ERROR;
		}
	}
	if (xs->error == XS_SWTIMEOUT) {
		/* Software Timeout! */
		printf ("scsi timeout!\n");
#if DP_DEBUG
		dp_print_regs();
		printf("TCMD = 0x%x\n", RD_ADR(u_char, DP_TCMD));
#endif
		dp_reset();
	}
#if 1
	/* another hack: read cannot handle anything but SUCCESSFULLY_QUEUED */
	if (xs->cmd->opcode == 0x28)
		return SUCCESSFULLY_QUEUED;
#endif
	return retval;
}

/*===========================================================================*
 *				dp_intr					     *
 *===========================================================================*/

/* This is where a lot of the work happens!  This is called in non-interrupt
   mode when an interrupt would have happened.  It is also the real interrupt
   routine.  It uses dp_dvr_state to determine the next actions along with
   cur_xs->flags.  */

void dp_intr(void)
{
	int x = splhigh();
#if DP_DEBUG
	printf("\n REAL dp_intr\n");
#endif
	dp_intr_work();
	splx(x);
}

void dp_intr_work(void)
{
	u_char isr;
	u_char new_phase;
	u_char stat1;
	u_char stat2;
	static u_char status;
	static u_char message;
	int ret;

	scsi_select_ctlr (DP8490);
	WR_ADR(u_char, DP_EMR_ISR, DP_EF_ISR_NEXT);
	isr = RD_ADR (u_char, DP_EMR_ISR);
	dp_clear_isr();

	stat1 = RD_ADR(u_char, DP_STAT1);
	new_phase = (stat1 >> 2) & 7;

#if DP_DEBUG
	printf ("dp_intr: dvr_state %d isr=0x%x new_phase = %d %s\n",
		dp_dvr_state, isr, new_phase, dp_phase_names[new_phase]);
#if DP_DEBUG > 1
	dp_print_regs();
#endif
#endif
#if 0
	/* de-assert the bus */
	WR_ADR(u_char, DP_ICMD, DP_EMODE);
	/* disable dma */
	RD_ADR(u_char, DP_MODE) &= ~DP_M_DMA;
#endif
	if (isr & DP_ISR_BSYERR) {
#if DP_DEBUG
		printf("dp_intr: Busy error\n");
#endif
	}
	if (isr & DP_ISR_EDMA) {
#if DP_DEBUG > 1
		printf("dp_intr: EDMA detected\n");
#endif
		RD_ADR(u_char, DP_MODE) &= ~DP_M_DMA;
		WR_ADR(u_char, DP_ICMD, DP_EMODE);
	}
        if (!(isr & DP_ISR_APHS)) {
#if DP_DEBUG > 1
	 	printf("dp_intr: Not an APHS!\n");
		printf("dp_intr: dvr_state %d isr=0x%x (exit)\n",
			dp_dvr_state, isr);
#endif
		return;
	}

	switch (dp_dvr_state) {
	case DP_DVR_ARB: 	/* Next comes the command phase! */
		if (new_phase != DP_PHASE_CMD) {
#if DP_DEBUG
		    printf ("Phase mismatch cmd!\n");
#endif
		    goto phase_mismatch;
		}
		dp_dvr_state = DP_DVR_CMD;
		ret = dp_pdma_out(cur_xs->cmd, cur_xs->cmdlen, DP_PHASE_CMD);
		dp_clear_isr();
		break;

	case DP_DVR_CMD:	/* Next comes the data i/o phase if needed. */
		/*
		 * This state can potentially accept data in, data out,
		 * or status for new_phase. data in or data out could
		 * be skipped (new_phase is status) if an error was detected
		 * in the command.
		 */
		if (cur_xs->flags & SCSI_DATA_UIO) {
		   /* UIO work. */
		   panic ("scsi uio");
		}
		if (new_phase == DP_PHASE_DATAI) {
#if 1
		    /* just a quick hack until we
		       can trust flags to be correct
		     */
		    if (!cur_xs->data) {
#else
		    if (!(cur_xs->flags & SCSI_DATA_IN)) {
#endif
#if DP_DEBUG
			printf("Phase mismatch in.\n");
#endif
			goto phase_mismatch;
		    }
		    /* expect STAT phase next */
		    dp_dvr_state = DP_DVR_DATA;
		    ret = dp_pdma_in(cur_xs->data, cur_xs->datalen,
		    			 DP_PHASE_DATAI);
		    dp_clear_isr();
		    break;
		}
		else if (new_phase == DP_PHASE_DATAO) {
#if 1
		    /* just a quick hack until we
		       can trust flags to be correct
		     */
		    if (!cur_xs->data) {
#else
		    if (!(cur_xs->flags & SCSI_DATA_OUT)) {
#endif
#if DP_DEBUG
			printf("Phase mismatch out.\n");
#endif
			goto phase_mismatch;
		    }
		    /* expect STAT phase next */
		    dp_dvr_state = DP_DVR_DATA;
		    ret = dp_pdma_out(cur_xs->data, cur_xs->datalen,
		    			 DP_PHASE_DATAO);
		    dp_clear_isr();
		    break;
		}
		/* Fall through to next phase. */
	case DP_DVR_DATA:	/* Next comes the stat phase */
		if (new_phase != DP_PHASE_STATUS) {
#if DP_DEBUG
		    printf("Phase mismatch stat.\n");
#endif
		    goto phase_mismatch;
		}
		dp_dvr_state = DP_DVR_STAT;
		dp_pdma_in(&status, 1, DP_PHASE_STATUS);
		dp_clear_isr();
#if DP_DEBUG > 1
		printf("status = 0x%x\n", status);
#endif
		break;

	case DP_DVR_STAT:
		if (new_phase != DP_PHASE_MSGI) {
#if DP_DEBUG
			printf ("msgi phase mismatch\n");
#endif
			goto phase_mismatch;
		}
		dp_dvr_state = DP_DVR_MSGI;
		dp_pdma_in(&message, 1, DP_PHASE_MSGI);
		dp_clear_isr();
#if DP_DEBUG > 1
		printf("message = 0x%x\n", message);
#endif
#if 0
		if (status != SCSI_OK && dp_try_count < cur_xs->retries) {
		    printf("dp_intr: retry: dp_try_count = %d\n",
			dp_try_count);
		    dp_restart_cmd();
		}
#endif
		break;

	default:
phase_mismatch:
		/* TEMP error generation!!! */
		dp_reset();
		dp_dvr_state = DP_DVR_READY;
		cur_xs->error = XS_DRIVER_STUFFUP;
		dp_intr_retval = HAD_ERROR;
		/* If this true interrupt code, call the done routine. */
		if (cur_xs->when_done) {
		  (*(cur_xs->when_done))(cur_xs->done_arg, cur_xs->done_arg2);
		}
	}

    if (dp_dvr_state == DP_DVR_MSGI) {
#if DP_DEBUG > 1
	printf ("dvr_stat: dp_try_count = %d\n", dp_try_count);
#endif
	WR_ADR (u_char, DP_MODE, 0);	/* Turn off monbsy, dma, ... */
	if (status == SCSI_OK) {
	  cur_xs->error = XS_NOERROR;
	  dp_intr_retval = COMPLETE;
	} else if (status & SCSI_BUSY) {
	  cur_xs->error = XS_BUSY;
	  dp_intr_retval = HAD_ERROR;
	} else if (status & SCSI_CHECK) {
	  /* Do a sense command. */
	  cur_xs->error = XS_SENSE;
	  dp_intr_retval = HAD_ERROR;
	  dp_get_sense (cur_xs);
	}
	cur_xs->flags |= ITSDONE;
	dp_dvr_state = DP_DVR_READY;
#if DP_DEBUG > 1
	printf("calling wakeup on 0x%x\n", cur_xs);
#endif
	wakeup((caddr_t) cur_xs);
	/* If this true interrupt code, call the done routine. */
	if (cur_xs->when_done) {
#if DP_DEBUG > 1
	  printf("dp_intr: calling when_done 0x%x\n", cur_xs->when_done);
#endif
	  (*(cur_xs->when_done))(cur_xs->done_arg, cur_xs->done_arg2);
	}
    }
#if DP_DEBUG > 1
  printf ("exit dp_intr.\n");
#endif
}


/*===========================================================================*
 *				dp_initialize				     *
 *===========================================================================*/

dp_initialize()
{
#if DP_DEBUG
	printf("dp_initialize()\n");
#endif
	scsi_select_ctlr (DP8490);
	WR_ADR (u_char, DP_ICMD, DP_EMODE);	/* Set Enhanced mode */
	WR_ADR (u_char, DP_MODE, 0);		/* Disable everything. */
	WR_ADR (u_char, DP_EMR_ISR, DP_EF_RESETIP);
	WR_ADR (u_char, DP_EMR_ISR, DP_EF_NOP);
	WR_ADR (u_char, DP_SER, 0x80); 	/* scsi adr 7. */
	dp_scsi_phase = DP_PHASE_NONE;
	dp_needs_init = 0;
}

/*===========================================================================*
 *				dp_reset				     *
 *===========================================================================*/

/*
 * Reset dp SCSI bus.
 */

dp_reset()
{
  volatile int i;
  int x = splbio();

  scsi_select_ctlr (DP8490);
  WR_ADR (u_char, DP_MODE, 0);			/* get into harmless state */
  WR_ADR (u_char, DP_OUTDATA, 0);
  WR_ADR (u_char, DP_ICMD, DP_A_RST|DP_EMODE);	/* assert RST on SCSI bus */
  for (i = 55; i; --i);				/* wait 25 usec */
  WR_ADR (u_char, DP_ICMD, DP_EMODE);		/* deassert RST, get off bus */
  WR_ADR (u_char, DP_EMR_ISR, DP_EF_ISR_NEXT | DP_EMR_APHS);
  WR_ADR (u_char, DP_EMR_ISR, DP_INT_MASK);	/* set interrupt mask */
  splx(x);
  for (i = 800000; i; --i);			/* wait 360 msec */
  dp_needs_reset = 0;
}

/*===========================================================================*
 *				dp_wait_bus_free			     *
 *===========================================================================*/
/* Wait for the SCSI bus to become free.  Currently polled because I am
 * assuming a single initiator configuration -- so this code would not be
 * running if the bus were busy.
 */
int
dp_wait_bus_free()
{
  int i;
  u_char stat1;
  volatile int j;

  /* get into a harmless state */
  WR_ADR (u_char, DP_TCMD, 0);
  WR_ADR (u_char, DP_MODE, 0);		/* return to initiator mode */
  WR_ADR (u_char, DP_ICMD, DP_EMODE);	/* clear SEL, disable data out */
  i = WAIT_MUL * 2000;
  while (i--) {
    /* Must be clear for 2 usec, so read twice */
    stat1 = RD_ADR (u_char, DP_STAT1);
    if (stat1 & (DP_S_BSY | DP_S_SEL)) continue;
    for (j = 5; j; j--);
    stat1 = RD_ADR (u_char, DP_STAT1);
    if (stat1 & (DP_S_BSY | DP_S_SEL)) continue;
    return OK;
  }
#if DP_DEBUG
  printf("wait bus free failed\n");
  dp_print_stat1(stat1);
#endif
  dp_needs_reset = 1;
  return NOT_OK;
}

/*===========================================================================*
 *				dp_select				     *
 *===========================================================================*/
/* Select SCSI device, set up for command transfer.
 */
int
dp_select(adr)
long adr;
{
  int i, stat1;

#if DP_DEBUG > 1
  printf("dp_select(0x%x)\n", adr);
#endif
  WR_ADR (u_char, DP_TCMD, 0);		/* get to harmless state */
  WR_ADR (u_char, DP_MODE, 0);		/* get to harmless state */
  WR_ADR (u_char, DP_OUTDATA, adr);	/* SCSI bus address */
  WR_ADR (u_char, DP_ICMD, DP_A_SEL | DP_ENABLE_DB | DP_EMODE);
  for (i = 0;; ++i) {			/* wait for target to assert SEL */
    stat1 = RD_ADR (u_char, DP_STAT1);
    if (stat1 & DP_S_BSY) break;	/* select successful */
    if (i > WAIT_MUL * 2000) {		/* timeout */
      u_char isr;
      WR_ADR(u_char, DP_EMR_ISR, DP_EF_ISR_NEXT);
      isr = RD_ADR (u_char, DP_EMR_ISR);
#if DP_DEBUG
      printf ("SCSI: SELECT timeout adr %d\n", adr);
      dp_print_regs();
      printf("ICMD = 0x%x isr = 0x%x\n", RD_ADR(u_char, DP_ICMD), isr);
#endif
      dp_reset();
      return NOT_OK;
    }
  }
  WR_ADR (u_char, DP_ICMD, DP_EMODE);	/* clear SEL, disable data out */
  WR_ADR (u_char, DP_OUTDATA, 0);
  dp_clear_isr();
  WR_ADR (u_char, DP_TCMD, 4);		/* bogus phase, guarantee mismatch */
  WR_ADR (u_char, DP_MODE, DP_M_BSY | DP_M_DMA);
  return OK;
}

/*===========================================================================*
 *				scsi_select_ctlr
 *===========================================================================*/
/* Select a SCSI device.
 */
scsi_select_ctlr (ctlr)
int ctlr;
{
  /* May need other stuff here to syncronize between dp & aic. */

  RD_ADR (u_char, ICU_IO) &= ~ICU_SCSI_BIT;	/* i/o, not port */
  RD_ADR (u_char, ICU_DIR) &= ~ICU_SCSI_BIT;	/* output */
  if (ctlr == DP8490)
    RD_ADR (u_char, ICU_DATA) &= ~ICU_SCSI_BIT;	/* select = 0 for 8490 */
  else
    RD_ADR (u_char, ICU_DATA) |= ICU_SCSI_BIT;	/* select = 1 for AIC6250 */
}

/*===========================================================================*
 *				dp_start_cmd				     *
 *===========================================================================*/

int dp_start_cmd(struct scsi_xfer *xs)
{
#if 0
  WR_ADR (u_char, DP_OUTDATA, 1 << xs->targ);	/* SCSI bus address */
  WR_ADR (u_char, DP_EMR_ISR, DP_EF_ARB);
  dp_dvr_state = DP_DVR_ARB;
#else

  /* This is not the "right" way to start it.  We should just have the
	chip do the select for us and interrupt at the end. */

  if (!dp_wait_bus_free()) {
#if DP_DEBUG
	printf("dp_start_cmd: DP DRIVER BUSY\n");
#endif
	xs->error = XS_BUSY;
	return TRY_AGAIN_LATER;
  }

  if (!dp_select(1 << xs->targ)) {
#if DP_DEBUG
	printf("dp_start_cmd: DP DRIVER STUFFUP\n");
#endif
	xs->error = XS_DRIVER_STUFFUP;
	return HAD_ERROR;
  }
#endif

  /* After selection, we now wait for the APHS interrupt! */
  dp_dvr_state = DP_DVR_ARB;	/* Just finished the select/arbitration */
  dp_try_count = 1;

  if (!(xs->flags & SCSI_NOMASK)) {
    /* Set up the timeout! */
#if DP_DEBUG > 1
    printf ("dp_start_cmd: dp timeouts not done\n");
#endif
  }
  return SUCCESSFULLY_QUEUED;
}

/*===========================================================================*
 *				dp_restart_cmd				     *
 *===========================================================================*/

int dp_restart_cmd()
{
#if 0
  WR_ADR (u_char, DP_OUTDATA, xs->targ);	/* SCSI bus address */
  WR_ADR (u_char, DP_EMR_ISR, DP_EF_ARB);
  dp_dvr_state = DP_DVR_ARB;
#endif

  /* This is not the "right" way to start it.  We should just have the
	chip do the select for us and interrupt at the end. */

  DELAY(50);
#if DP_DEBUG
  printf ("restart .. stat1=0x%x stat2=0x%x\n", RD_ADR(u_char, DP_STAT1),
    RD_ADR(u_char, DP_STAT2));
#endif
  if (!dp_wait_bus_free()) {
	cur_xs->error = XS_BUSY;
	return;
  }

#if DP_DEBUG
  printf ("restart .1 stat1=0x%x stat2=0x%x\n", RD_ADR(u_char, DP_STAT1),
    RD_ADR(u_char, DP_STAT2));
  printf ("cur_xs->targ=%d\n",cur_xs->targ);
#endif
  if (!dp_select (1 << cur_xs->targ)) {
	cur_xs->error = XS_DRIVER_STUFFUP;
	return;
  }

#if DP_DEBUG
  printf ("restart .2 stat1=0x%x stat2=0x%x\n", RD_ADR(u_char, DP_STAT1),
    RD_ADR(u_char, DP_STAT2));
#endif
  /* After selection, we now wait for the APHS interrupt! */
  dp_dvr_state = DP_DVR_ARB;	/* Just finished the select/arbitration */
  dp_try_count++;

  if (!(cur_xs->flags & SCSI_NOMASK)) {
    /* Set up the timeout! */
#if DP_DEBUG
    printf ("dp_restart_cmd: dp timeouts not done\n");
#endif
  }
}

/*===========================================================================*
 *				dp_pdma_out				     *
 *===========================================================================*/

/* Note:  in NetBSD, the scsi dma addresses are set by the mapping hardware
   to inhibit cache.  There is therefore, no need to worry about cache hits
   during access to dma addresses. */

int dp_pdma_out(char *buf, int count, int phase)
{
  int cnt;
  int ret = OK;
  u_int stat2;

#if DP_DEBUG
  printf("dp_pdma_out: write %d bytes\n", count);
#endif
#if DP_DEBUG > 1
  if (RD_ADR(u_char, DP_STAT2) & DP_S_IRQ)
    printf("WARNING: stat2:IRQ set on call to dp_pdma_out\n");
#endif

  /* Set it up. */
  WR_ADR(u_char, DP_TCMD, phase);
  RD_ADR(u_char, DP_MODE) |= DP_M_DMA;
  WR_ADR(u_char, DP_ICMD, DP_ENABLE_DB | DP_EMODE);
  WR_ADR(u_char, DP_START_SEND, 0);

  /* Do the pdma: first longs, then bytes. */
  while (count > sizeof(long)) {
    WR_ADR(long, DP_DMA, *(((long *)buf)++));
    count -= sizeof(long);
  }
  while (count-- > 1) {
    WR_ADR(u_char, DP_DMA, *(buf++));
  }

  /* wait for DRQ to be asserted for the last byte, or an
   * interrupt request to be signaled
   */
  while (1) {
    stat2 = RD_ADR(u_char, DP_STAT2);
    if (stat2 & (DP_S_IRQ | DP_S_DRQ)) break;
  }

  if (stat2 & DP_S_DRQ) {
    WR_ADR(u_char, DP_DMA_EOP, *buf);
  }
  else {
    /* dma error! */
#if DP_DEBUG
    printf ("dma write error!\n");
    dp_print_stat1(RD_ADR(u_char, DP_STAT1));
    dp_print_stat2(stat2);
#endif
    cur_xs->error = XS_DRIVER_STUFFUP;
    /* Clear dma mode, just in case, and disable the bus. */
    RD_ADR (u_char, DP_MODE) &= ~DP_M_DMA;
    WR_ADR (u_char, DP_ICMD, DP_EMODE);
    ret = NOT_OK;
  }
#if 0
  /* Clear dma mode, just in case, and disable the bus. */
  RD_ADR (u_char, DP_MODE) &= ~DP_M_DMA;
  WR_ADR (u_char, DP_ICMD, DP_EMODE);
#endif

  return ret;
}

/*===========================================================================*
 *				dp_pdma_in				     *
 *===========================================================================*/

/* Note:  in NetBSD, the scsi dma addresses are set by the mapping hardware
   to inhibit cache.  There is therefore, no need to worry about cache hits
   during access to dma addresses. */

int dp_pdma_in(char *buf, int count, int phase)
{
  int ret = OK;
  int i_count = count;
  u_int stat2;
  u_char *dma_adr = (u_char *) DP_DMA;	/* Address for last few bytes. */

#if DP_DEBUG > 1
  printf("dp_pdma_in: read %d bytes\n", count);
#endif
  /* Set it up. */
  WR_ADR(u_char, DP_TCMD, phase);
  RD_ADR(u_char, DP_MODE) |= DP_M_DMA;
  WR_ADR(u_char, DP_EMR_ISR, DP_EF_START_RCV | DP_EMR_APHS);

  /* Do the pdma */
  while (count >= sizeof(long)) {
    *(((long *)buf)++) = RD_ADR(long, DP_DMA);
    count -= sizeof(long);
  }

  while (count-- > 0) {
    *(buf++) = RD_ADR(u_char, (dma_adr++));
  }

  /* Clear dma mode, just in case, and disable the bus. */
  RD_ADR(u_char, DP_MODE) &= ~DP_M_DMA;
  WR_ADR(u_char, DP_ICMD, DP_EMODE);
  return ret;
}

dp_wait_for_edma()
{
	int i;

	for (i = 0; i < 1000000; ++i) {
		u_char tcmd = RD_ADR(u_char, DP_TCMD);
		if (tcmd & DP_TCMD_EDMA) {
#if DP_DEBUG > 1
			printf("dp_wait_for_phase: EDMA detected\n");
#endif
			RD_ADR(u_char, DP_MODE) &= ~DP_M_DMA;
			WR_ADR(u_char, DP_ICMD, DP_EMODE);
			return;
		}
	}
	printf("wait for edma timeout\n");
#if DP_DEBUG
	dp_print_regs();
#endif
	panic("dp: wait for edma");
}

dp_wait_for_phase(u_char phase)
{
    int i;
    u_char isr;

#if DP_DEBUG > 1
    printf("wait for phase %d...", phase);
#endif
    /* set the TCR register */
    WR_ADR(u_char, DP_TCMD, phase);
    /* wait for phase match */
    for (i = 0; i < 1000000; ++i) {
	u_char stat2 = RD_ADR(u_char, DP_STAT2);
	if (stat2 & DP_S_PHASE) {
#if DP_DEBUG > 1
	    printf("done\n");
#endif
	    /* completely clear the isr */
	    WR_ADR(u_char, DP_EMR_ISR, DP_EF_ISR_NEXT);
	    isr = RD_ADR (u_char, DP_EMR_ISR);
	    dp_clear_isr();
	    return;
	}
    }
    printf("wait for phase %d timeout\n", phase);
#if DP_DEBUG
    dp_print_regs();
#endif
    panic("dp: wait for phase");
}

/*===========================================================================*
 *				dp_get_sense				     *
 *===========================================================================*/

dp_get_sense (struct scsi_xfer *xs)
{
    u_char status;
    u_char message;
    u_char isr;
    int ret;

    bzero((u_char *) &xs->sense, sizeof(xs->sense));

    /* completely clear the isr on entry */
    WR_ADR(u_char, DP_EMR_ISR, DP_EF_ISR_NEXT);
    isr = RD_ADR (u_char, DP_EMR_ISR);
    dp_clear_isr();

    RD_ADR(u_char, DP_MODE) &= ~DP_M_DMA;
    WR_ADR(u_char, DP_ICMD, DP_EMODE);

#if DP_DEBUG > 2
    printf ("sense 1: wait bus free\n");
#endif
    if (!dp_wait_bus_free()) {
#if DP_DEBUG > 2
	printf("sense 1: wait-bus-free failed\n");
#endif
	xs->error = XS_BUSY;
	return;
    }

#if DP_DEBUG > 2
    printf ("sense 2: select device\n");
#endif
    if (!dp_select (1 << xs->targ)) {
#if DP_DEBUG
	printf("sense 2: select failed\n");
#endif
	xs->error = XS_DRIVER_STUFFUP;
	return;
    }
    /* completely clear the isr */
    WR_ADR(u_char, DP_EMR_ISR, DP_EF_ISR_NEXT);
    isr = RD_ADR (u_char, DP_EMR_ISR);
    dp_clear_isr();

    /* send the command */
    sense_cmd[1] = xs->lu << 5;
#if 0
    sense_cmd[4] = sizeof(struct scsi_sense_data);
#else
    sense_cmd[4] = 0x04;
#endif
    dp_wait_for_phase(DP_PHASE_CMD);
    ret = dp_pdma_out(sense_cmd, sizeof(sense_cmd), DP_PHASE_CMD);
    if (ret != OK) {
#if DP_DEBUG
	printf("dp_pdma_out: ret=%d\n", ret);
#endif
	return;
    }

    /* read sense data */
    dp_wait_for_edma();
    dp_wait_for_phase(DP_PHASE_DATAI);
    ret = dp_pdma_in((u_char *) &xs->sense, sense_cmd[4], DP_PHASE_DATAI);
    if (ret != OK) {
#if DP_DEBUG
      printf ("dp_pdma_in: ret=%d\n", ret);
#endif
    }

    /* read status */
    dp_wait_for_phase(DP_PHASE_STATUS);
    ret = dp_pdma_in(&status, 1, DP_PHASE_STATUS);
    if (ret != OK) {
#if DP_DEBUG
      printf ("dp_pdma_in: ret=%d\n", ret);
#endif
    }

    /* read message */
    dp_wait_for_phase(DP_PHASE_MSGI);
    ret = dp_pdma_in(&message, 1, DP_PHASE_MSGI);
    if (ret != OK) {
#if DP_DEBUG
      printf ("dp_pdma_in: ret=%d\n", ret);
#endif
    }

#if DP_DEBUG
    printf("sense status = 0x%x\n", status);
    printf("  sense (0x%x) valid = %d code = 0x%x class = 0x%x\n",
	*(u_char *) &xs->sense,
	xs->sense.valid, xs->sense.error_code, xs->sense.error_class);
#endif

    if (status & SCSI_BUSY) {
	xs->error = XS_BUSY;
    }
    WR_ADR (u_char, DP_MODE, 0);	/* Turn off monbsy, dma, ... */
}
