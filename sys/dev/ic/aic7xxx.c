/*	$NetBSD: aic7xxx.c,v 1.2 1996/01/13 02:05:22 thorpej Exp $	*/

/*
 * Generic driver for the aic7xxx based adaptec SCSI controllers
 * Copyright (c) 1994, 1995 Justin T. Gibbs.
 * All rights reserved.
 *
 * Product specific probe and attach routines can be found in:
 * i386/isa/aic7770.c	27/284X and aic7770 motherboard controllers
 * /pci/aic7870.c	294x and aic7870 motherboard controllers
 *
 * Portions of this driver are based on the FreeBSD 1742 Driver:
 *
 * Written by Julian Elischer (julian@tfs.com)
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
 * commenced: Sun Sep 27 18:14:01 PDT 1992
 */
/*
 * TODO:
 *	Add target reset capabilities
 *	Implement Target Mode
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <machine/pio.h>

#include <dev/isa/isareg.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsiconf.h>

#include <dev/ic/aic7xxxvar.h>

int     ahc_init __P((struct ahc_softc *));
void    ahc_loadseq __P((int));
int     ahc_scsi_cmd __P((struct scsi_xfer *));
void    ahc_timeout __P((void *));
void    ahc_done __P((struct ahc_softc *, struct ahc_scb *));
struct  ahc_scb *ahc_get_scb __P((struct ahc_softc *, int));
void    ahc_free_scb __P((struct ahc_softc *, struct ahc_scb *, int));
void	ahc_abort_scb __P((struct ahc_softc *, struct ahc_scb *));
void    ahcminphys __P((struct buf *));
int	ahc_poll __P((struct ahc_softc *, struct scsi_xfer *, int));

/* Different debugging levels */
#define AHC_SHOWMISC 0x0001
#define AHC_SHOWCMDS 0x0002
#define AHC_SHOWSCBS 0x0004
/*#define AHC_DEBUG /**/
int     ahc_debug = AHC_SHOWMISC;

/*#define AHC_MORE_DEBUG /**/

#ifdef AHC_MORE_DEBUG
#define DEBUGLEVEL  -1
#define DEBUGTARGET 0x0
#endif

/**** bit definitions for SCSIDEF ****/
#define	HSCSIID		0x07		/* our SCSI ID */
#define HWSCSIID	0x0f		/* our SCSI ID if Wide Bus */

struct scsi_adapter ahc_switch = {
	ahc_scsi_cmd,
	ahcminphys,
	0,
	0,
};


/* the below structure is so we have a default dev struct for our link struct */
struct scsi_device ahc_dev = {
	NULL,				/* Use default error handler */
	NULL,				/* have a queue, served by this */
	NULL,				/* have no async handler */
	NULL,				/* Use default 'done' routine */
};


/*
 * All of these should be in a separate header file shared by the sequencer
 * code and the kernel level driver.  The only catch is that we would need to
 * add an additional 0xc00 offset when using them in the kernel driver.  The
 * aic7770 assembler must be modified to allow include files as well.  All
 * page numbers refer to the Adaptec AIC-7770 Data Book available from
 * Adaptec's Technical Documents Department 1-800-934-2766
 */

/* -------------------- AIC-7770 offset definitions ----------------------- */

/*
 * SCSI Sequence Control (p. 3-11).
 * Each bit, when set starts a specific SCSI sequence on the bus
 */
#define SCSISEQ			0xc00ul
#define		TEMODEO		0x80
#define		ENSELO		0x40
#define		ENSELI		0x20
#define		ENRSELI		0x10
#define		ENAUTOATNO	0x08
#define		ENAUTOATNI	0x04
#define		ENAUTOATNP	0x02
#define		SCSIRSTO	0x01

/*
 * SCSI Transfer Control 1 Register (pp. 3-14,15).
 * Controls the SCSI module data path.
 */
#define	SXFRCTL1		0xc02ul
#define		BITBUCKET	0x80
#define		SWRAPEN		0x40
#define		ENSPCHK		0x20
#define		STIMESEL	0x18
#define		ENSTIMER	0x04
#define		ACTNEGEN	0x02
#define		STPWEN		0x01	/* Powered Termination */

/*
 * SCSI Interrrupt Mode 1 (pp. 3-28,29).
 * Set bits in this register enable the corresponding
 * interrupt source.
 */
#define	SIMODE1			0xc11ul
#define		ENSELTIMO	0x80
#define		ENATNTARG	0x40
#define		ENSCSIRST	0x20
#define		ENPHASEMIS	0x10
#define		ENBUSFREE	0x08
#define		ENSCSIPERR	0x04
#define		ENPHASECHG	0x02
#define		ENREQINIT	0x01

/*
 * SCSI Control Signal Read Register (p. 3-15).
 * Reads the actual state of the SCSI bus pins
 */
#define SCSISIGI		0xc03ul
#define		CDI		0x80
#define		IOI		0x40
#define		MSGI		0x20
#define		ATNI		0x10
#define		SELI		0x08
#define		BSYI		0x04
#define		REQI		0x02
#define		ACKI		0x01

/*
 * SCSI Contol Signal Write Register (p. 3-16).
 * Writing to this register modifies the control signals on the bus.  Only
 * those signals that are allowed in the current mode (Initiator/Target) are
 * asserted.
 */
#define SCSISIGO		0xc03ul
#define		CDO		0x80
#define		IOO		0x40
#define		MSGO		0x20
#define		ATNO		0x10
#define		SELO		0x08
#define		BSYO		0x04
#define		REQO		0x02
#define		ACKO		0x01

/* XXX document this thing */
#define SCSIRATE		0xc04ul

/*
 * SCSI ID (p. 3-18).
 * Contains the ID of the board and the current target on the
 * selected channel
 */
#define SCSIID			0xc05ul
#define		TID		0xf0		/* Target ID mask */
#define		OID		0x0f		/* Our ID mask */

/*
 * SCSI Status 0 (p. 3-21)
 * Contains one set of SCSI Interrupt codes
 * These are most likely of interest to the sequencer
 */
#define SSTAT0			0xc0bul
#define		TARGET		0x80		/* Board is a target */
#define		SELDO		0x40		/* Selection Done */
#define		SELDI		0x20		/* Board has been selected */
#define		SELINGO		0x10		/* Selection In Progress */
#define		SWRAP		0x08		/* 24bit counter wrap */
#define		SDONE		0x04		/* STCNT = 0x000000 */
#define		SPIORDY		0x02		/* SCSI PIO Ready */
#define		DMADONE		0x01		/* DMA transfer completed */

/*
 * Clear SCSI Interrupt 1 (p. 3-23)
 * Writing a 1 to a bit clears the associated SCSI Interrupt in SSTAT1.
 */
#define CLRSINT1		0xc0cul
#define		CLRSELTIMEO	0x80
#define		CLRATNO		0x40
#define		CLRSCSIRSTI	0x20
/*  UNUSED			0x10 */
#define		CLRBUSFREE	0x08
#define		CLRSCSIPERR	0x04
#define		CLRPHASECHG	0x02
#define		CLRREQINIT	0x01

/*
 * SCSI Status 1 (p. 3-24)
 * These interrupt bits are of interest to the kernel driver
 */
#define SSTAT1			0xc0cul
#define		SELTO		0x80
#define		ATNTARG		0x40
#define		SCSIRSTI	0x20
#define		PHASEMIS	0x10
#define		BUSFREE		0x08
#define		SCSIPERR	0x04
#define		PHASECHG	0x02
#define		REQINIT		0x01

/*
 * Selection/Reselection ID (p. 3-31)
 * Upper four bits are the device id.  The ONEBIT is set when the re/selecting
 * device did not set its own ID.
 */
#define SELID			0xc19ul
#define		SELID_MASK	0xf0
#define		ONEBIT		0x08
/*  UNUSED			0x07 */

/*
 * SCSI Block Control (p. 3-32)
 * Controls Bus type and channel selection.  In a twin channel configuration
 * addresses 0x00-0x1e are gated to the appropriate channel based on this
 * register.  SELWIDE allows for the coexistence of 8bit and 16bit devices
 * on a wide bus.
 */
#define SBLKCTL			0xc1ful
/*  UNUSED			0xc0 */
#define		AUTOFLUSHDIS	0x20
/*  UNUSED			0x10 */
#define		SELBUSB		0x08
/*  UNUSED			0x04 */
#define		SELWIDE		0x02
/*  UNUSED			0x01 */

/*
 * Sequencer Control (p. 3-33)
 * Error detection mode and speed configuration
 */
#define SEQCTL			0xc60ul
#define		PERRORDIS	0x80
#define		PAUSEDIS	0x40
#define		FAILDIS		0x20
#define		FASTMODE	0x10
#define		BRKADRINTEN	0x08
#define		STEP		0x04
#define		SEQRESET	0x02
#define		LOADRAM		0x01

/*
 * Sequencer RAM Data (p. 3-34)
 * Single byte window into the Scratch Ram area starting at the address
 * specified by SEQADDR0 and SEQADDR1.  To write a full word, simply write
 * four bytes in sucessesion.  The SEQADDRs will increment after the most
 * significant byte is written
 */
#define SEQRAM			0xc61ul

/*
 * Sequencer Address Registers (p. 3-35)
 * Only the first bit of SEQADDR1 holds addressing information
 */
#define SEQADDR0		0xc62ul
#define SEQADDR1		0xc63ul
#define		SEQADDR1_MASK	0x01

/*
 * Accumulator
 * We cheat by passing arguments in the Accumulator up to the kernel driver
 */
#define ACCUM			0xc64ul

#define SINDEX			0xc65ul

/*
 * Board Control (p. 3-43)
 */
#define BCTL			0xc84ul
/*   RSVD			0xf0 */
#define		ACE		0x08	/* Support for external processors */
/*   RSVD			0x06 */
#define		ENABLE		0x01

/*
 * Host Control (p. 3-47) R/W
 * Overal host control of the device.
 */
#define HCNTRL			0xc87ul
/*    UNUSED			0x80 */
#define		POWRDN		0x40
/*    UNUSED			0x20 */
#define		SWINT		0x10
#define		IRQMS		0x08
#define		PAUSE		0x04
#define		INTEN		0x02
#define		CHIPRST		0x01

/*
 * SCB Pointer (p. 3-49)
 * Gate one of the four SCBs into the SCBARRAY window.
 */
#define SCBPTR			0xc90ul

/*
 * Interrupt Status (p. 3-50)
 * Status for system interrupts
 */
#define INTSTAT			0xc91ul
#define		SEQINT_MASK	0xf0		/* SEQINT Status Codes */
#define			BAD_PHASE	0x00
#define			SEND_REJECT	0x10
#define			NO_IDENT	0x20
#define			NO_MATCH	0x30
#define			MSG_SDTR	0x40
#define			MSG_WDTR	0x50
#define			MSG_REJECT	0x60
#define			BAD_STATUS	0x70
#define			RESIDUAL	0x80
#define			ABORT_TAG	0x90
#define		BRKADRINT 0x08
#define		SCSIINT	  0x04
#define		CMDCMPLT  0x02
#define		SEQINT    0x01
#define		INT_PEND  (BRKADRINT | SEQINT | SCSIINT | CMDCMPLT)

/*
 * Hard Error (p. 3-53)
 * Reporting of catastrophic errors.  You usually cannot recover from
 * these without a full board reset.
 */
#define ERROR			0xc92ul
/*    UNUSED			0xf0 */
#define		PARERR		0x08
#define		ILLOPCODE	0x04
#define		ILLSADDR	0x02
#define		ILLHADDR	0x01

/*
 * Clear Interrupt Status (p. 3-52)
 */
#define CLRINT			0xc92ul
#define		CLRBRKADRINT	0x08
#define		CLRSCSIINT      0x04
#define		CLRCMDINT	0x02
#define		CLRSEQINT	0x01

/*
 * SCB Auto Increment (p. 3-59)
 * Byte offset into the SCB Array and an optional bit to allow auto
 * incrementing of the address during download and upload operations
 */
#define SCBCNT			0xc9aul
#define		SCBAUTO		0x80
#define		SCBCNT_MASK	0x1f

/*
 * Queue In FIFO (p. 3-60)
 * Input queue for queued SCBs (commands that the seqencer has yet to start)
 */
#define QINFIFO			0xc9bul

/*
 * Queue In Count (p. 3-60)
 * Number of queued SCBs
 */
#define QINCNT			0xc9cul

/*
 * Queue Out FIFO (p. 3-61)
 * Queue of SCBs that have completed and await the host
 */
#define QOUTFIFO		0xc9dul

/*
 * Queue Out Count (p. 3-61)
 * Number of queued SCBs in the Out FIFO
 */
#define QOUTCNT			0xc9eul

#define SCBARRAY		0xca0ul

/* ---------------- END AIC-7770 Register Definitions ----------------- */

/* --------------------- AIC-7870-only definitions -------------------- */

#define DSPCISTATUS		0xc86ul

/* ---------------------- Scratch RAM Offsets ------------------------- */
/* These offsets are either to values that are initialized by the board's
 * BIOS or are specified by the Linux sequencer code.  If I can figure out
 * how to read the EISA configuration info at probe time, the cards could
 * be run without BIOS support installed
 */

/*
 * 1 byte per target starting at this address for configuration values
 */
#define HA_TARG_SCRATCH		0xc20ul

/*
 * The sequencer will stick the frist byte of any rejected message here so
 * we can see what is getting thrown away.
 */
#define HA_REJBYTE		0xc31ul

/*
 * Length of pending message
 */
#define HA_MSG_LEN		0xc34ul

/*
 * message body
 */
#define HA_MSG_START		0xc35ul	/* outgoing message body */

/*
 * These are offsets into the card's scratch ram.  Some of the values are
 * specified in the AHA2742 technical reference manual and are initialized
 * by the BIOS at boot time.
 */
#define HA_ARG_1		0xc4aul
#define HA_RETURN_1		0xc4aul
#define		SEND_SENSE	0x80
#define		SEND_WDTR	0x80
#define		SEND_SDTR	0x80
#define		SEND_REJ	0x40

#define HA_SIGSTATE		0xc4bul

#define HA_SCBCOUNT		0xc52ul
#define HA_FLAGS		0xc53ul
#define		SINGLE_BUS	0x00
#define		TWIN_BUS	0x01
#define		WIDE_BUS	0x02
#define		ACTIVE_MSG	0x20
#define		IDENTIFY_SEEN	0x40
#define		RESELECTING	0x80

#define	HA_ACTIVE0		0xc54ul
#define	HA_ACTIVE1		0xc55ul
#define	SAVED_TCL		0xc56ul
#define WAITING_SCBH		0xc57ul
#define WAITING_SCBT		0xc58ul

#define HA_SCSICONF		0xc5aul
#define INTDEF			0xc5cul
#define HA_HOSTCONF		0xc5dul

#define MSG_ABORT               0x06
#define	BUS_8_BIT		0x00
#define BUS_16_BIT		0x01
#define BUS_32_BIT		0x02

/*
 * Since the sequencer can disable pausing in a critical section, we
 * must loop until it actually stops.
 * XXX Should add a timeout in here??
 */
#define PAUSE_SEQUENCER(ahc) \
	do {								\
		outb(HCNTRL + ahc->sc_iobase, ahc->pause);		\
		while ((inb(HCNTRL + ahc->sc_iobase) & PAUSE) == 0)	\
			;						\
	} while (0)

#define UNPAUSE_SEQUENCER(ahc) \
	do {								\
		outb(HCNTRL + ahc->sc_iobase, ahc->unpause);		\
	} while (0)

/*
 * Restart the sequencer program from address zero
 * XXX Should add a timeout in here??
 */
#define RESET_SEQUENCER(ahc) \
	do {								\
		do {							\
			outb(SEQCTL + ahc->sc_iobase, SEQRESET|FASTMODE); \
		} while (inb(SEQADDR0 + ahc->sc_iobase) != 0 &&		\
			 inb(SEQADDR1 + ahc->sc_iobase) != 0);		\
	} while (0)

#define RESTART_SEQUENCER(ahc) \
	do {								\
		RESET_SEQUENCER(ahc);					\
		UNPAUSE_SEQUENCER(ahc);					\
	} while (0)

#ifdef  AHC_DEBUG
void
ahc_print_scb(scb)
	struct ahc_scb *scb;
{

	printf("scb:0x%x control:0x%x tcl:0x%x cmdlen:%d cmdpointer:0x%x\n",
	    scb,
	    scb->control,
	    scb->target_channel_lun,
	    scb->cmdlen,
	    scb->cmdpointer);
	printf("\tdatlen:%d data:0x%x res:0x%x segs:0x%x segp:0x%x\n",
	    scb->datalen[2] << 16 | scb->datalen[1] << 8 | scb->datalen[0],
	    scb->data,
	    scb->RESERVED[1] << 8 | scb->RESERVED[0],
	    scb->SG_segment_count,
	    scb->SG_list_pointer);
	printf("\tsg_addr:%x sg_len:%d\n",
	    scb->ahc_dma[0].seg_addr,
	    scb->ahc_dma[0].seg_len);
	printf("	size:%d\n",
	    (int)&scb->next_waiting - (int)scb);
}

void
ahc_print_active_scb(ahc)
	struct ahc_softc *ahc;
{
	int iobase = ahc->sc_iobase;
	int scb_index;

	PAUSE_SEQUENCER(ahc);
	scb_index = inb(SCBPTR + iobase);
	UNPAUSE_SEQUENCER(ahc);

	ahc_print_scb(ahc->scbarray[scb_index]);
}
#endif

#define         PARERR          0x08
#define         ILLOPCODE       0x04
#define         ILLSADDR        0x02
#define         ILLHADDR        0x01

static struct {
	u_char errno;
	char *errmesg;
} hard_error[] = {
	{ ILLHADDR,  "Illegal Host Access" },
	{ ILLSADDR,  "Illegal Sequencer Address referrenced" },
	{ ILLOPCODE, "Illegal Opcode in sequencer program" },
	{ PARERR,    "Sequencer Ram Parity Error" }
};


/*
 * Valid SCSIRATE values.  (p. 3-17)
 * Provides a mapping of tranfer periods in ns to the proper value to
 * stick in the scsiscfr reg to use that transfer rate.
 */
static struct {
	u_char sxfr;
	int period; /* in ns */
	char *rate;
} ahc_syncrates[] = {
	{ 0x00, 100, "10.0"  },
	{ 0x10, 125,  "8.0"  },
	{ 0x20, 150,  "6.67" },
	{ 0x30, 175,  "5.7"  },
	{ 0x40, 200,  "5.0"  },
	{ 0x50, 225,  "4.4"  },
	{ 0x60, 250,  "4.0"  },
	{ 0x70, 275,  "3.6"  }
};

static int ahc_num_syncrates =
	sizeof(ahc_syncrates) / sizeof(ahc_syncrates[0]);

/*
 * Check if the device can be found at the port given
 * and if so, determine configuration and set it up for further work.
 */

int
ahcprobe(ahc, iobase)
	struct ahc_softc *ahc;
	int iobase;
{

	ahc->sc_iobase = iobase;

	/*
	 * Try to initialize a unit at this location
	 * reset the AIC-7770, read its registers,
	 * and fill in the dev structure accordingly
	 */

	if (ahc_init(ahc) != 0)
		return (0);

	return (1);
}


/*
 * Look up the valid period to SCSIRATE conversion in our table.
 */
static u_char
ahc_scsirate(offset, period, ahc, target)
	u_char offset;
	int period;
	struct ahc_softc *ahc;
	int target;
{
	u_char scsirate;
	int i;

	for (i = 0; i < ahc_num_syncrates; i++) {
		if ((ahc_syncrates[i].period - period) >= 0) {
			printf("%s: target %d synchronous at %sMB/s, "
			       "offset = %d\n",
			    ahc->sc_dev.dv_xname, target,
			    ahc_syncrates[i].rate, offset);
#ifdef AHC_DEBUG
#endif /* AHC_DEBUG */
			return ((ahc_syncrates[i].sxfr) | (offset & 0x0f));
		}
	}

	/* Default to asyncronous transfers.  Also reject this SDTR request. */
	printf("%s: target %d using asyncronous transfers\n",
	    ahc->sc_dev.dv_xname, target);
	return (0);
#ifdef AHC_DEBUG
#endif /* AHC_DEBUG */
}

ahcprint()
{

}

/*
 * Attach all the sub-devices we can find
 */
int
ahcattach(ahc)
	struct ahc_softc *ahc;
{

	TAILQ_INIT(&ahc->free_scb);

	/*
	 * fill in the prototype scsi_link.
	 */
	ahc->sc_link.adapter_softc = ahc;
	ahc->sc_link.adapter_target = ahc->ahc_scsi_dev;
	ahc->sc_link.adapter = &ahc_switch;
	ahc->sc_link.device = &ahc_dev;
	ahc->sc_link.openings = 1;
	ahc->sc_link.flags = DEBUGLEVEL;
	ahc->sc_link.quirks = 0;
	
	/*
	 * ask the adapter what subunits are present
	 */
	printf("%s: Probing channel A\n", ahc->sc_dev.dv_xname);
	config_found((void *)ahc, &ahc->sc_link, ahcprint);
	if (ahc->type & AHC_TWIN) {
		/* Configure the second scsi bus */
		ahc->sc_link_b = ahc->sc_link;
		/* XXXX Didn't do this before. */
		ahc->sc_link_b.adapter_target = ahc->ahc_scsi_dev_b;
		ahc->sc_link_b.quirks = 0x0008;	/**/
		printf("%s: Probing channel B\n", ahc->sc_dev.dv_xname);
		config_found((void *)ahc, &ahc->sc_link_b, ahcprint);
	}
	
	return 1;
}

void
ahc_send_scb(ahc, scb)
	struct ahc_softc *ahc;
	struct ahc_scb *scb;
{
	int iobase = ahc->sc_iobase;

	PAUSE_SEQUENCER(ahc);
	outb(QINFIFO + iobase, scb->position);
	UNPAUSE_SEQUENCER(ahc);
}

static void
ahc_getscb(iobase, scb)
	int iobase;
	struct ahc_scb *scb;
{

	outb(SCBCNT + iobase, SCBAUTO);
	insb(SCBARRAY + iobase, scb, SCB_UP_SIZE);
	outb(SCBCNT + iobase, 0);
}

/*
 * Catch an interrupt from the adaptor
 */
int
ahcintr(ahc)
	struct ahc_softc *ahc;
{
	int iobase = ahc->sc_iobase;
	u_char intstat = inb(INTSTAT + iobase);
	u_char status;
	struct ahc_scb *scb = NULL;
	struct scsi_xfer *xs = NULL;
	
	/*
	 * Is this interrupt for me? or for
	 * someone who is sharing my interrupt
	 */
	if ((intstat & INT_PEND) == 0)
		return 0;
	
	if (intstat & BRKADRINT) {
		/* We upset the sequencer :-( */

		/* Lookup the error message */
		int i, error = inb(ERROR + iobase);
		int num_errors =  sizeof(hard_error)/sizeof(hard_error[0]);
		for (i = 0; error != 1 && i < num_errors; i++)
			error >>= 1;
		panic("%s: brkadrint, %s at seqaddr = 0x%x\n",
		    ahc->sc_dev.dv_xname, hard_error[i].errmesg,
		    (inb(SEQADDR1 + iobase) << 8) |
		    (inb(SEQADDR0 + iobase) << 0));
	}
	
	if (intstat & SEQINT) {
		switch (intstat & SEQINT_MASK) {
		case BAD_PHASE:
			panic("%s: unknown scsi bus phase.  "
			      "Attempting to continue\n",
			    ahc->sc_dev.dv_xname);
			break;
		case SEND_REJECT:
			printf("%s: Warning - "
			       "message reject, message type: 0x%x\n",
			    ahc->sc_dev.dv_xname,
			    inb(HA_REJBYTE + iobase));
			break;
		case NO_IDENT:
			panic("%s: No IDENTIFY message from reconnecting "
			      "target %d at seqaddr = 0x%lx "
			      "SAVED_TCL == 0x%x\n",
			    ahc->sc_dev.dv_xname,
			    (inb(SELID + iobase) >> 4) & 0xf,
			    (inb(SEQADDR1 + iobase) << 8) |
			    (inb(SEQADDR0 + iobase) << 0),
			    inb(SAVED_TCL + iobase));
			break;
		case NO_MATCH: {
			u_char active;
			int active_port = HA_ACTIVE0 + iobase;
			int tcl = inb(SCBARRAY+1 + iobase);
			int target = (tcl >> 4) & 0x0f;
			printf("%s: no active SCB for reconnecting "
			       "target %d, channel %c - issuing ABORT\n",
			    ahc->sc_dev.dv_xname,
			    target, tcl & 0x08 ? 'B' : 'A');
			printf("SAVED_TCL == 0x%x\n", inb(SAVED_TCL + iobase));
			if (tcl & 0x88) {
				/* Second channel stores its info
				 * in byte two of HA_ACTIVE
				 */
				active_port++;
			}
			active = inb(active_port);
			active &= ~(0x01 << (target & 0x07));
			outb(SCBARRAY + iobase, SCB_NEEDDMA);
			outb(active_port, active);
			outb(CLRSINT1 + iobase, CLRSELTIMEO);
			RESTART_SEQUENCER(ahc);
			break;
		}
		case MSG_SDTR: {
			u_char scsi_id =
			    (inb(SCSIID + iobase) >> 0x4) |
			    (inb(SBLKCTL + iobase) & 0x08);
			u_char scratch, offset;
			int period;

			/*
			 * Help the sequencer to translate the
			 * negotiated transfer rate.  Transfer is
			 * 1/4 the period in ns as is returned by
			 * the sync negotiation message.  So, we must
			 * multiply by four
			 */
			period = inb(HA_ARG_1 + iobase) << 2;
			/* The bottom half of SCSIXFER */
			offset = inb(ACCUM + iobase);

			printf("%s: SDTR, target %d period %d offset %d\n",
			    ahc->sc_dev.dv_xname, scsi_id, period, offset);
			scratch = inb(HA_TARG_SCRATCH + iobase + scsi_id);
			scratch &= 0x80;
			scratch |= ahc_scsirate(offset, period, ahc, scsi_id);

			if ((scratch & 0x7f) == 0) {
				/*
				 * The requested rate was so low
				 * that asyncronous transfers are
				 * faster (not to mention the
				 * controller won't support them),
				 * so we issue a message reject to
				 * ensure we go to asyncronous
				 * transfers.
				 */
				outb(HA_RETURN_1 + iobase, SEND_REJ);
			} else if (ahc->sdtrpending & (0x01 << scsi_id)) {
				/*
				 * Don't send an SDTR back to the
				 * target, since we asked first.
				 */
				outb(HA_RETURN_1 + iobase, 0);
			} else {
				/*
				 * Send our own SDTR in reply
				 */
#ifdef AHC_DEBUG
				if (ahc_debug & AHC_SHOWMISC)
				    printf("Sending SDTR!!\n");
#endif
				outb(HA_RETURN_1 + iobase, SEND_SDTR);
			}
			/*
			 * Negate the flags
			 */
			ahc->needsdtr &= ~(0x01 << scsi_id);
			ahc->sdtrpending &= ~(0x01 << scsi_id);

			outb(HA_TARG_SCRATCH + iobase + scsi_id, scratch);
			outb(SCSIRATE + iobase, scratch);
			break;
		}
		case MSG_WDTR: {
			u_char scsi_id =
			    (inb(SCSIID + iobase) >> 0x4) |
			    (inb(SBLKCTL + iobase) & 0x08);
			u_char scratch, width;

			width = inb(ACCUM + iobase);

			scratch = inb(HA_TARG_SCRATCH + iobase + scsi_id);

			if (ahc->wdtrpending & (0x01 << scsi_id)) {
				/*
				 * Don't send a WDTR back to the
				 * target, since we asked first.
				 */
				outb(HA_RETURN_1 + iobase, 0);
				switch (width) {
				case BUS_8_BIT:
					scratch &= 0x7f;
					break;
				case BUS_16_BIT:
					printf("%s: target %d using 16Bit "
					       "transfers\n",
					    ahc->sc_dev.dv_xname, scsi_id);
					scratch &= 0xf8;
					scratch |= 0x88;
					break;
				case BUS_32_BIT:
					/* XXXX */
				}
			} else {
				/*
				 * Send our own WDTR in reply
				 */
				switch (width) {
				case BUS_8_BIT:
					scratch &= 0x7f;
					break;
				case BUS_32_BIT:
					/* Negotiate 16_BITS */
					width = BUS_16_BIT;
				case BUS_16_BIT:
					printf("%s: target %d using 16Bit "
					       "transfers\n",
					    ahc->sc_dev.dv_xname, scsi_id);
					scratch &= 0xf8;
					scratch |= 0x88;
					break;
				}
				outb(HA_RETURN_1 + iobase,
				     width | SEND_WDTR);
			}
			ahc->needwdtr &= ~(0x01 << scsi_id);
			ahc->wdtrpending &= ~(0x01 << scsi_id);

			outb(HA_TARG_SCRATCH + iobase + scsi_id, scratch);
			outb(SCSIRATE + iobase, scratch);
			break;
		}
		case MSG_REJECT: {
			/*
			 * What we care about here is if we had an
			 * outstanding SDTR or WDTR message for this
			 * target.  If we did, this is a signal that
			 * the target is refusing negotiation.
			 */

			u_char scsi_id =
			    (inb(SCSIID + iobase) >> 0x4) |
			    (inb(SBLKCTL + iobase) & 0x08);
			u_char scratch;
			u_short mask;

			scratch = inb(HA_TARG_SCRATCH + iobase + scsi_id);

			mask = (0x01 << scsi_id);
			if (ahc->wdtrpending & mask) {
				/* note 8bit xfers and clear flag */
				scratch &= 0x7f;
				ahc->needwdtr &= ~mask;
				ahc->wdtrpending &= ~mask;
				printf("%s: target %d refusing "
				       "WIDE negotiation.  Using "
				       "8bit transfers\n",
				    ahc->sc_dev.dv_xname, scsi_id);
			} else if (ahc->sdtrpending & mask) {
				/* note asynch xfers and clear flag */
				scratch &= 0xf0;
				ahc->needsdtr &= ~mask;
				ahc->sdtrpending &= ~mask;
				printf("%s: target %d refusing "
				       "syncronous negotiation; using "
				       "asyncronous transfers\n",
				    ahc->sc_dev.dv_xname, scsi_id);
			} else {
				/*
				 * Otherwise, we ignore it.
				 */
#ifdef AHC_DEBUG
				if (ahc_debug & AHC_SHOWMISC)
					printf("Message reject -- ignored\n");
#endif
				break;
			}

			outb(HA_TARG_SCRATCH + iobase + scsi_id, scratch);
			outb(SCSIRATE + iobase, scratch);
			break;
		}
		case BAD_STATUS: {
			int scb_index = inb(SCBPTR + iobase);
			scb = ahc->scbarray[scb_index];

			/*
			 * The sequencer will notify us when a command
			 * has an error that would be of interest to
			 * the kernel.  This allows us to leave the sequencer
			 * running in the common case of command completes
			 * without error.
			 */

			/*
			 * Set the default return value to 0 (don't
			 * send sense).  The sense code with change
			 * this if needed and this reduces code
			 * duplication.
			 */
			outb(HA_RETURN_1 + iobase, 0);
			if (!scb || scb->flags == SCB_FREE) {
				printf("%s: ahcintr: referenced scb not "
				       "valid during seqint 0x%x scb(%d)\n",
				    ahc->sc_dev.dv_xname, intstat, scb_index);
				goto clear;
			}

			xs = scb->xs;

			ahc_getscb(iobase, scb);

#ifdef AHC_MORE_DEBUG
			if (xs->sc_link->target == DEBUGTARGET)
				ahc_print_scb(scb);
#endif
			xs->status = scb->target_status;
			switch (scb->target_status) {
			case SCSI_OK:
				printf("%s: Interrupted for status of 0???\n",
				    ahc->sc_dev.dv_xname);
				break;
			case SCSI_CHECK:
#ifdef AHC_DEBUG
				sc_print_addr(xs->sc_link);
				printf("requests Check Status\n");
#endif

				if (xs->error == XS_NOERROR &&
				    scb->flags != SCB_CHKSENSE) {
					u_char flags;
					u_char head;
					u_char tail;
					struct ahc_dma_seg *sg = scb->ahc_dma;
					struct scsi_sense *sc = &(scb->sense_cmd);
					u_char control = scb->control;
					u_char tcl = scb->target_channel_lun;
#ifdef AHC_DEBUG
					sc_print_addr(xs->sc_link);
					printf("Sending Sense\n");
#endif
					bzero(scb, SCB_DOWN_SIZE);
					scb->flags = SCB_CHKSENSE;
					scb->control = (control & SCB_TE);
					sc->opcode = REQUEST_SENSE;
					sc->byte2 =  xs->sc_link->lun << 5;
					sc->length = sizeof(struct scsi_sense_data);
					sc->control = 0;

					sg->seg_addr = vtophys(&xs->sense);
					sg->seg_len = sizeof(struct scsi_sense_data);

					scb->target_channel_lun = tcl;
					scb->SG_segment_count = 1;
					scb->SG_list_pointer = vtophys(sg);
					scb->cmdpointer = vtophys(sc);
					scb->cmdlen = sizeof(*sc);

					outb(SCBCNT + iobase, SCBAUTO);
					outsb(SCBARRAY + iobase, scb,
					    SCB_DOWN_SIZE);
					outb(SCBCNT + iobase, 0);
					outb(SCBARRAY + iobase + 30,
					    SCB_LIST_NULL);

					/*
					 * Add this SCB to the "waiting for
					 * selection" list.
					 */
					head = inb(WAITING_SCBH + iobase);
					tail = inb(WAITING_SCBT + iobase);
					if (head & SCB_LIST_NULL) {
						/* List was empty */
						head = scb->position;
						tail = SCB_LIST_NULL;
					} else if (tail & SCB_LIST_NULL) {
						/* List had one element */
						tail = scb->position;
						outb(SCBPTR + iobase, head);
						outb(SCBARRAY + iobase + 30,
						    tail);
					} else {
						outb(SCBPTR + iobase, tail);
						tail = scb->position;
						outb(SCBARRAY + iobase + 30,
						    tail);
					}
					outb(WAITING_SCBH + iobase, head);
					outb(WAITING_SCBT + iobase, tail);
					outb(HA_RETURN_1 + iobase, SEND_SENSE);
					break;
				}
				/*
				 * Have the sequencer do a normal command
				 * complete with either a "DRIVER_STUFFUP"
				 * error or whatever other error condition
				 * we already had.
				 */
				if (xs->error == XS_NOERROR)
					xs->error = XS_DRIVER_STUFFUP;
				break;
			case SCSI_BUSY:
				sc_print_addr(xs->sc_link);
				printf("Target Busy\n");
				xs->error = XS_BUSY;
				break;
#if 0
			case SCSI_QUEUE_FULL:
				/*
				 * The upper level SCSI code will eventually
				 * handle this properly.
				 */
				sc_print_addr(xs->sc_link);
				printf("Queue Full\n");
				xs->error = XS_BUSY;
				break;
#endif
			default:
				sc_print_addr(xs->sc_link);
				printf("unexpected targ_status: %x\n",
				    scb->target_status);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
			break;
		}
		case RESIDUAL: {
			int scb_index = inb(SCBPTR + iobase);
			scb = ahc->scbarray[scb_index];

			/*
			 * Don't clobber valid resid info with
			 * a resid coming from a check sense
			 * operation.
			 */
			if (scb->flags != SCB_CHKSENSE)
				scb->xs->resid =
				    (inb(iobase + SCBARRAY + 17) << 16) |
				    (inb(iobase + SCBARRAY + 16) <<  8) |
				    (inb(iobase + SCBARRAY + 15) <<  0);
#ifdef AHC_MORE_DEBUG
			printf("ahc: Handled Residual\n");
#endif
			break;
		}
		case ABORT_TAG: {
			int scb_index = inb(SCBPTR + iobase);
			scb = ahc->scbarray[scb_index];

			/*
			 * We didn't recieve a valid tag back from
			 * the target on a reconnect.
			 */
			sc_print_addr(xs->sc_link);
			printf("invalid tag recieved on channel %c "
			       "-- sending ABORT_TAG\n",
			    (xs->sc_link->quirks & 0x08) ? 'B' : 'A');
			scb->xs->error = XS_DRIVER_STUFFUP;
			untimeout(ahc_timeout, scb);
			ahc_done(ahc, scb);
			break;
		}
		default:
			printf("%s: seqint, intstat == 0x%x, scsisigi = 0x%x\n",
			    ahc->sc_dev.dv_xname,
			    intstat, inb(SCSISIGI + iobase));
			break;
		}

	clear:
		/*
		 * Clear the upper byte that holds SEQINT status
		 * codes and clear the SEQINT bit.
		 */
		outb(CLRINT + iobase, CLRSEQINT);

		/*
		 *  The sequencer is paused immediately on
		 *  a SEQINT, so we should restart it when
		 *  we leave this section.
		 */
		UNPAUSE_SEQUENCER(ahc);
	}
	
	if (intstat & SCSIINT) {
		int scb_index = inb(SCBPTR + iobase);
		scb = ahc->scbarray[scb_index];

		status = inb(SSTAT1 + iobase);

		if (!scb || scb->flags == SCB_FREE) {
			printf("%s: ahcintr - referenced scb not "
			       "valid during scsiint 0x%x scb(%d)\n",
			    ahc->sc_dev.dv_xname, status, scb_index);
			outb(CLRSINT1 + iobase, status);
			UNPAUSE_SEQUENCER(ahc);
			outb(CLRINT + iobase, CLRSCSIINT);
			scb = NULL;
			goto cmdcomplete;
		}
		xs = scb->xs;

#ifdef AHC_MORE_DEBUG
		if ((xs->sc_link->target & 0xf) == DEBUGTARGET)
			printf("Intr status %x\n", status);
#endif

		if (status & SELTO) {
			u_char active;
			u_char waiting;
			u_char flags;
			int active_port = HA_ACTIVE0 + iobase;

			outb(SCSISEQ + iobase, ENRSELI);
			xs->error = XS_SELTIMEOUT;
			/*
			 * Clear any pending messages for the timed out
			 * target, and mark the target as free
			 */
			flags = inb(HA_FLAGS + iobase);
			outb(HA_FLAGS + iobase, flags & ~ACTIVE_MSG);

			if (scb->target_channel_lun & 0x88)
			    active_port++;

			active = inb(active_port) &
			    ~(0x01 << (xs->sc_link->target & 0x07));
			outb(active_port, active);

			outb(SCBARRAY + iobase, SCB_NEEDDMA);

			outb(CLRSINT1 + iobase, CLRSELTIMEO);

			outb(CLRINT + iobase, CLRSCSIINT);

			/* Shift the waiting for selection queue forward */
			waiting = inb(WAITING_SCBH + iobase);
			outb(SCBPTR + iobase, waiting);
			waiting = inb(SCBARRAY + iobase + 30);
			outb(WAITING_SCBH + iobase, waiting);

			RESTART_SEQUENCER(ahc);
		}

		if (status & SCSIPERR) {
			sc_print_addr(xs->sc_link);
			printf("parity error on channel %c\n",
			    (xs->sc_link->quirks & 0x08) ? 'B' : 'A');
			xs->error = XS_DRIVER_STUFFUP;

			outb(CLRSINT1 + iobase, CLRSCSIPERR);
			UNPAUSE_SEQUENCER(ahc);

			outb(CLRINT + iobase, CLRSCSIINT);
			scb = NULL;
		}
		if (status & BUSFREE) {
#if 0
			/*
			 * Has seen busfree since selection, i.e.
			 * a "spurious" selection. Shouldn't happen.
			 */
			printf("ahc: unexpected busfree\n");
#if 0
			xs->error = XS_DRIVER_STUFFUP;
			outb(CLRSINT1 + iobase, BUSFREE); /* CLRBUSFREE */
#endif
#endif
		} else {
			printf("%s: Unknown SCSIINT. Status = 0x%x\n",
			    ahc->sc_dev.dv_xname, status);
			outb(CLRSINT1 + iobase, status);
			UNPAUSE_SEQUENCER(ahc);
			outb(CLRINT + iobase, CLRSCSIINT);
			scb = NULL;
		}
		if (scb != NULL) {
			/* We want to process the command */
			untimeout(ahc_timeout, scb);
			ahc_done(ahc, scb);
		}
	}

cmdcomplete:
	if (intstat & CMDCMPLT) {
		int scb_index;

		do {
			scb_index = inb(QOUTFIFO + iobase);
			scb = ahc->scbarray[scb_index];

			if (!scb || scb->flags == SCB_FREE) {
				printf("%s: WARNING "
			               "no command for scb %d (cmdcmplt)\n"
				       "QOUTCNT == %d\n",
				    ahc->sc_dev.dv_xname,
				    scb_index, inb(QOUTCNT + iobase));
				outb(CLRINT + iobase, CLRCMDINT);
				continue;
			}

			/* XXXX Should do this before reading FIFO? */
			outb(CLRINT + iobase, CLRCMDINT);
			untimeout(ahc_timeout, scb);
			ahc_done(ahc, scb);
		} while (inb(QOUTCNT + iobase));
	}
	
	return 1;
}

/*
 * We have a scb which has been processed by the
 * adaptor, now we look to see how the operation
 * went.
 */
void
ahc_done(ahc, scb)
	struct ahc_softc *ahc;
	struct ahc_scb *scb;
{
	struct scsi_xfer *xs = scb->xs;
	
#ifdef AHC_MORE_DEBUG
	if ((xs->sc_link->target & 0xf) == DEBUGTARGET) {
		xs->sc_link->flags |= 0xf0;
		SC_DEBUG(xs->sc_link, SDEV_DB2, ("ahc_done\n"));
		printf("%x %x %x %x\n",
		    scb->flags,
		    scb->target_status,
		    xs->flags,
		    xs->error);
	}
#endif
	
	/*
	 * Put the results of the operation
	 * into the xfer and call whoever started it
	 */
	if (xs->error == XS_NOERROR) {
		if (scb->flags == SCB_ABORTED)
			xs->error = XS_DRIVER_STUFFUP;
		else if (scb->flags == SCB_CHKSENSE)
			xs->error = XS_SENSE;
	}
	
	xs->flags |= ITSDONE;
	
#ifdef AHC_TAGENABLE
	if (xs->cmd->opcode == 0x12 && xs->error == XS_NOERROR) {
		struct scsi_inquiry_data *inq_data;
		u_short mask = 0x01 << (xs->sc_link->target |
					(scb->target_channel_lun & 0x08));
		/*
		 * Sneak a look at the results of the SCSI Inquiry
		 * command and see if we can do Tagged queing.  XXX This
		 * should really be done by the higher level drivers.
		 */
		inq_data = (struct scsi_inquiry_data *)xs->data;
		if (((inq_data->device & SID_TYPE) == 0)
		    && (inq_data->flags & SID_CmdQue)
		    && !(ahc->tagenable & mask)) {
			/*
			 * Disk type device and can tag
			 */
			sc_print_addr(xs->sc_link);
			printf("Tagged Queuing Device\n");
			ahc->tagenable |= mask;
#ifdef QUEUE_FULL_SUPPORTED
			xs->sc_link->openings += 2; */
#endif
		}
	}
#endif
	
	ahc_free_scb(ahc, scb, xs->flags);
	scsi_done(xs);
}

/*
 * Start the board, ready for normal operation
 */
/* XXXX clean */
int
ahc_init(ahc)
	struct ahc_softc *ahc;
{
	int iobase = ahc->sc_iobase;
	u_char scsi_conf, sblkctl, i;
	int intdef, max_targ = 16, wait;

	/*
	 * Assume we have a board at this stage
	 * Find out the configured interupt and the card type.
	 */

#ifdef AHC_DEBUG
	printf("%s: scb %d bytes; SCB_SIZE %d bytes, ahc_dma %d bytes\n",
	    ahc->sc_dev.dv_xname, sizeof(struct ahc_scb), SCB_DOWN_SIZE,
	    sizeof(struct ahc_dma_seg));
#endif /* AHC_DEBUG */
	/*printf("%s: reading board settings\n", ahc->sc_dev.dv_xname);/**/
	
	/* Save the IRQ type before we do a chip reset */
	
	ahc->unpause = (inb(HCNTRL + iobase) & IRQMS) | INTEN;
	ahc->pause = ahc->unpause | PAUSE;
	outb(HCNTRL + iobase, CHIPRST | ahc->pause);
	
	/*
	 * Ensure that the reset has finished
	 */
	wait = 1000;
	while (wait--) {
		delay(1000);
		if (!(inb(HCNTRL + iobase) & CHIPRST))
		    break;
	}
	if (wait == 0) {
		printf("\n%s: WARNING - Failed chip reset!  "
		       "Trying to initialize anyway.\n", ahc->sc_dev.dv_xname);
		/* Forcibly clear CHIPRST */
		outb(HCNTRL + iobase, ahc->pause);
	}

	switch (ahc->type) {
	case AHC_274:
		printf("%s: 274x ", ahc->sc_dev.dv_xname);
		ahc->maxscbs = 0x4;
		break;
	case AHC_284:
		printf("%s: 284x ", ahc->sc_dev.dv_xname);
		ahc->maxscbs = 0x4;
		break;
	case AHC_AIC7870:
	case AHC_294:
		if (ahc->type == AHC_AIC7870)
			printf("%s: aic7870 ", ahc->sc_dev.dv_xname);
		else
			printf("%s: 294x ", ahc->sc_dev.dv_xname);
		ahc->maxscbs = 0x10;
		#define DFTHRESH        3
		outb(DSPCISTATUS + iobase, DFTHRESH << 6);
		/*
		 * XXX Hard coded SCSI ID until we can read it from the
		 * SEEPROM or NVRAM.
		 */
		outb(HA_SCSICONF + iobase, 0x07 | (DFTHRESH << 6));
		/* In case we are a wide card */
		outb(HA_SCSICONF + 1 + iobase, 0x07);
		break;
	default:
	        printf("%s: unknown(0x%x) ", ahc->sc_dev.dv_xname, ahc->type);
		break;
	}
	
	/* Determine channel configuration and who we are on the scsi bus. */
	switch ((sblkctl = inb(SBLKCTL + iobase) & 0x0f)) {
	case 0:
		ahc->ahc_scsi_dev = (inb(HA_SCSICONF + iobase) & HSCSIID);
		printf("Single Channel, SCSI Id=%d, ", ahc->ahc_scsi_dev);
		outb(HA_FLAGS + iobase, SINGLE_BUS);
		break;
	case 2:
		ahc->ahc_scsi_dev = (inb(HA_SCSICONF + 1 + iobase) & HWSCSIID);
		printf("Wide Channel, SCSI Id=%d, ", ahc->ahc_scsi_dev);
		ahc->type |= AHC_WIDE;
		outb(HA_FLAGS + iobase, WIDE_BUS);
		break;
	case 8:
		ahc->ahc_scsi_dev = (inb(HA_SCSICONF + iobase) & HSCSIID);
		ahc->ahc_scsi_dev_b = (inb(HA_SCSICONF + 1 + iobase) & HSCSIID);
		printf("Twin Channel, A SCSI Id=%d, B SCSI Id=%d, ",
		    ahc->ahc_scsi_dev, ahc->ahc_scsi_dev_b);
		ahc->type |= AHC_TWIN;
		outb(HA_FLAGS + iobase, TWIN_BUS);
		break;
	default:
		printf(" Unsupported adapter type.  %x Ignoring\n", sblkctl);
		return(-1);
	}

	/*
	 * Take the bus led out of diagnostic mode
	 */
	outb(SBLKCTL + iobase, sblkctl);

	/*
	 * Number of SCBs that will be used. Rev E aic7770s and
	 * aic7870s have 16.  The rest have 4.
	 */
	if (!(ahc->type & AHC_AIC7870)) {
		/*
		 * See if we have a Rev E or higher
		 * aic7770. Anything below a Rev E will
		 * have a R/O autoflush disable configuration
		 * bit.
		 */
		u_char sblkctl_orig;
		sblkctl_orig = inb(SBLKCTL + iobase);
		sblkctl = sblkctl_orig ^ AUTOFLUSHDIS;
		outb(SBLKCTL + iobase, sblkctl);
		sblkctl = inb(SBLKCTL + iobase);
		if (sblkctl != sblkctl_orig) {
			printf("aic7770 >= Rev E, ");
			/*
			 * Ensure autoflush is enabled
			 */
			sblkctl &= ~AUTOFLUSHDIS;
			outb(SBLKCTL + iobase, sblkctl);
		} else
			printf("aic7770 <= Rev C, ");
	} else
		printf("aic7870, ");
	printf("%d SCBs\n", ahc->maxscbs);
	
	if (ahc->pause & IRQMS)
		printf("%s: Using Level Sensitive Interrupts\n",
		    ahc->sc_dev.dv_xname);
	else
		printf("%s: Using Edge Triggered Interrupts\n",
		    ahc->sc_dev.dv_xname);
	
	if (!(ahc->type & AHC_AIC7870)) {
		/*
		 * The 294x cards are PCI, so we get their interrupt from the
		 * PCI BIOS.
		 */

		intdef = inb(INTDEF + iobase);
		switch (intdef & 0xf) {
		case 9:
			ahc->sc_irq = 9;
			break;
		case 10:
			ahc->sc_irq = 10;
			break;
		case 11:
			ahc->sc_irq = 11;
			break;
		case 12:
			ahc->sc_irq = 12;
			break;
		case 14:
			ahc->sc_irq = 14;
			break;
		case 15:
			ahc->sc_irq = 15;
			break;
		default:
			printf("illegal irq setting\n");
			return (EIO);
		}
	}
	
	/* Set the SCSI Id, SXFRCTL1, and SIMODE1, for both channels */
	if (ahc->type & AHC_TWIN) {
		/*
		 * The device is gated to channel B after a chip reset,
		 * so set those values first
		 */
		outb(SCSIID + iobase, ahc->ahc_scsi_dev_b);
		scsi_conf = inb(HA_SCSICONF + 1 + iobase) & (ENSPCHK|STIMESEL);
		outb(SXFRCTL1 + iobase, scsi_conf|ENSTIMER|ACTNEGEN|STPWEN);
		outb(SIMODE1 + iobase, ENSELTIMO|ENSCSIPERR);
		/* Select Channel A */
		outb(SBLKCTL + iobase, 0);
	}
	outb(SCSIID + iobase, ahc->ahc_scsi_dev);
	scsi_conf = inb(HA_SCSICONF + iobase) & (ENSPCHK|STIMESEL);
	outb(SXFRCTL1 + iobase, scsi_conf|ENSTIMER|ACTNEGEN|STPWEN);
	outb(SIMODE1 + iobase, ENSELTIMO|ENSCSIPERR);
	
	/*
	 * Look at the information that board initialization or
	 * the board bios has left us.  In the lower four bits of each
	 * target's scratch space any value other than 0 indicates
	 * that we should initiate syncronous transfers.  If it's zero,
	 * the user or the BIOS has decided to disable syncronous
	 * negotiation to that target so we don't activate the needsdr
	 * flag.
	 */
	ahc->needsdtr_orig = 0;
	ahc->needwdtr_orig = 0;
	if (!(ahc->type & AHC_WIDE))
		max_targ = 8;

	for (i = 0; i < max_targ; i++) {
		u_char target_settings = inb(HA_TARG_SCRATCH + i + iobase);
#if 0 /* XXXX */
		target_settings |= 0x8f;
#endif
		if (target_settings & 0x0f) {
			ahc->needsdtr_orig |= (0x01 << i);
			/* Default to a asyncronous transfers (0 offset) */
			target_settings &= 0xf0;
		}
		if (target_settings & 0x80) {
			ahc->needwdtr_orig |= (0x01 << i);
			/*
			 * We'll set the Wide flag when we
			 * are successful with Wide negotiation,
			 * so turn it off for now so we aren't
			 * confused.
			 */
			target_settings &= 0x7f;
		}
		outb(HA_TARG_SCRATCH + i + iobase, target_settings);
	}
	/*
	 * If we are not a WIDE device, forget WDTR.  This
	 * makes the driver work on some cards that don't
	 * leave these fields cleared when the BIOS is not
	 * installed.
	 */
	if (!(ahc->type & AHC_WIDE))
		ahc->needwdtr_orig = 0;
	ahc->needsdtr = ahc->needsdtr_orig;
	ahc->needwdtr = ahc->needwdtr_orig;
	ahc->sdtrpending = 0;
	ahc->wdtrpending = 0;
	ahc->tagenable = 0;
	
	/*
	 * Clear the control byte for every SCB so that the sequencer
	 * doesn't get confused and think that one of them is valid
	 */
	for (i = 0; i < ahc->maxscbs; i++) {
		outb(SCBPTR + iobase, i);
		outb(SCBARRAY + iobase, 0);
	}

#ifdef AHC_DEBUG
	printf("NEEDSDTR == 0x%x\nNEEDWDTR == 0x%x\n", ahc->needsdtr,
		ahc->needwdtr);
#endif

	/*
	 * Set the number of availible SCBs
	 */
	outb(HA_SCBCOUNT + iobase, ahc->maxscbs);

	/* We don't have any busy targets right now */
	outb(HA_ACTIVE0 + iobase, 0);
	outb(HA_ACTIVE1 + iobase, 0);
	
	/* We don't have any waiting selections */
	outb(WAITING_SCBH + iobase, SCB_LIST_NULL);
	outb(WAITING_SCBT + iobase, SCB_LIST_NULL);
	/*
	 * Load the Sequencer program and Enable the adapter.
	 * Place the aic7770 in fastmode which makes a big
	 * difference when doing many small block transfers.
	 */
	
	printf("%s: Downloading Sequencer Program...", ahc->sc_dev.dv_xname);
	ahc_loadseq(iobase);
	printf("Done\n");
	
	if (!(ahc->type & AHC_AIC7870))
		outb(BCTL + iobase, ENABLE);
	
	/* Reset the bus */
	outb(SCSISEQ + iobase, SCSIRSTO);
	delay(1000);
	outb(SCSISEQ + iobase, 0);
	
	RESTART_SEQUENCER(ahc);
	
	return (0);
}

void
ahcminphys(bp)
	struct buf *bp;
{

	if (bp->b_bcount > ((AHC_NSEG - 1) << PGSHIFT))
		bp->b_bcount = ((AHC_NSEG - 1) << PGSHIFT);
	minphys(bp);
}

/*
 * start a scsi operation given the command and
 * the data address, target, and lun all of which
 * are stored in the scsi_xfer struct
 */
int
ahc_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct ahc_softc *ahc = sc_link->adapter_softc;
	struct ahc_scb *scb;
	struct ahc_dma_seg *sg;
	int seg;            /* scatter gather seg being worked on */
	u_long thiskv, thisphys, nextphys;
	int bytes_this_seg, bytes_this_page, datalen, flags;
	int s;
	u_short	mask = (0x01 << (sc_link->target | (sc_link->quirks & 0x08)));

#ifdef AHC_MORE_DEBUG
	if ((sc_link->target & 0xf) == DEBUGTARGET) {
		printf("ahc ahc_scsi_cmd for %x\n", sc_link->target);
		sc_link->flags = 0xf0;
		SC_DEBUG(sc_link, SDEV_DB2, ("ahc_scsi_cmd\n"));
	}
#endif
	
	/*
	 * get a scb to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if ((flags & (ITSDONE|INUSE)) != INUSE) {
		printf("%s: done or not in use?\n", ahc->sc_dev.dv_xname);
		xs->flags &= ~ITSDONE;
		xs->flags |= INUSE;
	}
	if ((scb = ahc_get_scb(ahc, flags)) == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
	}
	scb->xs = xs;
	
#ifdef AHC_MORE_DEBUG
	if ((sc_link->target & 0xf) == DEBUGTARGET) {
		sc_link->flags = 0xf0;
		SC_DEBUG(sc_link, SDEV_DB3, ("start scb(%x)\n", scb));
	}
#endif
	
	if (flags & SCSI_RESET) {
		/* XXX: Needs Implementation */
		printf("ahc: SCSI_RESET called.\n");
	}
	
	/*
	 * Put all the arguments for the xfer in the scb
	 */
	scb->control = 0;
	if (ahc->tagenable & mask)
		scb->control |= SCB_TE;
	if ((ahc->needwdtr & mask) && !(ahc->wdtrpending & mask)) {
		scb->control |= SCB_NEEDWDTR;
		ahc->wdtrpending |= mask;
	}
	if ((ahc->needsdtr & mask) && !(ahc->sdtrpending & mask)) {
		scb->control |= SCB_NEEDSDTR;
		ahc->sdtrpending |= mask;
	}
	scb->target_channel_lun = ((sc_link->target << 4) & 0xF0) |
	    (sc_link->quirks & 0x08) | (sc_link->lun & 0x07);
	scb->cmdlen = xs->cmdlen;
	scb->cmdpointer = vtophys(xs->cmd);
	
	xs->resid = 0;
	if (xs->datalen) {
		scb->SG_list_pointer = vtophys(scb->ahc_dma);
		sg = scb->ahc_dma;
		seg = 0;
		{
			/*
			 * Set up the scatter gather block
			 */
#ifdef AHC_MORE_DEBUG
			if ((sc_link->target & 0xf) == DEBUGTARGET) {
				sc_link->flags = 0xf0;
				SC_DEBUG(sc_link, SDEV_DB4,
				     ("%ld @%x:- ", xs->datalen, xs->data));
			}
#endif
			datalen = xs->datalen;
			thiskv = (long) xs->data;
			thisphys = vtophys(thiskv);
			
			while (datalen && seg < AHC_NSEG) {
				bytes_this_seg = 0;

				/* put in the base address */
				sg->seg_addr = thisphys;

#ifdef AHC_MORE_DEBUG
				if ((sc_link->target & 0xf) == DEBUGTARGET) {
					sc_link->flags = 0xf0;
					SC_DEBUGN(sc_link, SDEV_DB4, ("0x%lx",
					    thisphys));
				}
#endif

				/* do it at least once */
				nextphys = thisphys;
				while (datalen && thisphys == nextphys) {
					/*
					 * This page is contiguous (physically)
					 * with the the last, just extend the
					 * length
					 */
					/* how far to the end of the page */
					nextphys = (thisphys & ~PGOFSET) + NBPG;
					bytes_this_page = nextphys - thisphys;
					/**** or the data ****/
					bytes_this_page = min(bytes_this_page,
							      datalen);
					bytes_this_seg += bytes_this_page;
					datalen -= bytes_this_page;

					/* get more ready for the next page */
					thiskv = (thiskv & ~PGOFSET) + NBPG;
					if (datalen)
						thisphys = vtophys(thiskv);
				}
				/*
				 * next page isn't contiguous, finish the seg
				 */
#ifdef AHC_MORE_DEBUG
				if ((sc_link->target & 0xf) == DEBUGTARGET) {
					sc_link->flags = 0xf0;
					SC_DEBUGN(sc_link, SDEV_DB4, ("(0x%x)",
					    bytes_this_seg));
				}
#endif
				sg->seg_len = bytes_this_seg;
				sg++;
				seg++;
			}
		}
		/*end of iov/kv decision */
		scb->SG_segment_count = seg;
#ifdef AHC_MORE_DEBUG
		if ((sc_link->target & 0xf) == DEBUGTARGET) {
			sc_link->flags = 0xf0;
			SC_DEBUGN(sc_link, SDEV_DB4, ("\n"));
		}
#endif
		if (datalen) {
			/*
			 * there's still data, must have run out of segs!
			 */
			printf("%s: ahc_scsi_cmd: more than %d dma segs\n",
			    ahc->sc_dev.dv_xname, AHC_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			ahc_free_scb(ahc, scb, flags);
			return (COMPLETE);
		}
	} else {
		scb->SG_list_pointer = (physaddr)0;
		scb->SG_segment_count = 0;
	}

#ifdef AHC_MORE_DEBUG
	if (sc_link->target == DEBUGTARGET)
		ahc_print_scb(scb);
#endif

	s = splbio();

	ahc_send_scb(ahc, scb);

	/*
	 * Usually return SUCCESSFULLY QUEUED
	 */
	if ((flags & SCSI_POLL) == 0) {
		timeout(ahc_timeout, scb, (xs->timeout * hz) / 1000);
		splx(s);
#ifdef AHC_MORE_DEBUG
		if ((sc_link->target & 0xf) == DEBUGTARGET) {
			sc_link->flags = 0xf0;
			SC_DEBUG(sc_link, SDEV_DB3, ("cmd_sent\n"));
		}
#endif
		return (SUCCESSFULLY_QUEUED);
	}
	
	splx(s);
	
	/*
	 * If we can't use interrupts, poll on completion
	 */
#ifdef AHC_MORE_DEBUG
	if ((sc_link->target & 0xf) == DEBUGTARGET) {
		sc_link->flags = 0xf0;
		SC_DEBUG(sc_link, SDEV_DB3, ("cmd_wait\n"));
	}
#endif
	if (ahc_poll(ahc, xs, xs->timeout)) {
		ahc_timeout(scb);
		if (ahc_poll(ahc, xs, 2000))
			ahc_timeout(scb);
	}
	return (COMPLETE);
}


/*
 * A scb (and hence an scb entry on the board is put onto the
 * free list.
 */
void
ahc_free_scb(ahc, scb, flags)
	struct ahc_softc *ahc;
	struct  ahc_scb *scb;
	int flags;
{
	int s;

	s = splbio();

	scb->flags = SCB_FREE;
	TAILQ_INSERT_TAIL(&ahc->free_scb, scb, chain);
#ifdef AHC_DEBUG
	ahc->activescbs--;
#endif

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (scb->chain.tqe_next == 0)
		wakeup(&ahc->free_scb);

	splx(s);
}

/* XXXX check */
static inline void
ahc_init_scb(ahc, scb)
	struct ahc_softc *ahc;
	struct ahc_scb *scb;
{
	int iobase = ahc->sc_iobase;
	u_char scb_index;

	bzero(scb, sizeof(struct ahc_scb));
	scb->position = ahc->numscbs;
	/*
	 * Place in the scbarray
	 * Never is removed.  Position
	 * in ahc->scbarray is the scbarray
	 * position on the board we will
	 * load it into.
	 */
	ahc->scbarray[scb->position] = scb;

	/*
	 * Initialize the host memory location
	 * of this SCB down on the board and
	 * flag that it should be DMA's before
	 * reference.  Also set its psuedo
	 * next pointer (for use in the psuedo
	 * list of SCBs waiting for selection)
	 * to SCB_LIST_NULL.
	 */
	scb->control = SCB_NEEDDMA;
	scb->host_scb = vtophys(scb);
	scb->next_waiting = SCB_LIST_NULL;
	PAUSE_SEQUENCER(ahc);
	scb_index = inb(SCBPTR + iobase);
	outb(SCBPTR + iobase, scb->position);
	outb(SCBCNT + iobase, SCBAUTO);
	outsb(SCBARRAY + iobase, scb, 31);
	outb(SCBCNT + iobase, 0);
	outb(SCBPTR + iobase, scb_index);
	UNPAUSE_SEQUENCER(ahc);
	scb->control = 0;
}

static inline void
ahc_reset_scb(ahc, scb)
	struct ahc_softc *ahc;
	struct ahc_scb *scb;
{

	bzero(scb, SCB_BZERO_SIZE);
}

/*
 * Get a free scb
 *
 * If there are none, see if we can allocate a new one.
 */
struct ahc_scb *
ahc_get_scb(ahc, flags)
	struct ahc_softc *ahc;
	int flags;
{
	struct ahc_scb *scb;
	int s;

	s = splbio();

	/*
	 * If we can and have to, sleep waiting for one to come free
	 * but only if we can't allocate a new one.
	 */
	for (;;) {
		scb = ahc->free_scb.tqh_first;
		if (scb) {
			TAILQ_REMOVE(&ahc->free_scb, scb, chain);
			break;
		}
		if (ahc->numscbs < ahc->maxscbs) {
			if (scb = (struct ahc_scb *) malloc(sizeof(struct ahc_scb),
			    M_TEMP, M_NOWAIT)) {
				ahc_init_scb(ahc, scb);
				ahc->numscbs++;
			} else {
				printf("%s: can't malloc scb\n",
				    ahc->sc_dev.dv_xname);
				goto out;
			}
			break;
		}
		if ((flags & SCSI_NOSLEEP) != 0)
			goto out;
		tsleep(&ahc->free_scb, PRIBIO, "ahcscb", 0);
	}

	ahc_reset_scb(ahc, scb);
	scb->flags = SCB_ACTIVE;
#ifdef AHC_DEBUG
	ahc->activescbs++;
	if (ahc->activescbs == ahc->maxscbs)
		printf("%s: Max SCBs active\n", ahc->sc_dev.dv_xname);
#endif

out:
	splx(s);
	return (scb);
}

/* XXXX check */
void
ahc_loadseq(iobase)
	int iobase;
{
	static u_char seqprog[] = {
#               include "aic7xxx_seq.h"
	};

	outb(SEQCTL + iobase, PERRORDIS|SEQRESET|LOADRAM);
	outsb(SEQRAM + iobase, seqprog, sizeof(seqprog));
	outb(SEQCTL + iobase, FASTMODE|SEQRESET);
}

/*
 * Function to poll for command completion when in poll mode
 */
int
ahc_poll(ahc, xs, count)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
	int count;
{                               /* in msec  */
	int iobase = ahc->sc_iobase;
	int stport = INTSTAT + iobase;

	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (inb(stport) & INT_PEND)
			ahcintr(ahc);
		if (xs->flags & ITSDONE)
			return 0;
		delay(1000);
		count--;
	}
	return 1;
}

/* XXXX check */
void
ahc_abort_scb(ahc, scb)
	struct ahc_softc *ahc;
	struct ahc_scb *scb;
{
	int iobase = ahc->sc_iobase;
	int found = 0;
	int scb_index;
	u_char flags;
	u_char scb_control;

	PAUSE_SEQUENCER(ahc);
	/*
	 * Case 1: In the QINFIFO
	 */
	{
		int saved_queue[AHC_SCB_MAX];
		int i;
		int queued = inb(QINCNT + iobase);

		for (i = 0; i < (queued - found); i++) {
			saved_queue[i] = inb(QINFIFO + iobase);
			if (saved_queue[i] == scb->position) {
				i--;
				found = 1;
			}
		}
		/* Re-insert entries back into the queue */
		for (queued = 0; queued < i; queued++)
			outb(QINFIFO + iobase, saved_queue[queued]);

		if (found)
			goto done;
	}
	
	scb_index = inb(SCBPTR + iobase);
	/*
	 * Case 2: Not the active command
	 */
	if (scb_index != scb->position) {
		/*
		 * Select the SCB we want to abort
		 * and turn off the disconnected bit.
		 * the driver will then abort the command
		 * and notify us of the abort.
		 */
		outb(SCBPTR + iobase, scb->position);
		scb_control = inb(SCBARRAY + iobase);
		scb_control &= ~SCB_DIS;
		outb(SCBARRAY + iobase, scb_control);
		outb(SCBPTR + iobase, scb_index);
		goto done;
	}
	scb_control = inb(SCBARRAY + iobase);
	if (scb_control & SCB_DIS) {
		scb_control &= ~SCB_DIS;
		outb(SCBARRAY + iobase, scb_control);
		goto done;
	}
	/*
	 * Case 3: Currently active command
	 */
	if ((flags = inb(HA_FLAGS + iobase)) & ACTIVE_MSG) {
		/*
		 * If there's a message in progress,
		 * reset the bus and have all devices renegotiate.
		 */
		if (scb->target_channel_lun & 0x08) {
			ahc->needsdtr |= (ahc->needsdtr_orig & 0xff00);
			ahc->sdtrpending &= 0x00ff;
			outb(HA_ACTIVE1, 0);
		} else if (ahc->type & AHC_WIDE) {
			ahc->needsdtr = ahc->needsdtr_orig;
			ahc->needwdtr = ahc->needwdtr_orig;
			ahc->sdtrpending = 0;
			ahc->wdtrpending = 0;
			outb(HA_ACTIVE0, 0);
			outb(HA_ACTIVE1, 0);
		} else {
			ahc->needsdtr |= (ahc->needsdtr_orig & 0x00ff);
			ahc->sdtrpending &= 0xff00;
			outb(HA_ACTIVE0, 0);
		}

		/* Reset the bus */
		outb(SCSISEQ + iobase, SCSIRSTO);
		delay(1000);
		outb(SCSISEQ + iobase, 0);
		goto done;
	}
	
	/*
	 * Otherwise, set up an abort message and have the sequencer
	 * clean up
	 */
	outb(HA_FLAGS + iobase, flags | ACTIVE_MSG);
	outb(HA_MSG_LEN + iobase, 1);
	outb(HA_MSG_START + iobase, MSG_ABORT);
	
	outb(SCSISIGO + iobase, inb(HA_SIGSTATE + iobase) | 0x10);
	
done:
	scb->flags = SCB_ABORTED;
	UNPAUSE_SEQUENCER(ahc);
	ahc_done(ahc, scb);
	return;
}

void
ahc_timeout(arg)
	void *arg;
{
	struct ahc_scb *scb = arg;
	struct scsi_xfer *xs = scb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct ahc_softc *ahc = sc_link->adapter_softc;
	int s;

	sc_print_addr(sc_link);
	printf("timed out");

	s = splbio();

#ifdef SCSIDEBUG
	show_scsi_cmd(scb->xs);
#endif
#ifdef  AHC_DEBUG
	if (ahc_debug & AHC_SHOWSCBS)
		ahc_print_active_scb(ahc);
#endif /*AHC_DEBUG */

	if (scb->flags & SCB_IMMED) {
		printf("\n");
		scb->xs->retries = 0;   /* I MEAN IT ! */
		scb->flags |= SCB_IMMED_FAIL;
		ahc_done(ahc, scb);
		splx(s);
		return;
	}

	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (scb->flags == SCB_ABORTED) {
		/* abort timed out */
		printf(" AGAIN\n");
		scb->xs->retries = 0;	/* I MEAN IT ! */
		ahc_done(ahc, scb);
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		scb->xs->error = XS_TIMEOUT;
		scb->flags = SCB_ABORTED;
		ahc_abort_scb(ahc, scb);
		/* 2 secs for the abort */
		if ((xs->flags & SCSI_POLL) == 0)
			timeout(ahc_timeout, scb, 2 * hz);
	}

	splx(s);
}
