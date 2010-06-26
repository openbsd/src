/*	$OpenBSD: seagate.c,v 1.35 2010/06/26 23:24:44 guenther Exp $	*/

/*
 * ST01/02, Future Domain TMC-885, TMC-950 SCSI driver
 *
 * Copyright 1994, Charles Hannum (mycroft@ai.mit.edu)
 * Copyright 1994, Kent Palmkvist (kentp@isy.liu.se)
 * Copyright 1994, Robert Knier (rknier@qgraph.com)
 * Copyright 1992, 1994 Drew Eckhardt (drew@colorado.edu)
 * Copyright 1994, Julian Elischer (julian@tfs.com)
 *
 * Others that has contributed by example code is
 *		Glen Overby (overby@cray.com)
 *		Tatu Yllnen
 *		Brian E Litzinger
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPERS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * kentp  940307 alpha version based on newscsi-03 version of Julians SCSI-code
 * kentp  940314 Added possibility to not use messages
 * rknier 940331 Added fast transfer code
 * rknier 940407 Added assembler coded data transfers
 */

/*
 * What should really be done:
 *
 * Add missing tests for timeouts
 * Restructure interrupt enable/disable code (runs to long with int disabled)
 * Find bug? giving problem with tape status
 * Add code to handle Future Domain 840, 841, 880 and 881
 * adjust timeouts (startup is very slow)
 * add code to use tagged commands in SCSI2
 * Add code to handle slow devices better (sleep if device not disconnecting)
 * Fix unnecessary interrupts
 */

/*
 * Note to users trying to share a disk between DOS and unix:
 * The ST01/02 is a translating host-adapter. It is not giving DOS
 * the same number of heads/tracks/sectors as specified by the disk.
 * It is therefore important to look at what numbers DOS thinks the
 * disk has. Use these to disklabel your disk in an appropriate manner
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/malloc.h>

#include <machine/intr.h>
#include <machine/pio.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <i386/isa/isa_machdep.h>	/* XXX USES ISA HOLE DIRECTLY */

#define	SEA_SCB_MAX	32	/* allow maximally 8 scsi control blocks */
#define SCB_TABLE_SIZE	8	/* start with 8 scb entries in table */
#define BLOCK_SIZE	512	/* size of READ/WRITE areas on SCSI card */

/*
 * defining SEA_BLINDTRANSFER will make DATA IN and DATA OUT to be done with
 * blind transfers, i.e. no check is done for scsi phase changes. This will
 * result in data loss if the scsi device does not send its data using
 * BLOCK_SIZE bytes at a time.
 * If SEA_BLINDTRANSFER defined and SEA_ASSEMBLER also defined will result in
 * the use of blind transfers coded in assembler. SEA_ASSEMBLER is no good
 * without SEA_BLINDTRANSFER defined.
 */
#define	SEA_BLINDTRANSFER	/* do blind transfers */
#define	SEA_ASSEMBLER		/* Use assembly code for fast transfers */

/*
 * defining SEA_NOMSGS causes messages not to be used (thereby disabling
 * disconnects)
 */
#undef	SEA_NOMSGS

/*
 * defining SEA_NODATAOUT makes dataout phase being aborted
 */
#undef	SEA_NODATAOUT

/* Debugging definitions. Should not be used unless you want a lot of
   printouts even under normal conditions */

#undef	SEA_DEBUGQUEUE		/* Display info about queue-lengths */

/******************************* board definitions **************************/
/*
 * CONTROL defines
 */
#define CMD_RST		0x01		/* scsi reset */
#define CMD_SEL		0x02		/* scsi select */
#define CMD_BSY		0x04		/* scsi busy */
#define	CMD_ATTN	0x08		/* scsi attention */
#define CMD_START_ARB	0x10		/* start arbitration bit */
#define	CMD_EN_PARITY	0x20		/* enable scsi parity generation */
#define CMD_INTR	0x40		/* enable scsi interrupts */
#define CMD_DRVR_ENABLE	0x80		/* scsi enable */

/*
 * STATUS
 */
#define STAT_BSY	0x01		/* scsi busy */
#define STAT_MSG	0x02		/* scsi msg */
#define STAT_IO		0x04		/* scsi I/O */
#define STAT_CD		0x08		/* scsi C/D */
#define STAT_REQ	0x10		/* scsi req */
#define STAT_SEL	0x20		/* scsi select */
#define STAT_PARITY	0x40		/* parity error bit */
#define STAT_ARB_CMPL	0x80		/* arbitration complete bit */

/*
 * REQUESTS
 */
#define PH_DATAOUT	(0)
#define PH_DATAIN	(STAT_IO)
#define PH_CMD		(STAT_CD)
#define PH_STAT		(STAT_CD | STAT_IO)
#define PH_MSGOUT	(STAT_MSG | STAT_CD)
#define PH_MSGIN	(STAT_MSG | STAT_CD | STAT_IO)

#define PH_MASK		(STAT_MSG | STAT_CD | STAT_IO)

#define PH_INVALID	0xff

#define SEA_RAMOFFSET	0x00001800

#define BASE_CMD	(CMD_INTR | CMD_EN_PARITY)

#define	SEAGATE		1	/* Seagate ST0[12] */
#define	FDOMAIN		2	/* Future Domain TMC-{885,950} */
#define	FDOMAIN840	3	/* Future Domain TMC-{84[01],88[01]} */

/******************************************************************************/

/* scsi control block used to keep info about a scsi command */
struct sea_scb {
        u_char *data;			/* position in data buffer so far */
	int datalen;			/* bytes remaining to transfer */
	TAILQ_ENTRY(sea_scb) chain;
	struct scsi_xfer *xs;		/* the scsi_xfer for this cmd */
	int flags;			/* status of the instruction */
#define	SCB_FREE	0
#define	SCB_ACTIVE	1
#define SCB_ABORTED	2
#define SCB_TIMEOUT	4
#define SCB_ERROR	8
};

/*
 * data structure describing current status of the scsi bus. One for each
 * controller card.
 */
struct sea_softc {
	struct device sc_dev;
	struct isadev sc_id;
	void *sc_ih;

	int type;			/* board type */
	caddr_t	maddr;			/* Base address for card */
	caddr_t	maddr_cr_sr;		/* Address of control and status reg */
	caddr_t	maddr_dr;		/* Address of data register */

	struct scsi_link sc_link;	/* prototype for subdevs */
	TAILQ_HEAD(, sea_scb) free_list, ready_list, nexus_list;
	struct sea_scb *nexus;		/* currently connected command */
	int numscbs;			/* number of scsi control blocks */
	struct sea_scb scb[SCB_TABLE_SIZE];

	int our_id;			/* our scsi id */
	u_char our_id_mask;
	volatile u_char busy[8];	/* index=target, bit=lun, Keep track of
					   busy luns at device target */
};

/* flag showing if main routine is running. */
static volatile int main_running = 0;

#define	STATUS	(*(volatile u_char *)sea->maddr_cr_sr)
#define CONTROL	STATUS
#define DATA	(*(volatile u_char *)sea->maddr_dr)

/*
 * These are "special" values for the tag parameter passed to sea_select
 * Not implemented right now.
 */
#define TAG_NEXT	-1	/* Use next free tag */
#define TAG_NONE	-2	/*
				 * Establish I_T_L nexus instead of I_T_L_Q
				 * even on SCSI-II devices.
				 */

typedef struct {
	char *signature;
	int offset, length;
	int type;
} BiosSignature;

/*
 * Signatures for automatic recognition of board type
 */
static const BiosSignature signatures[] = {
{"ST01 v1.7  (C) Copyright 1987 Seagate", 15, 37, SEAGATE},
{"SCSI BIOS 2.00  (C) Copyright 1987 Seagate", 15, 40, SEAGATE},

/*
 * The following two lines are NOT mistakes. One detects ROM revision
 * 3.0.0, the other 3.2. Since seagate has only one type of SCSI adapter,
 * and this is not going to change, the "SEAGATE" and "SCSI" together
 * are probably "good enough"
 */
{"SEAGATE SCSI BIOS ", 16, 17, SEAGATE},
{"SEAGATE SCSI BIOS ", 17, 17, SEAGATE},

/*
 * However, future domain makes several incompatible SCSI boards, so specific
 * signatures must be used.
 */
{"FUTURE DOMAIN CORP. (C) 1986-1989 V5.0C2/14/89", 5, 45, FDOMAIN},
{"FUTURE DOMAIN CORP. (C) 1986-1989 V6.0A7/28/89", 5, 46, FDOMAIN},
{"FUTURE DOMAIN CORP. (C) 1986-1990 V6.0105/31/90",5, 47, FDOMAIN},
{"FUTURE DOMAIN CORP. (C) 1986-1990 V6.0209/18/90",5, 47, FDOMAIN},
{"FUTURE DOMAIN CORP. (C) 1986-1990 V7.009/18/90", 5, 46, FDOMAIN},
{"FUTURE DOMAIN CORP. (C) 1992 V8.00.004/02/92",   5, 44, FDOMAIN},
{"FUTURE DOMAIN TMC-950",			   5, 21, FDOMAIN},
};

#define	nsignatures	(sizeof(signatures) / sizeof(signatures[0]))

#ifdef notdef
static const char *bases[] = {
	(char *) 0xc8000, (char *) 0xca000, (char *) 0xcc000,
	(char *) 0xce000, (char *) 0xdc000, (char *) 0xde000
};

#define	nbases		(sizeof(bases) / sizeof(bases[0]))
#endif

struct		sea_scb *sea_get_scb(struct sea_softc *, int);
int		seaintr(void *);
void		sea_scsi_cmd(struct scsi_xfer *);
int 		sea_poll(struct sea_softc *, struct scsi_xfer *, int);
int		sea_select(struct sea_softc *sea, struct sea_scb *scb);
int		sea_transfer_pio(struct sea_softc *sea, u_char *phase,
		    int *count, u_char **data);
int		sea_abort(struct sea_softc *, struct sea_scb *scb);
static void 	sea_main(void);
static void 	sea_information_transfer(struct sea_softc *);
void		sea_timeout(void *);
void		sea_done(struct sea_softc *, struct sea_scb *);
void		sea_free_scb(struct sea_softc *, struct sea_scb *, int);
void 		sea_init(struct sea_softc *);
void 		sea_send_scb(struct sea_softc *sea, struct sea_scb *scb);
void		sea_reselect(struct sea_softc *sea);

struct scsi_adapter sea_switch = {
	sea_scsi_cmd,
	scsi_minphys,	/* no special minphys(), since driver uses PIO */
	0,
	0,
};

/* the below structure is so we have a default dev struct for our link struct */
struct scsi_device sea_dev = {
	NULL,		/* use default error handler */
	NULL,		/* have a queue, served by this */
	NULL,		/* have no async handler */
	NULL,		/* Use default 'done' routine */
};

int	seaprobe(struct device *, void *, void *);
void	seaattach(struct device *, struct device *, void *);
int	seaprint(void *, const char *);

struct cfattach sea_ca = {
	sizeof(struct sea_softc), seaprobe, seaattach
};

struct cfdriver sea_cd = {
	NULL, "sea", DV_DULL
};

#ifdef SEA_DEBUGQUEUE
void
sea_queue_length(struct sea_softc *sea)
{
	struct sea_scb *scb;
	int connected, issued, disconnected;

	connected = sea->nexus ? 1 : 0;
	issued = 0;
	TAILQ_FOREACH(scb, &sea->ready_list, chain)
		issued++;
	disconnected = 0;
	TAILQ_FOREACH(scb, &sea->nexus_list, chain)
		disconnected++;
	printf("%s: length: %d/%d/%d\n", sea->sc_dev.dv_xname, connected,
	    issued, disconnected);
}
#endif

/*
 * Check if the device can be found at the port given and if so, detect the
 * type the type of board.  Set it up ready for further work. Takes the isa_dev
 * structure from autoconf as an argument.
 * Returns 1 if card recognized, 0 if errors.
 */
int
seaprobe(struct device *parent, void *match, void *aux)
{
	struct sea_softc *sea = match;
	struct isa_attach_args *ia = aux;
	int i;

	/*
	 * Could try to find a board by looking through all possible addresses.
	 * This is not done the right way now, because I have not found a way
	 * to get a boards virtual memory address given its physical.  There is
	 * a function that returns the physical address for a given virtual
	 * address, but not the other way around.
	 */

	if (ia->ia_maddr == MADDRUNK) {
		/* XXX */
		return 0;
	} else
		sea->maddr = ISA_HOLE_VADDR(ia->ia_maddr);

	/* check board type */	/* No way to define this through config */
	for (i = 0; i < nsignatures; i++)
		if (!bcmp(sea->maddr + signatures[i].offset,
		    signatures[i].signature, signatures[i].length)) {
			sea->type = signatures[i].type;
			break;
		}

	/* Find controller and data memory addresses */
	switch (sea->type) {
	case SEAGATE:
	case FDOMAIN840:
		sea->maddr_cr_sr =
		    (void *) (((u_char *)sea->maddr) + 0x1a00);
		sea->maddr_dr =
		    (void *) (((u_char *)sea->maddr) + 0x1c00);
		break;
	case FDOMAIN:
		sea->maddr_cr_sr =
		    (void *) (((u_char *)sea->maddr) + 0x1c00);
		sea->maddr_dr =
		    (void *) (((u_char *)sea->maddr) + 0x1e00);
		break;
	default:
#if 0
		printf("%s: board type unknown at address %p\n",
		    sea->sc_dev.dv_xname, sea->maddr);
#endif
		return 0;
	}

	/* Test controller RAM (works the same way on future domain cards?) */
	*((u_char *)sea->maddr + SEA_RAMOFFSET) = 0xa5;
	*((u_char *)sea->maddr + SEA_RAMOFFSET + 1) = 0x5a;

	if ((*((u_char *)sea->maddr + SEA_RAMOFFSET) != 0xa5) ||
	    (*((u_char *)sea->maddr + SEA_RAMOFFSET + 1) != 0x5a)) {
		printf("%s: board RAM failure\n", sea->sc_dev.dv_xname);
		return 0;
	}

	ia->ia_drq = DRQUNK;
	ia->ia_msize = 0x2000;
	ia->ia_iosize = 0;
	return 1;
}

int
seaprint(void *aux, const char *name)
{
	if (name != NULL)
		printf("%s: scsibus ", name);
	return UNCONF;
}

/*
 * Attach all sub-devices we can find
 */
void
seaattach(struct device *parent, struct device *self, void *aux)
{
	struct isa_attach_args *ia = aux;
	struct sea_softc *sea = (void *)self;
	struct scsibus_attach_args saa;

	sea_init(sea);

	/*
	 * fill in the prototype scsi_link.
	 */
	sea->sc_link.adapter_softc = sea;
	sea->sc_link.adapter_target = sea->our_id;
	sea->sc_link.adapter = &sea_switch;
	sea->sc_link.device = &sea_dev;
	sea->sc_link.openings = 1;

	printf("\n");

	sea->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_BIO, seaintr, sea, sea->sc_dev.dv_xname);

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sea->sc_link;

	/*
	 * ask the adapter what subunits are present
	 */
	config_found(self, &saa, seaprint);
}

/*
 * Catch an interrupt from the adaptor
 */
int
seaintr(void *arg)
{
	struct sea_softc *sea = arg;

#ifdef DEBUG	/* extra overhead, and only needed for intr debugging */
	if ((STATUS & STAT_PARITY) == 0 &&
	    (STATUS & (STAT_SEL | STAT_IO)) != (STAT_SEL | STAT_IO))
		return 0;
#endif

loop:
	/* dispatch to appropriate routine if found and done=0 */
	/* should check to see that this card really caused the interrupt */

	if (STATUS & STAT_PARITY) {
		/* Parity error interrupt */
		printf("%s: parity error\n", sea->sc_dev.dv_xname);
		return 1;
	}

	if ((STATUS & (STAT_SEL | STAT_IO)) == (STAT_SEL | STAT_IO)) {
		/* Reselect interrupt */
		sea_reselect(sea);
		if (!main_running)
			sea_main();
		goto loop;
	}

	return 1;
}

/*
 * Setup data structures, and reset the board and the SCSI bus.
 */
void
sea_init(struct sea_softc *sea)
{
	int i;

	/* Reset the scsi bus (I don't know if this is needed */
	CONTROL = BASE_CMD | CMD_DRVR_ENABLE | CMD_RST;
	delay(25);	/* hold reset for at least 25 microseconds */
	CONTROL = BASE_CMD;
	delay(10);	/* wait a Bus Clear Delay (800 ns + bus free delay (800 ns) */

	/* Set our id (don't know anything about this) */
	switch (sea->type) {
	case SEAGATE:
		sea->our_id = 7;
		break;
	case FDOMAIN:
	case FDOMAIN840:
		sea->our_id = 6;
		break;
	}
	sea->our_id_mask = 1 << sea->our_id;

	/* init fields used by our routines */
	sea->nexus = 0;
	TAILQ_INIT(&sea->ready_list);
	TAILQ_INIT(&sea->nexus_list);
	TAILQ_INIT(&sea->free_list);
	for (i = 0; i < 8; i++)
		sea->busy[i] = 0x00;

	/* link up the free list of scbs */
	sea->numscbs = SCB_TABLE_SIZE;
	for (i = 0; i < SCB_TABLE_SIZE; i++) {
		TAILQ_INSERT_TAIL(&sea->free_list, &sea->scb[i], chain);
	}
}

/*
 * start a scsi operation given the command and the data address. Also needs
 * the unit, target and lu.
 */
void
sea_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *sc_link = xs->sc_link;
	struct sea_softc *sea = sc_link->adapter_softc;
	struct sea_scb *scb;
	int flags;
	int s;

	SC_DEBUG(sc_link, SDEV_DB2, ("sea_scsi_cmd\n"));

	flags = xs->flags;
	if ((scb = sea_get_scb(sea, flags)) == NULL) {
		xs->error = XS_NO_CCB;
		scsi_done(xs);
		return;
	}
	scb->flags = SCB_ACTIVE;
	scb->xs = xs;
	timeout_set(&scb->xs->stimeout, sea_timeout, scb);

	if (flags & SCSI_RESET) {
		/*
		 * Try to send a reset command to the card.
		 * XXX Not implemented.
		 */
		printf("%s: resetting\n", sea->sc_dev.dv_xname);
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		return;
	}

	/*
	 * Put all the arguments for the xfer in the scb
	 */
	scb->datalen = xs->datalen;
	scb->data = xs->data;

#ifdef SEA_DEBUGQUEUE
	sea_queue_length(sea);
#endif

	s = splbio();

	sea_send_scb(sea, scb);

	/*
	 * Usually return SUCCESSFULLY QUEUED
	 */
	if ((flags & SCSI_POLL) == 0) {
		timeout_add_msec(&scb->xs->stimeout, xs->timeout);
		splx(s);
		return;
	}

	splx(s);

	/*
	 * If we can't use interrupts, poll on completion
	 */
	if (sea_poll(sea, xs, xs->timeout)) {
		sea_timeout(scb);
		if (sea_poll(sea, xs, 2000))
			sea_timeout(scb);
	}
}

/*
 * Get a free scb. If there are none, see if we can allocate a new one.  If so,
 * put it in the hash table too; otherwise return an error or sleep.
 */
struct sea_scb *
sea_get_scb(struct sea_softc *sea, int flags)
{
	int s;
	struct sea_scb *scb;

	s = splbio();

	/*
	 * If we can and have to, sleep waiting for one to come free
	 * but only if we can't allocate a new one.
	 */
	for (;;) {
		scb = TAILQ_FIRST(&sea->free_list);
		if (scb) {
			TAILQ_REMOVE(&sea->free_list, scb, chain);
			break;
		}
		if (sea->numscbs < SEA_SCB_MAX) {
			scb = malloc(sizeof(*scb), M_TEMP, M_NOWAIT | M_ZERO);
			if (scb) {
				sea->numscbs++;
			} else
				printf("%s: can't malloc scb\n",
				    sea->sc_dev.dv_xname);
			break;
		}
		if ((flags & SCSI_NOSLEEP) != 0)
			break;
		tsleep(&sea->free_list, PRIBIO, "seascb", 0);
	}

	splx(s);
	return scb;
}

/*
 * Try to send this command to the board. Because this board does not use any
 * mailboxes, this routine simply adds the command to the queue held by the
 * sea_softc structure.
 * A check is done to see if the command contains a REQUEST_SENSE command, and
 * if so the command is put first in the queue, otherwise the command is added
 * to the end of the queue. ?? Not correct ??
 */
void
sea_send_scb(struct sea_softc *sea, struct sea_scb *scb)
{

	TAILQ_INSERT_TAIL(&sea->ready_list, scb, chain);
	/* Try to do some work on the card. */
	if (!main_running)
		sea_main();
}

/*
 * Coroutine that runs as long as more work can be done on the seagate host
 * adapter in a system.  Both sea_scsi_cmd and sea_intr will try to start it in
 * case it is not running.
 */
void
sea_main(void)
{
	struct sea_softc *sea;
	struct sea_scb *scb;
	int done;
	int unit;
	int s;

	main_running = 1;

	/*
	 * This should not be run with interrupts disabled, but use the splx
	 * code instead.
	 */
loop:
	done = 1;
	for (unit = 0; unit < sea_cd.cd_ndevs; unit++) {
		sea = sea_cd.cd_devs[unit];
		if (!sea)
			continue;
		s = splbio();
		if (!sea->nexus) {
			/*
			 * Search through the ready_list for a command
			 * destined for a target that's not busy.
			 */
			TAILQ_FOREACH(scb, &sea->ready_list, chain) {
				if (!(sea->busy[scb->xs->sc_link->target] &
				    (1 << scb->xs->sc_link->lun))) {
					TAILQ_REMOVE(&sea->ready_list, scb,
					    chain);

					/* Re-enable interrupts. */
					splx(s);

					/*
					 * Attempt to establish an I_T_L nexus.
					 * On success, sea->nexus is set.
					 * On failure, we must add the command
					 * back to the issue queue so we can
					 * keep trying.
					 */

					/*
					 * REQUEST_SENSE commands are issued
					 * without tagged queueing, even on
					 * SCSI-II devices because the
					 * contingent alligence condition
					 * exists for the entire unit.
					 */

					/*
					 * First check that if any device has
					 * tried a reconnect while we have done
					 * other things with interrupts
					 * disabled.
					 */

					if ((STATUS & (STAT_SEL | STAT_IO)) ==
					    (STAT_SEL | STAT_IO)) {
						sea_reselect(sea);
						break;
					}
					if (sea_select(sea, scb)) {
						s = splbio();
						TAILQ_INSERT_HEAD(&sea->ready_list,
						    scb, chain);
						splx(s);
					} else
						break;
				} /* if target/lun is not busy */
			} /* for scb */
			if (!sea->nexus) {
				/* check for reselection phase */
				if ((STATUS & (STAT_SEL | STAT_IO)) ==
				    (STAT_SEL | STAT_IO)) {
					sea_reselect(sea);
				}
			}
		} /* if (!sea->nexus) */

		splx(s);
		if (sea->nexus) {	/* we are connected. Do the task */
			sea_information_transfer(sea);
			done = 0;
		} else
			break;
	} /* for instance */

	if (!done)
		goto loop;

	main_running = 0;
}

void
sea_free_scb(struct sea_softc *sea, struct sea_scb *scb, int flags)
{
	int s;

	s = splbio();

	scb->flags = SCB_FREE;
	TAILQ_INSERT_HEAD(&sea->free_list, scb, chain);

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (TAILQ_NEXT(scb, chain) == NULL)
		wakeup((caddr_t)&sea->free_list);

	splx(s);
}

void
sea_timeout(void *arg)
{
	struct sea_scb *scb = arg;
	struct scsi_xfer *xs = scb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct sea_softc *sea = sc_link->adapter_softc;
	int s;

	sc_print_addr(sc_link);
	printf("timed out");

	s = splbio();

	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (scb->flags & SCB_ABORTED) {
		/* abort timed out */
		printf(" AGAIN\n");
		scb->xs->retries = 0;
		scb->flags |= SCB_ABORTED;
		sea_done(sea, scb);
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		scb->flags |= SCB_ABORTED;
		sea_abort(sea, scb);
		/* 2 secs for the abort */
		if ((xs->flags & SCSI_POLL) == 0)
			timeout_add_sec(&scb->xs->stimeout, 2);
	}

	splx(s);
}

void
sea_reselect(struct sea_softc *sea)
{
	u_char target_mask;
	int i;
	u_char lun, phase;
	u_char msg[3];
	int len;
	u_char *data;
	struct sea_scb *scb;
	int abort = 0;

	if (!((target_mask = STATUS) & STAT_SEL)) {
		printf("%s: wrong state 0x%x\n", sea->sc_dev.dv_xname,
		    target_mask);
		return;
	}

	/* wait for a device to win the reselection phase */
	/* signals this by asserting the I/O signal */
	for (i = 10; i && (STATUS & (STAT_SEL | STAT_IO | STAT_BSY)) !=
	    (STAT_SEL | STAT_IO | 0); i--);
	/* !! Check for timeout here */
	/* the data bus contains original initiator id ORed with target id */
	target_mask = DATA;
	/* see that we really are the initiator */
	if (!(target_mask & sea->our_id_mask)) {
		printf("%s: polled reselection was not for me: 0x%x\n",
		    sea->sc_dev.dv_xname, target_mask);
		return;
	}
	/* find target who won */
	target_mask &= ~sea->our_id_mask;
	/* host responds by asserting the BSY signal */
	CONTROL = BASE_CMD | CMD_DRVR_ENABLE | CMD_BSY;
	/* target should respond by deasserting the SEL signal */
	for (i = 50000; i && (STATUS & STAT_SEL); i++);
	/* remove the busy status */
	CONTROL = BASE_CMD | CMD_DRVR_ENABLE;
	/* we are connected. Now we wait for the MSGIN condition */
	for (i = 50000; i && !(STATUS & STAT_REQ); i--);
	/* !! Add timeout check here */
	/* hope we get an IDENTIFY message */
	len = 3;
	data = msg;
	phase = PH_MSGIN;
	sea_transfer_pio(sea, &phase, &len, &data);

	if (MSG_ISIDENTIFY(msg[0])) {
		printf("%s: expecting IDENTIFY message, got 0x%x\n",
		    sea->sc_dev.dv_xname, msg[0]);
		abort = 1;
		scb = NULL;
	} else {
		lun = msg[0] & 0x07;

		/*
		 * Find the command corresponding to the I_T_L or I_T_L_Q nexus
		 * we just reestablished, and remove it from the disconnected
		 * queue.
		 */
		TAILQ_FOREACH(scb, &sea->nexus_list, chain)
			if (target_mask == (1 << scb->xs->sc_link->target) &&
			    lun == scb->xs->sc_link->lun) {
				TAILQ_REMOVE(&sea->nexus_list, scb,
				    chain);
				break;
			}
		if (!scb) {
			printf("%s: target %02x lun %d not disconnected\n",
			    sea->sc_dev.dv_xname, target_mask, lun);
			/*
			 * Since we have an established nexus that we can't do
			 * anything with, we must abort it.
			 */
			abort = 1;
		}
	}

	if (abort) {
		msg[0] = MSG_ABORT;
		len = 1;
		data = msg;
		phase = PH_MSGOUT;
		CONTROL = BASE_CMD | CMD_ATTN;
		sea_transfer_pio(sea, &phase, &len, &data);
	} else
		sea->nexus = scb;

	return;
}

/*
 * Transfer data in given phase using polled I/O.
 */
int
sea_transfer_pio(struct sea_softc *sea, u_char *phase, int *count, u_char **data)
{
	u_char p = *phase, tmp;
	int c = *count;
	u_char *d = *data;
	int timeout;

	do {
		/*
		 * Wait for assertion of REQ, after which the phase bits will
		 * be valid.
		 */
		for (timeout = 0; timeout < 50000; timeout++)
			if ((tmp = STATUS) & STAT_REQ)
				break;
		if (!(tmp & STAT_REQ)) {
			printf("%s: timeout waiting for STAT_REQ\n",
			    sea->sc_dev.dv_xname);
			break;
		}

		/*
		 * Check for phase mismatch.  Reached if the target decides
		 * that it has finished the transfer.
		 */
		if (sea->type == FDOMAIN840)
			tmp = ((tmp & 0x08) >> 2) |
			      ((tmp & 0x02) << 2) |
			       (tmp & 0xf5);
		if ((tmp & PH_MASK) != p)
			break;

		/* Do actual transfer from SCSI bus to/from memory. */
		if (!(p & STAT_IO))
			DATA = *d;
		else
			*d = DATA;
		++d;

		/*
		 * The SCSI standard suggests that in MSGOUT phase, the
		 * initiator should drop ATN on the last byte of the message
		 * phase after REQ has been asserted for the handshake but
		 * before the initiator raises ACK.
		 * Don't know how to accomplish this on the ST01/02.
		 */

#if 0
		/*
		 * XXX
		 * The st01 code doesn't wait for STAT_REQ to be deasserted.
		 * Is this ok?
		 */
		for (timeout = 0; timeout < 200000L; timeout++)
			if (!(STATUS & STAT_REQ))
				break;
		if (STATUS & STAT_REQ)
			printf("%s: timeout on wait for !STAT_REQ",
			    sea->sc_dev.dv_xname);
#endif
	} while (--c);

	*count = c;
	*data = d;
	tmp = STATUS;
	if (tmp & STAT_REQ)
		*phase = tmp & PH_MASK;
	else
		*phase = PH_INVALID;

	if (c && (*phase != p))
		return -1;
	return 0;
}

/*
 * Establish I_T_L or I_T_L_Q nexus for new or existing command including
 * ARBITRATION, SELECTION, and initial message out for IDENTIFY and queue
 * messages.  Return -1 if selection could not execute for some reason, 0 if
 * selection succeded or failed because the target did not respond.
 */
int
sea_select(struct sea_softc *sea, struct sea_scb *scb)
{
	u_char msg[3], phase;
	u_char *data;
	int len;
	int timeout;

	CONTROL = BASE_CMD;
	DATA = sea->our_id_mask;
	CONTROL = (BASE_CMD & ~CMD_INTR) | CMD_START_ARB;

	/* wait for arbitration to complete */
	for (timeout = 0; timeout < 3000000L; timeout++)
		if (STATUS & STAT_ARB_CMPL)
			break;
	if (!(STATUS & STAT_ARB_CMPL)) {
		if (STATUS & STAT_SEL) {
			printf("%s: arbitration lost\n", sea->sc_dev.dv_xname);
			scb->flags |= SCB_ERROR;
		} else {
			printf("%s: arbitration timeout\n",
			    sea->sc_dev.dv_xname);
			scb->flags |= SCB_TIMEOUT;
		}
		CONTROL = BASE_CMD;
		return -1;
	}

	delay(2);
	DATA = (u_char)((1 << scb->xs->sc_link->target) | sea->our_id_mask);
	CONTROL =
#ifdef SEA_NOMSGS
	    (BASE_CMD & ~CMD_INTR) | CMD_DRVR_ENABLE | CMD_SEL;
#else
	    (BASE_CMD & ~CMD_INTR) | CMD_DRVR_ENABLE | CMD_SEL | CMD_ATTN;
#endif
	delay(1);

	/* wait for a bsy from target */
	for (timeout = 0; timeout < 2000000L; timeout++)
		if (STATUS & STAT_BSY)
			break;
	if (!(STATUS & STAT_BSY)) {
		/* should return some error to the higher level driver */
		CONTROL = BASE_CMD;
		scb->flags |= SCB_TIMEOUT;
		return 0;
	}

	/* Try to make the target to take a message from us */
#ifdef SEA_NOMSGS
	CONTROL = (BASE_CMD & ~CMD_INTR) | CMD_DRVR_ENABLE;
#else
	CONTROL = (BASE_CMD & ~CMD_INTR) | CMD_DRVR_ENABLE | CMD_ATTN;
#endif
	delay(1);

	/* should start a msg_out phase */
	for (timeout = 0; timeout < 2000000L; timeout++)
		if (STATUS & STAT_REQ)
			break;
	/* Remove ATN. */
	CONTROL = BASE_CMD | CMD_DRVR_ENABLE;
	if (!(STATUS & STAT_REQ)) {
		/*
		 * This should not be taken as an error, but more like an
		 * unsupported feature!  Should set a flag indicating that the
		 * target don't support messages, and continue without failure.
		 * (THIS IS NOT AN ERROR!)
		 */
	} else {
		msg[0] = MSG_IDENTIFY(scb->xs->sc_link->lun, 1);
		len = 1;
		data = msg;
		phase = PH_MSGOUT;
		/* Should do test on result of sea_transfer_pio(). */
		sea_transfer_pio(sea, &phase, &len, &data);
	}
	if (!(STATUS & STAT_BSY))
		printf("%s: after successful arbitrate: no STAT_BSY!\n",
		    sea->sc_dev.dv_xname);

	sea->nexus = scb;
	sea->busy[scb->xs->sc_link->target] |= 1 << scb->xs->sc_link->lun;
	/* This assignment should depend on possibility to send a message to target. */
	CONTROL = BASE_CMD | CMD_DRVR_ENABLE;
	/* XXX Reset pointer in command? */
	return 0;
}

/*
 * Send an abort to the target.  Return 1 success, 0 on failure.
 */
int
sea_abort(struct sea_softc *sea, struct sea_scb *scb)
{
	struct sea_scb *tmp;
	u_char msg, phase, *msgptr;
	int len;

	/*
	 * If the command hasn't been issued yet, we simply remove it from the
	 * issue queue
	 * XXX Could avoid this loop.
	 */
	TAILQ_FOREACH(tmp, &sea->ready_list, chain)
		if (scb == tmp) {
			TAILQ_REMOVE(&sea->ready_list, scb, chain);
			/* XXX Set some type of error result for operation. */
			return 1;
		}

	/*
	 * If any commands are connected, we're going to fail the abort and let
	 * the high level SCSI driver retry at a later time or issue a reset.
	 */
	if (sea->nexus)
		return 0;

	/*
	 * If the command is currently disconnected from the bus, and there are
	 * no connected commands, we reconnect the I_T_L or I_T_L_Q nexus
	 * associated with it, go into message out, and send an abort message.
	 */
	TAILQ_FOREACH(tmp, &sea->nexus_list, chain)
		if (scb == tmp) {
			if (sea_select(sea, scb))
				return 0;

			msg = MSG_ABORT;
			msgptr = &msg;
			len = 1;
			phase = PH_MSGOUT;
			CONTROL = BASE_CMD | CMD_ATTN;
			sea_transfer_pio(sea, &phase, &len, &msgptr);

			TAILQ_FOREACH(tmp, &sea->nexus_list, chain)
				if (scb == tmp) {
					TAILQ_REMOVE(&sea->nexus_list,
					    scb, chain);
					/* XXX Set some type of error result
					   for the operation. */
					return 1;
				}
		}

	/* Command not found in any queue; race condition? */
	return 1;
}

void
sea_done(struct sea_softc *sea, struct sea_scb *scb)
{
	struct scsi_xfer *xs = scb->xs;

	timeout_del(&scb->xs->stimeout);

	xs->resid = scb->datalen;

	/* XXXX need to get status */
	if (scb->flags == SCB_ACTIVE) {
		xs->resid = 0;
	} else {
		if (scb->flags & (SCB_TIMEOUT | SCB_ABORTED))
			xs->error = XS_TIMEOUT;
		if (scb->flags & SCB_ERROR)
			xs->error = XS_DRIVER_STUFFUP;
	}
	sea_free_scb(sea, scb, xs->flags);
	scsi_done(xs);
}

/*
 * Wait for completion of command in polled mode.
 */
int
sea_poll(struct sea_softc *sea, struct scsi_xfer *xs, int count)
{
	int s;

	while (count) {
		/* try to do something */
		s = splbio();
		if (!main_running)
			sea_main();
		splx(s);
		if (xs->flags & ITSDONE)
			return 0;
		delay(1000);
		count--;
	}
	return 1;
}

/*
 * Do the transfer.  We know we are connected.  Update the flags, and call
 * sea_done() when task accomplished.  Dialog controlled by the target.
 */
void
sea_information_transfer(struct sea_softc *sea)
{
	int timeout;
	u_char msgout = MSG_NOOP;
	int len;
	int s;
	u_char *data;
	u_char phase, tmp, old_phase = PH_INVALID;
	struct sea_scb *scb = sea->nexus;
	int loop;

	for (timeout = 0; timeout < 10000000L; timeout++) {
		tmp = STATUS;
		if (tmp & STAT_PARITY)
			printf("%s: parity error detected\n",
			    sea->sc_dev.dv_xname);
		if (!(tmp & STAT_BSY)) {
			for (loop = 0; loop < 20; loop++)
				if ((tmp = STATUS) & STAT_BSY)
					break;
			if (!(tmp & STAT_BSY)) {
				printf("%s: !STAT_BSY unit in data transfer!\n",
				    sea->sc_dev.dv_xname);
				s = splbio();
				sea->nexus = NULL;
				scb->flags = SCB_ERROR;
				splx(s);
				sea_done(sea, scb);
				return;
			}
		}

		/* we only have a valid SCSI phase when REQ is asserted */
		if (!(tmp & STAT_REQ))
			continue;

		if (sea->type == FDOMAIN840)
			tmp = ((tmp & 0x08) >> 2) |
			      ((tmp & 0x02) << 2) |
			       (tmp & 0xf5);
		phase = tmp & PH_MASK;
		if (phase != old_phase)
			old_phase = phase;

		switch (phase) {
		case PH_DATAOUT:
#ifdef SEA_NODATAOUT
			printf("%s: SEA_NODATAOUT set, attempted DATAOUT aborted\n",
			    sea->sc_dev.dv_xname);
			msgout = MSG_ABORT;
			CONTROL = BASE_CMD | CMD_ATTN;
			break;
#endif
		case PH_DATAIN:
			if (!scb->data)
				printf("no data address!\n");
#ifdef SEA_BLINDTRANSFER
			if (scb->datalen && !(scb->datalen % BLOCK_SIZE)) {
				while (scb->datalen) {
					for (loop = 0; loop < 50000; loop++)
						if ((tmp = STATUS) & STAT_REQ)
							break;
					if (!(tmp & STAT_REQ)) {
						printf("%s: timeout waiting for STAT_REQ\n",
						    sea->sc_dev.dv_xname);
						/* XXX Do something? */
					}
					if (sea->type == FDOMAIN840)
						tmp = ((tmp & 0x08) >> 2) |
						      ((tmp & 0x02) << 2) |
						       (tmp & 0xf5);
					if ((tmp & PH_MASK) != phase)
						break;
					if (!(phase & STAT_IO)) {
#ifdef SEA_ASSEMBLER
						int block = BLOCK_SIZE;
						void *a = sea->maddr_dr;
						asm("shr $2, %%ecx\n\t\
						    cld\n\t\
						    rep\n\t\
						    movsl" :
						    "=S" (scb->data),
							"=c" (block) ,
						    "=D" (a) :
						    "0" (scb->data),
						    "2" (a),
						    "1" (block) );
#else
						int count;
						for (count = 0;
						    count < BLOCK_SIZE;
						    count++)
							DATA = *(scb->data++);
#endif
					} else {
#ifdef SEA_ASSEMBLER
						int block = BLOCK_SIZE;
						void *a = sea->maddr_dr;
						asm("shr $2, %%ecx\n\t\
						    cld\n\t\
						    rep\n\t\
						    movsl" :
						    "=D" (scb->data), "=c" (block) ,
						    "=S" (a) :
						    "0" (scb->data),
							"2" (a) ,
						    "1" (block) );
#else
						int count;
					        for (count = 0;
						    count < BLOCK_SIZE;
						    count++)
							*(scb->data++) = DATA;
#endif
					}
					scb->datalen -= BLOCK_SIZE;
				}
			}
#endif
			if (scb->datalen)
				sea_transfer_pio(sea, &phase, &scb->datalen,
				    &scb->data);
			break;
		case PH_MSGIN:
			/* Multibyte messages should not be present here. */
			len = 1;
			data = &tmp;
			sea_transfer_pio(sea, &phase, &len, &data);
			/* scb->MessageIn = tmp; */

			switch (tmp) {
			case MSG_ABORT:
				scb->flags = SCB_ABORTED;
				printf("sea: command aborted by target\n");
				CONTROL = BASE_CMD;
				sea_done(sea, scb);
				return;
			case MSG_CMDCOMPLETE:
				s = splbio();
				sea->nexus = NULL;
				splx(s);
				sea->busy[scb->xs->sc_link->target] &=
				    ~(1 << scb->xs->sc_link->lun);
				CONTROL = BASE_CMD;
				sea_done(sea, scb);
				return;
			case MSG_MESSAGE_REJECT:
				printf("%s: message_reject received\n",
				    sea->sc_dev.dv_xname);
				break;
			case MSG_DISCONNECT:
				s = splbio();
				TAILQ_INSERT_TAIL(&sea->nexus_list,
				    scb, chain);
				sea->nexus = NULL;
				CONTROL = BASE_CMD;
				splx(s);
				return;
			case MSG_SAVEDATAPOINTER:
			case MSG_RESTOREPOINTERS:
				/* save/restore of pointers are ignored */
				break;
			default:
				/*
				 * This should be handled in the pio data
				 * transfer phase, as the ATN should be raised
				 * before ACK goes false when rejecting a
				 * message.
				 */
				printf("%s: unknown message in: %x\n",
				    sea->sc_dev.dv_xname, tmp);
				break;
			} /* switch (tmp) */
			break;
		case PH_MSGOUT:
			len = 1;
			data = &msgout;
			/* sea->last_message = msgout; */
			sea_transfer_pio(sea, &phase, &len, &data);
			if (msgout == MSG_ABORT) {
				printf("%s: sent message abort to target\n",
				    sea->sc_dev.dv_xname);
				s = splbio();
				sea->busy[scb->xs->sc_link->target] &=
				    ~(1 << scb->xs->sc_link->lun);
				sea->nexus = NULL;
				scb->flags = SCB_ABORTED;
				splx(s);
				/* enable interrupt from scsi */
				sea_done(sea, scb);
				return;
			}
			msgout = MSG_NOOP;
			break;
		case PH_CMD:
			len = scb->xs->cmdlen;
			data = (char *) scb->xs->cmd;
			sea_transfer_pio(sea, &phase, &len, &data);
			break;
		case PH_STAT:
			len = 1;
			data = &tmp;
			sea_transfer_pio(sea, &phase, &len, &data);
			scb->xs->status = tmp;
			break;
		default:
			printf("sea: unknown phase\n");
		} /* switch (phase) */
	} /* for (...) */

	/* If we get here we have got a timeout! */
	printf("%s: timeout in data transfer\n", sea->sc_dev.dv_xname);
	scb->flags = SCB_TIMEOUT;
	/* XXX Should I clear scsi-bus state? */
	sea_done(sea, scb);
}
