/*	$NetBSD: scsi.c,v 1.19 1995/09/03 03:39:38 briggs Exp $	*/

/*
 * This driver is obsolete as of 2 September 1995.  mac68k5380.c should
 * be used instead if at all possible.  [briggs 2Sept95]
 */

/*
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

#define PSEUDO_DMA 1

static int pdebug = 0;

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
#include <scsi/scsiconf.h>

#include "scsi_defs.h"
#define PAD(n)	u_char	n[15]
#include <machine/scsi_5380.h>
#undef PAD
#define SCI_PHASE_DISC		0	/* sort of ... */
#define SCI_CLR_INTR(regs)	{register int temp = regs->sci_iack;}
#define SCI_ACK(ptr,phase)	(ptr)->sci_tcmd = (phase)
#define SCSI_TIMEOUT_VAL	10000000
#define WAIT_FOR_NOT_REQ(ptr) {					\
	int scsi_timeout = SCSI_TIMEOUT_VAL;			\
	while ( ((ptr)->sci_bus_csr & SCI_BUS_REQ) &&		\
		 ((ptr)->sci_bus_csr & SCI_BUS_REQ) &&		\
		 ((ptr)->sci_bus_csr & SCI_BUS_REQ) &&		\
		 (--scsi_timeout) );				\
	if (!scsi_timeout) {					\
		printf("scsi timeout--WAIT_FOR_NOT_REQ---scsi.c, line %d.\n", \
			__LINE__);				\
		goto scsi_timeout_error;			\
	}							\
}
#define WAIT_FOR_REQ(ptr) {					\
	int scsi_timeout = SCSI_TIMEOUT_VAL;			\
	while ( (((ptr)->sci_bus_csr & SCI_BUS_REQ) == 0) &&	\
		(((ptr)->sci_bus_csr & SCI_BUS_REQ) == 0) &&	\
		(((ptr)->sci_bus_csr & SCI_BUS_REQ) == 0) &&	\
		 (--scsi_timeout) );				\
	if (!scsi_timeout) {					\
		printf("scsi timeout--WAIT_FOR_REQ---scsi.c, line %d.\n", \
			__LINE__);				\
		goto scsi_timeout_error;			\
	}							\
}
#define WAIT_FOR_BSY(ptr) {					\
	int scsi_timeout = SCSI_TIMEOUT_VAL;			\
	while ( (((ptr)->sci_bus_csr & SCI_BUS_BSY) == 0) &&	\
		(((ptr)->sci_bus_csr & SCI_BUS_BSY) == 0) &&	\
		(((ptr)->sci_bus_csr & SCI_BUS_BSY) == 0) &&	\
		 (--scsi_timeout) );				\
	if (!scsi_timeout) {					\
		printf("scsi timeout--WAIT_FOR_BSY---scsi.c, line %d.\n", \
			__LINE__);				\
		goto scsi_timeout_error;			\
	}							\
}

#ifdef DDB
int     Debugger();
#else
#define Debugger() panic("Should call Debugger here (mac/dev/scsi.c).")
#endif

typedef unsigned long int physaddr;
typedef sci_regmap_t sci_padded_regmap_t;

#define NNCR5380	1

struct ncr5380_softc {
	struct device sc_dev;

	void   *reg_base;
	int     adapter_target;
	struct scsi_link sc_link;
};
/* From Guide to Mac II family hardware, p. 137 */
/* These are "adjusted" in the init routine. */
extern vm_offset_t SCSIBase;
static volatile sci_padded_regmap_t *ncr = (sci_regmap_t *) 0x10000;
static volatile long *sci_4byte_addr = (long *) 0x6000;
static volatile u_char *sci_1byte_addr = (u_char *) 0x12000;

static unsigned int ncr5380_adapter_info(struct ncr5380_softc * ncr5380);
static void ncr5380_minphys(struct buf * bp);
static int ncr5380_scsi_cmd(struct scsi_xfer * xs);

static int ncr5380_show_scsi_cmd(struct scsi_xfer * xs);
static int ncr5380_reset_target(int adapter, int target);
static int ncr5380_poll(int adapter, int timeout);
static int ncr5380_send_cmd(struct scsi_xfer * xs);

extern void ncr5380_intr(int adapter);
extern void spinwait(int);

static int
scsi_gen(int adapter, int id, int lun,
    struct scsi_generic * cmd, int cmdlen,
    void *databuf, int datalen);
static int
scsi_group0(int adapter, int id, int lun,
    int opcode, int addr, int len,
    int flags, caddr_t databuf, int datalen);

struct scsi_adapter ncr5380_switch = {
	ncr5380_scsi_cmd,	/* scsi_cmd()		 */
	ncr5380_minphys,	/* scsi_minphys()	 */
	0,			/* open_target_lu()	 */
	0,			/* close_target_lu()	 */
};
/* This is copied from julian's bt driver */
/* "so we have a default dev struct for our link struct." */
struct scsi_device ncr5380_dev = {
	NULL,			/* Use default error handler.	    */
	NULL,			/* have a queue, served by this (?) */
	NULL,			/* have no async handler.	    */
	NULL,			/* Use default "done" routine.	    */
};

extern int matchbyname();
static int ncrprobe();
static void ncrattach();

struct cfdriver ncrscsicd =
{NULL, "ncrscsi", ncrprobe, ncrattach,
DV_DULL, sizeof(struct ncr5380_softc), NULL, 0};

static int
ncr_print(aux, name)
	void   *aux;
	char   *name;
{
	/* printf("%s: (sc_link = 0x%x)", name, (int) aux); return UNCONF; */
}

static int
ncrprobe(parent, match, aux)
	struct device *parent;
	void   *match;
	void   *aux;
{
	static int probed = 0;
	struct ncr5380_softc *ncr5380;

	if (!mac68k_machine.scsi80) {
		return 0;
	}
	ncr5380 = (struct ncr5380_softc *) match;

	if (strcmp(*((char **) aux), ncr5380->sc_dev.dv_xname)) {
		return 0;
	}
	if (!probed) {
		/*
		 * Adjust values based on SCSIBase.
		 */
		ncr = (volatile sci_regmap_t *) (SCSIBase + (u_int) ncr);
		sci_4byte_addr = (volatile long *)
		    (SCSIBase + (u_int) sci_4byte_addr);
		sci_1byte_addr = (volatile u_char *)
		    (SCSIBase + (u_int) sci_1byte_addr);
		probed = 1;
	}
	return 1;
}

static void
ncrattach(parent, dev, aux)
	struct device *parent, *dev;
	void   *aux;
{
	int     unit = dev->dv_unit;
	struct ncr5380_softc *ncr5380;
	int     r;

	ncr5380 = (struct ncr5380_softc *) dev;

	ncr5380->sc_link.scsibus = unit;
	ncr5380->sc_link.adapter_target = 7;
	ncr5380->sc_link.adapter = &ncr5380_switch;
	ncr5380->sc_link.device = &ncr5380_dev;
	ncr5380->sc_link.openings = 1;
#ifdef SCSIDEBUG
	ncr5380->sc_link.flags = SDEV_DB1 | SDEV_DB2 /* | SDEV_DB3 | SDEV_DB4 */ ;
#endif

	printf("\n");

	config_found(dev, &(ncr5380->sc_link), ncr_print);
}

#define MIN_PHYS	65536	/* BARF!!!! */
static void
ncr5380_minphys(struct buf * bp)
{
	if (bp->b_bcount > MIN_PHYS) {
		printf("Uh-oh...  ncr5380_minphys setting bp->b_bcount = %x.\n", MIN_PHYS);
		bp->b_bcount = MIN_PHYS;
	}
	minphys(bp);
}
#undef MIN_PHYS

static int
ncr5380_scsi_cmd(struct scsi_xfer * xs)
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
		s = splbio();
		ncr5380_reset_target(xs->sc_link->scsibus,
		    xs->sc_link->target);
		splx(s);
		return (COMPLETE);
	}
	xs->resid = 0;
	r = ncr5380_send_cmd(xs);
	xs->flags |= ITSDONE;
	scsi_done(xs);
#ifdef SCSIDEBUG
	printf("SCSI transfer done.\n");
#endif				/* SCSIDEBUG */
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
}

static int
ncr5380_show_scsi_cmd(struct scsi_xfer * xs)
{
	u_char *b = (u_char *) xs->cmd;
	int     i = 0;

	if (!(xs->flags & SCSI_RESET)) {
		printf("ncr5380(%d:%d:%d)-",
		    xs->sc_link->scsibus, xs->sc_link->target, xs->sc_link->lun);
		while (i < xs->cmdlen) {
			if (i)
				printf(",");
			printf("%x", b[i++]);
		}
		printf("-\n");
	} else {
		printf("ncr5380(%d:%d:%d)-RESET-\n",
		    xs->sc_link->scsibus, xs->sc_link->target, xs->sc_link->lun);
	}
}
/*
 * Actual chip control.
 */

extern void
spinwait(int ms)
{
	while (ms--)
		delay(1000);
}

extern void
ncr5380_intr(int adapter)
{
	register volatile sci_padded_regmap_t *regs = ncr;

	SCI_CLR_INTR(regs);
	regs->sci_mode = 0x00;
}

extern int
ncr5380_irq_intr(void)
{
	register volatile sci_padded_regmap_t *regs = ncr;

/*	if (regs->sci_csr != SCI_CSR_PHASE_MATCH)
		printf("scsi_irq_intr called (not just phase match -- "
			"csr = 0x%x, bus_csr = 0x%x).\n",
			regs->sci_csr, regs->sci_bus_csr);
	ncr5380_intr(0); */
	return 1;
}

extern int
ncr5380_drq_intr(void)
{
/*	printf("scsi_drq_intr called.\n"); */
/*	ncr5380_intr(0); */
	return 1;
}

static int
ncr5380_reset_target(int adapter, int target)
{
	register volatile sci_padded_regmap_t *regs = ncr;
	int     dummy;

	regs->sci_icmd = SCI_ICMD_TEST;
	regs->sci_icmd = SCI_ICMD_TEST | SCI_ICMD_RST;
	spinwait(250);		/* 250 ms */
	regs->sci_icmd = 0;

	regs->sci_mode = 0;
	regs->sci_tcmd = SCI_PHASE_DISC;
	regs->sci_sel_enb = 0;

	SCI_CLR_INTR(regs);
	SCI_CLR_INTR(regs);
}

static int
ncr5380_poll(int adapter, int timeout)
{
}

static int
ncr5380_send_cmd(struct scsi_xfer * xs)
{
	int     s;
	int     sense;

#ifdef SCSIDEBUG
	ncr5380_show_scsi_cmd(xs);
#endif
	s = splbio();
	sense = scsi_gen(xs->sc_link->scsibus, xs->sc_link->target,
	    xs->sc_link->lun, xs->cmd, xs->cmdlen,
	    xs->data, xs->datalen);
	splx(s);
	if (sense) {
		switch (sense) {
		case 0x02:	/* Check condition */
/*				printf("check cond. target %d.\n", xs->targ);*/
			s = splbio();
			scsi_group0(xs->sc_link->scsibus,
			    xs->sc_link->target,
			    xs->sc_link->lun,
			    0x3, 0x0,
			    sizeof(struct scsi_sense_data),
			    0, (caddr_t) & (xs->sense),
			    sizeof(struct scsi_sense_data));
			splx(s);
			xs->error = XS_SENSE;
			return COMPLETE;
		case 0x08:	/* Busy */
			xs->error = XS_BUSY;
			return COMPLETE;
		default:
			xs->error = XS_DRIVER_STUFFUP;
			return COMPLETE;
		}
	}
	xs->error = XS_NOERROR;
	return (COMPLETE);
}

static int
select_target(register volatile sci_padded_regmap_t * regs,
    u_char myid, u_char tid, int with_atn)
{
	register u_char bid, icmd;
	int     ret = SCSI_RET_RETRY;

	if ((regs->sci_bus_csr & (SCI_BUS_BSY | SCI_BUS_SEL)) &&
	    (regs->sci_bus_csr & (SCI_BUS_BSY | SCI_BUS_SEL)) &&
	    (regs->sci_bus_csr & (SCI_BUS_BSY | SCI_BUS_SEL)))
		return ret;

	/* for our purposes.. */
	myid = 1 << myid;
	tid = 1 << tid;

	regs->sci_sel_enb = 0;	/* myid;	we don't want any interrupts. */
	regs->sci_tcmd = 0;	/* Get into a harmless state. */
	regs->sci_mode = 0;	/* Get into a harmless state. */

	regs->sci_odata = myid;
	regs->sci_mode = SCI_MODE_ARB;
/*	regs->sci_mode |= SCI_MODE_ARB;		*/
	/* AIP might not set if BSY went true after we checked */
	for (bid = 0; bid < 20; bid++)	/* 20usec circa */
		if (regs->sci_icmd & SCI_ICMD_AIP)
			break;
	if ((regs->sci_icmd & SCI_ICMD_AIP) == 0) {
		goto lost;
	}
	delay(3);		/* 2.4us arb delay */

	if (regs->sci_icmd & SCI_ICMD_LST) {
		goto lost;
	}
	regs->sci_mode &= ~SCI_MODE_PAR_CHK;
	bid = regs->sci_data;

	if ((bid & ~myid) > myid) {
		goto lost;
	}
	if (regs->sci_icmd & SCI_ICMD_LST) {
		goto lost;
	}
	/* Won arbitration, enter selection phase now */
	icmd = regs->sci_icmd & ~(SCI_ICMD_DIFF | SCI_ICMD_TEST);
	icmd |= (with_atn ? (SCI_ICMD_SEL | SCI_ICMD_ATN) : SCI_ICMD_SEL);
	icmd |= SCI_ICMD_BSY;
	regs->sci_icmd = icmd;

	if (regs->sci_icmd & SCI_ICMD_LST) {
		goto nosel;
	}
	/* XXX a target that violates specs might still drive the bus XXX */
	/* XXX should put our id out, and after the delay check nothi XXX */
	/* XXX ng else is out there.				      XXX */

	delay(1);

	regs->sci_tcmd = 0;
	regs->sci_odata = myid | tid;
	regs->sci_sel_enb = 0;

/*	regs->sci_mode &= ~SCI_MODE_ARB;	 2 deskew delays, too */
	regs->sci_mode = 0;	/* 2 deskew delays, too */

	icmd |= SCI_ICMD_DATA;
	icmd &= ~(SCI_ICMD_BSY);

	regs->sci_icmd = icmd;

	/* bus settle delay, 400ns */
	delay(1);		/* 1us > 400ns ;-) */

/*	regs->sci_mode |= SCI_MODE_PAR_CHK; */

	{
		register int timeo = 2500;	/* 250 msecs in 100 usecs
						 * chunks */
		while ((regs->sci_bus_csr & SCI_BUS_BSY) == 0) {
			if (--timeo > 0) {
				delay(100);
			} else {
				goto nodev;
			}
		}
	}

	icmd &= ~(SCI_ICMD_DATA | SCI_ICMD_SEL);
	regs->sci_icmd = icmd;
	/*	regs->sci_sel_enb = myid;*//* looks like we should NOT have it */
	return SCSI_RET_SUCCESS;
nodev:
	ret = SCSI_RET_DEVICE_DOWN;
	regs->sci_sel_enb = myid;
nosel:
	icmd &= ~(SCI_ICMD_DATA | SCI_ICMD_SEL | SCI_ICMD_ATN);
	regs->sci_icmd = icmd;
lost:
	regs->sci_mode = 0;

	return ret;
}

static int
sci_data_out(regs, phase, count, data)
	register sci_padded_regmap_t *regs;
	unsigned char *data;
{
	register unsigned char icmd;
	register int cnt = 0;

	/* ..checks.. */

	icmd = regs->sci_icmd & ~(SCI_ICMD_DIFF | SCI_ICMD_TEST);
loop:
	if (SCI_CUR_PHASE(regs->sci_bus_csr) != phase)
		return cnt;

	WAIT_FOR_REQ(regs);
	icmd |= SCI_ICMD_DATA;
	regs->sci_icmd = icmd;
	regs->sci_odata = *data++;
	icmd |= SCI_ICMD_ACK;
	regs->sci_icmd = icmd;

	icmd &= ~(SCI_ICMD_DATA | SCI_ICMD_ACK);
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
	register sci_padded_regmap_t *regs;
	unsigned char *data;
{
	register unsigned char icmd;
	register int cnt = 0;

	/* ..checks.. */

	icmd = regs->sci_icmd & ~(SCI_ICMD_DIFF | SCI_ICMD_TEST);

loop:
	if (SCI_CUR_PHASE(regs->sci_bus_csr) != phase)
		return cnt;

	WAIT_FOR_REQ(regs);
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

static int
command_transfer(register volatile sci_padded_regmap_t * regs,
    int maxlen, u_char * data, u_char * status, u_char * msg)
{
	int     xfer = 0, phase;

/*	printf("command_transfer called for 0x%x.\n", *data); */

	regs->sci_icmd = 0;

	while (1) {

		WAIT_FOR_REQ(regs);

		phase = SCI_CUR_PHASE(regs->sci_bus_csr);

		switch (phase) {
		case SCSI_PHASE_CMD:
			SCI_ACK(regs, SCSI_PHASE_CMD);
			xfer += sci_data_out(regs, SCSI_PHASE_CMD,
			    maxlen, data);
			return xfer;
		case SCSI_PHASE_DATA_IN:
			printf("Data in phase in command_transfer?\n");
			return 0;
		case SCSI_PHASE_DATA_OUT:
			printf("Data out phase in command_transfer?\n");
			return 0;
		case SCSI_PHASE_STATUS:
			SCI_ACK(regs, SCSI_PHASE_STATUS);
			printf("status in command_transfer.\n");
			sci_data_in(regs, SCSI_PHASE_STATUS,
			    1, status);
			break;
		case SCSI_PHASE_MESSAGE_IN:
			SCI_ACK(regs, SCSI_PHASE_MESSAGE_IN);
			printf("msgin in command_transfer.\n");
			sci_data_in(regs, SCSI_PHASE_MESSAGE_IN,
			    1, msg);
			break;
		case SCSI_PHASE_MESSAGE_OUT:
			SCI_ACK(regs, SCSI_PHASE_MESSAGE_OUT);
			sci_data_out(regs, SCSI_PHASE_MESSAGE_OUT,
			    1, msg);
			break;
		default:
			printf("Unexpected phase 0x%x in "
			    "command_transfer().\n", phase);
	scsi_timeout_error:
			return xfer;
			break;
		}
	}
}

static int
data_transfer(register volatile sci_padded_regmap_t * regs,
    int maxlen, u_char * data, u_char * status, u_char * msg)
{
	int     retlen = 0, xfer, phase;

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
			SCI_ACK(regs, SCSI_PHASE_DATA_IN);
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
			SCI_ACK(regs, SCSI_PHASE_DATA_OUT);
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
			SCI_ACK(regs, SCSI_PHASE_STATUS);
			sci_data_in(regs, SCSI_PHASE_STATUS,
			    1, status);
			break;
		case SCSI_PHASE_MESSAGE_IN:
			SCI_ACK(regs, SCSI_PHASE_MESSAGE_IN);
			sci_data_in(regs, SCSI_PHASE_MESSAGE_IN,
			    1, msg);
			if (*msg == 0) {
				return retlen;
			} else {
				printf("message 0x%x in "
				    "data_transfer.\n", *msg);
			}
			break;
		case SCSI_PHASE_MESSAGE_OUT:
			SCI_ACK(regs, SCSI_PHASE_MESSAGE_OUT);
			sci_data_out(regs, SCSI_PHASE_MESSAGE_OUT,
			    1, msg);
			break;
		default:
			printf("Unexpected phase 0x%x in "
			    "data_transfer().\n", phase);
	scsi_timeout_error:
			return retlen;
			break;
		}
	}
}

static int
scsi_request(register volatile sci_padded_regmap_t * regs,
    int target, int lun, u_char * cmd, int cmdlen,
    char *databuf, int datalen, int *sent, int *ret)
{
/* Returns 0 on success, -1 on internal error, or the status byte */
	int     cmd_bytes_sent, r;
	u_char  stat, msg, c;

	*sent = 0;

	if ((r = select_target(regs, 7, target, 1)) != SCSI_RET_SUCCESS) {
		*ret = r;
		SCI_CLR_INTR(regs);
		switch (r) {
		case SCSI_RET_RETRY:
			return 0x08;
		default:
			printf("select_target(target %d, lun %d) failed(%d).\n",
			    target, lun, r);
		case SCSI_RET_DEVICE_DOWN:
			return -1;
		}
	}
	c = 0x80 | lun;

	if ((cmd_bytes_sent = command_transfer(regs, cmdlen,
		    (u_char *) cmd, &stat, &c))
	    != cmdlen) {
		SCI_CLR_INTR(regs);
		*ret = SCSI_RET_COMMAND_FAIL;
		printf("Data underrun sending CCB (%d bytes of %d, sent).\n",
		    cmd_bytes_sent, cmdlen);
		return -1;
	}
	*sent = data_transfer(regs, datalen, (u_char *) databuf,
	    &stat, &msg);

	*ret = 0;

	return stat;
}

static int
scsi_gen(int adapter, int id, int lun, struct scsi_generic * cmd,
    int cmdlen, void *databuf, int datalen)
{
	register volatile sci_padded_regmap_t *regs = ncr;
	int     i, j, sent, ret;

	cmd->bytes[0] = ((u_char) lun << 5);

	i = scsi_request(regs, id, lun, (u_char *) cmd, cmdlen,
	    databuf, datalen, &sent, &ret);

	return i;
}

static int
scsi_group0(int adapter, int id, int lun, int opcode, int addr, int len,
    int flags, caddr_t databuf, int datalen)
{
	register volatile sci_padded_regmap_t *regs = ncr;
	unsigned char cmd[6];
	int     i, j, sent, ret;

	cmd[0] = opcode;	/* Operation code           		 */
	cmd[1] = (lun << 5) | ((addr >> 16) & 0x1F);	/* Lun & MSB of addr	 */
	cmd[2] = (addr >> 8) & 0xFF;	/* addr					 */
	cmd[3] = addr & 0xFF;	/* LSB of addr				 */
	cmd[4] = len;		/* Allocation length			 */
	cmd[5] = flags;		/* Link/Flag				 */

	i = scsi_request(regs, id, lun, cmd, 6, databuf, datalen, &sent, &ret);

	return i;
}
/* pseudo-dma action */

#if PSEUDO_DMA

#define TIMEOUT	1000000
#define READY(poll) \
	i = TIMEOUT; \
	while ((regs->sci_csr & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) \
	       !=(SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) \
		if (   !(regs->sci_csr     & SCI_CSR_PHASE_MATCH) \
		    || !(regs->sci_bus_csr & SCI_BUS_BSY) \
		    || (i-- < 0) ) { \
			printf("scsi.c: timeout counter = %d, len = %d count=%d (count-len %d).\n", \
				i, len,count,count-len); \
			printf("pdebug = %d,  1=out, 2=in",pdebug); \
			/*dump_regs();*/ \
			if (poll && !(regs->sci_csr & SCI_CSR_PHASE_MATCH)) { \
				regs->sci_icmd &= ~SCI_ICMD_DATA; \
				len--; \
			} else { \
				regs->sci_mode &= ~SCI_MODE_DMA; \
			} \
			return count-len; \
		}

#define W1	*byte_data = *data++
#define W4	*long_data = *((long*)data)++

sci_pdma_out(regs, phase, count, data)
	register volatile sci_padded_regmap_t *regs;
	int     phase;
	int     count;
	u_char *data;
{
	register volatile long *long_data = sci_4byte_addr;
	register volatile u_char *byte_data = sci_1byte_addr;
	register int len = count, i;

	pdebug = 1;

	if (count < 128)
		return sci_data_out(regs, phase, count, data);

	WAIT_FOR_BSY(regs);
	regs->sci_mode |= SCI_MODE_DMA;
	regs->sci_icmd |= SCI_ICMD_DATA;
	regs->sci_dma_send = 0;

	while (len >= 64) {
		READY(1);
		W1;
		READY(1);
		W1;
		READY(1);
		W1;
		READY(1);
		W1;
		READY(1);
		W4;
		W4;
		W4;
		W4;
		W4;
		W4;
		W4;
		W4;
		W4;
		W4;
		W4;
		W4;
		W4;
		W4;
		W4;
		len -= 64;
	}
	while (len) {
		READY(1);
		W1;
		len--;
	}
	i = TIMEOUT;
	while (((regs->sci_csr & (SCI_CSR_DREQ | SCI_CSR_PHASE_MATCH))
		== SCI_CSR_PHASE_MATCH) && --i);
	if (!i)
		printf("scsi.c:%d: timeout waiting for SCI_CSR_DREQ.\n", __LINE__);
	*byte_data = 0;
scsi_timeout_error:
	regs->sci_mode &= ~SCI_MODE_DMA;
	return count - len;
}
#undef  W1
#undef  W4

#define R4	*((long *)data)++ = *long_data
#define R1	*data++ = *byte_data

sci_pdma_in(regs, phase, count, data)
	register volatile sci_padded_regmap_t *regs;
	int     phase;
	int     count;
	u_char *data;
{
	register volatile long *long_data = sci_4byte_addr;
	register volatile u_char *byte_data = sci_1byte_addr;
	register int len = count, i;

	pdebug = 2;
	if (count < 128)
		return sci_data_in(regs, phase, count, data);

/*	printf("Called sci_pdma_in(0x%x, 0x%x, %d, 0x%x.\n", regs, phase, count, data); */

	WAIT_FOR_BSY(regs);
	regs->sci_mode |= SCI_MODE_DMA;
	regs->sci_icmd |= SCI_ICMD_DATA;
	regs->sci_irecv = 0;

	while (len >= 1024) {
		READY(0);
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;		/* 128 */
		READY(0);
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;		/* 256 */
		READY(0);
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;		/* 384 */
		READY(0);
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;		/* 512 */
		READY(0);
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;		/* 640 */
		READY(0);
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;		/* 768 */
		READY(0);
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;		/* 896 */
		READY(0);
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;		/* 1024 */
		len -= 1024;
	}
	while (len >= 128) {
		READY(0);
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;
		R4;		/* 128 */
		len -= 128;
	}
	while (len) {
		READY(0);
		R1;
		len--;
	}
scsi_timeout_error:
	regs->sci_mode &= ~SCI_MODE_DMA;
	return count - len;
}
#undef R4
#undef R1
#endif
