/*	$NetBSD: aha1542.c,v 1.53 1995/10/03 20:58:56 mycroft Exp $	*/

/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Originally written by Julian Elischer (julian@tfs.com)
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
 */

/*
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
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

#include <machine/pio.h>

#include <dev/isa/isavar.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#ifndef DDB
#define Debugger() panic("should call debugger here (aha1542.c)")
#endif /* ! DDB */

/************************** board definitions *******************************/

/*
 * I/O Port Interface
 */
#define	AHA_BASE		aha->sc_iobase
#define	AHA_CTRL_STAT_PORT	(AHA_BASE + 0x0)	/* control & status */
#define	AHA_CMD_DATA_PORT	(AHA_BASE + 0x1)	/* cmds and datas */
#define	AHA_INTR_PORT		(AHA_BASE + 0x2)	/* Intr. stat */

/*
 * AHA_CTRL_STAT bits (write)
 */
#define AHA_HRST		0x80	/* Hardware reset */
#define AHA_SRST		0x40	/* Software reset */
#define AHA_IRST		0x20	/* Interrupt reset */
#define AHA_SCRST		0x10	/* SCSI bus reset */

/*
 * AHA_CTRL_STAT bits (read)
 */
#define AHA_STST		0x80	/* Self test in Progress */
#define AHA_DIAGF		0x40	/* Diagnostic Failure */
#define AHA_INIT		0x20	/* Mbx Init required */
#define AHA_IDLE		0x10	/* Host Adapter Idle */
#define AHA_CDF			0x08	/* cmd/data out port full */
#define AHA_DF			0x04	/* Data in port full */
#define AHA_INVDCMD		0x01	/* Invalid command */

/*
 * AHA_CMD_DATA bits (write)
 */
#define	AHA_NOP			0x00	/* No operation */
#define AHA_MBX_INIT		0x01	/* Mbx initialization */
#define AHA_START_SCSI		0x02	/* start scsi command */
#define AHA_START_BIOS		0x03	/* start bios command */
#define AHA_INQUIRE		0x04	/* Adapter Inquiry */
#define AHA_MBO_INTR_EN		0x05	/* Enable MBO available interrupt */
#define AHA_SEL_TIMEOUT_SET	0x06	/* set selection time-out */
#define AHA_BUS_ON_TIME_SET	0x07	/* set bus-on time */
#define AHA_BUS_OFF_TIME_SET	0x08	/* set bus-off time */
#define AHA_SPEED_SET		0x09	/* set transfer speed */
#define AHA_DEV_GET		0x0a	/* return installed devices */
#define AHA_CONF_GET		0x0b	/* return configuration data */
#define AHA_TARGET_EN		0x0c	/* enable target mode */
#define AHA_SETUP_GET		0x0d	/* return setup data */
#define AHA_WRITE_CH2		0x1a	/* write channel 2 buffer */
#define AHA_READ_CH2		0x1b	/* read channel 2 buffer */
#define AHA_WRITE_FIFO		0x1c	/* write fifo buffer */
#define AHA_READ_FIFO		0x1d	/* read fifo buffer */
#define AHA_ECHO		0x1e	/* Echo command data */
#define AHA_EXT_BIOS		0x28	/* return extended bios info */
#define AHA_MBX_ENABLE		0x29	/* enable mail box interface */

/*
 * AHA_INTR_PORT bits (read)
 */
#define AHA_ANY_INTR		0x80	/* Any interrupt */
#define AHA_SCRD		0x08	/* SCSI reset detected */
#define AHA_HACC		0x04	/* Command complete */
#define AHA_MBOA		0x02	/* MBX out empty */
#define AHA_MBIF		0x01	/* MBX in full */

/*
 * Mail box defs
 */
#define AHA_MBX_SIZE	16	/* mail box size */

#define	AHA_CCB_MAX	32	/* store up to 32 CCBs at one time */
#define	CCB_HASH_SIZE	32	/* hash table size for phystokv */
#define	CCB_HASH_SHIFT	9
#define	CCB_HASH(x)	((((long)(x))>>CCB_HASH_SHIFT) & (CCB_HASH_SIZE - 1))

#define aha_nextmbx(wmb, mbx, mbio) \
	if ((wmb) == &(mbx)->mbio[AHA_MBX_SIZE - 1])	\
		(wmb) = &(mbx)->mbio[0];		\
	else						\
		(wmb)++;

struct aha_mbx_out {
	u_char cmd;
	u_char ccb_addr[3];
};

struct aha_mbx_in {
	u_char stat;
	u_char ccb_addr[3];
};

struct aha_mbx {
	struct aha_mbx_out mbo[AHA_MBX_SIZE];
	struct aha_mbx_in mbi[AHA_MBX_SIZE];
	struct aha_mbx_out *tmbo;	/* Target Mail Box out */
	struct aha_mbx_in *tmbi;	/* Target Mail Box in */
};

/*
 * mbo.cmd values
 */
#define AHA_MBO_FREE	0x0	/* MBO entry is free */
#define AHA_MBO_START	0x1	/* MBO activate entry */
#define AHA_MBO_ABORT	0x2	/* MBO abort entry */

/*
 * mbi.stat values
 */
#define AHA_MBI_FREE	0x0	/* MBI entry is free */
#define AHA_MBI_OK	0x1	/* completed without error */
#define AHA_MBI_ABORT	0x2	/* aborted ccb */
#define AHA_MBI_UNKNOWN	0x3	/* Tried to abort invalid CCB */
#define AHA_MBI_ERROR	0x4	/* Completed with error */

/* FOR OLD VERSIONS OF THE !%$@ this may have to be 16 (yuk) */
#define	AHA_NSEG	17	/* Number of scatter gather segments <= 16 */
				/* allow 64 K i/o (min) */

struct aha_ccb {
	u_char opcode;
	u_char lun:3;
	u_char data_in:1;	/* must be 0 */
	u_char data_out:1;	/* must be 0 */
	u_char target:3;
	u_char scsi_cmd_length;
	u_char req_sense_length;
	u_char data_length[3];
	u_char data_addr[3];
	u_char link_addr[3];
	u_char link_id;
	u_char host_stat;
	u_char target_stat;
	u_char reserved[2];
	struct scsi_generic scsi_cmd;
	struct scsi_sense_data scsi_sense;
	struct aha_scat_gath {
		u_char seg_len[3];
		u_char seg_addr[3];
	} scat_gath[AHA_NSEG];
	/*----------------------------------------------------------------*/
	TAILQ_ENTRY(aha_ccb) chain;
	struct aha_ccb *nexthash;
	long hashkey;
	struct scsi_xfer *xs;		/* the scsi_xfer for this cmd */
	int flags;
#define CCB_FREE	0
#define CCB_ACTIVE	1
#define CCB_ABORTED	2
	struct aha_mbx_out *mbx;	/* pointer to mail box */
};

/*
 * opcode fields
 */
#define AHA_INITIATOR_CCB	0x00	/* SCSI Initiator CCB */
#define AHA_TARGET_CCB		0x01	/* SCSI Target CCB */
#define AHA_INIT_SCAT_GATH_CCB	0x02	/* SCSI Initiator with scatter gather */
#define AHA_RESET_CCB		0x81	/* SCSI Bus reset */

/*
 * aha_ccb.host_stat values
 */
#define AHA_OK		0x00	/* cmd ok */
#define AHA_LINK_OK	0x0a	/* Link cmd ok */
#define AHA_LINK_IT	0x0b	/* Link cmd ok + int */
#define AHA_SEL_TIMEOUT	0x11	/* Selection time out */
#define AHA_OVER_UNDER	0x12	/* Data over/under run */
#define AHA_BUS_FREE	0x13	/* Bus dropped at unexpected time */
#define AHA_INV_BUS	0x14	/* Invalid bus phase/sequence */
#define AHA_BAD_MBO	0x15	/* Incorrect MBO cmd */
#define AHA_BAD_CCB	0x16	/* Incorrect ccb opcode */
#define AHA_BAD_LINK	0x17	/* Not same values of LUN for links */
#define AHA_INV_TARGET	0x18	/* Invalid target direction */
#define AHA_CCB_DUP	0x19	/* Duplicate CCB received */
#define AHA_INV_CCB	0x1a	/* Invalid CCB or segment list */
#define AHA_ABORTED	42

struct aha_setup {
	u_char  sync_neg:1;
	u_char  parity:1;
		u_char:6;
	u_char  speed;
	u_char  bus_on;
	u_char  bus_off;
	u_char  num_mbx;
	u_char  mbx[3];
	struct {
		u_char  offset:4;
		u_char  period:3;
		u_char  valid:1;
	} sync[8];
	u_char  disc_sts;
};

struct aha_config {
	u_char  chan;
	u_char  intr;
	u_char  scsi_dev:3;
		u_char:5;
};

struct aha_inquire {
	u_char	boardid;	/* type of board */
				/* 0x31 = AHA-1540 */
				/* 0x41 = AHA-1540A/1542A/1542B */
				/* 0x42 = AHA-1640 */
				/* 0x43 = AHA-1542C */
				/* 0x44 = AHA-1542CF */
				/* 0x45 = AHA-1542CF, BIOS v2.01 */
	u_char	spec_opts;	/* special options ID */
				/* 0x41 = Board is standard model */
	u_char	revision_1;	/* firmware revision [0-9A-Z] */
	u_char	revision_2;	/* firmware revision [0-9A-Z] */
};

struct aha_extbios {
	u_char	flags;		/* Bit 3 == 1 extended bios enabled */
	u_char	mailboxlock;	/* mail box lock code to unlock it */
};

#define INT9	0x01
#define INT10	0x02
#define INT11	0x04
#define INT12	0x08
#define INT14	0x20
#define INT15	0x40

#define CHAN0	0x01
#define CHAN5	0x20
#define CHAN6	0x40
#define CHAN7	0x80

/*********************************** end of board definitions***************/

#define KVTOPHYS(x)	vtophys(x)

#ifdef	AHADEBUG
int	aha_debug = 1;
#endif /*AHADEBUG */

struct aha_softc {
	struct device sc_dev;
	struct isadev sc_id;
	void *sc_ih;

	int sc_iobase;
	int sc_irq, sc_drq;

	struct aha_mbx aha_mbx;		/* all the mailboxes */
	struct aha_ccb *ccbhash[CCB_HASH_SIZE];
	TAILQ_HEAD(, aha_ccb) free_ccb;
	int numccbs;
	int aha_scsi_dev;		/* our scsi id */
	struct scsi_link sc_link;
};

int aha_cmd();	/* XXX must be varargs to prototype */
int ahaintr __P((void *));
void aha_free_ccb __P((struct aha_softc *, struct aha_ccb *, int));
struct aha_ccb *aha_get_ccb __P((struct aha_softc *, int));
struct aha_ccb *aha_ccb_phys_kv __P((struct aha_softc *, u_long));
struct aha_mbx_out *aha_send_mbo __P((struct aha_softc *, int, struct aha_ccb *));
void aha_done __P((struct aha_softc *, struct aha_ccb *));
int aha_find __P((struct aha_softc *));
void aha_init __P((struct aha_softc *));
void ahaminphys __P((struct buf *));
int aha_scsi_cmd __P((struct scsi_xfer *));
int aha_poll __P((struct aha_softc *, struct scsi_xfer *, int));
int aha_set_bus_speed __P((struct aha_softc *));
int aha_bus_speed_check __P((struct aha_softc *, int));
void aha_timeout __P((void *arg));

struct scsi_adapter aha_switch = {
	aha_scsi_cmd,
	ahaminphys,
	0,
	0,
};

/* the below structure is so we have a default dev struct for out link struct */
struct scsi_device aha_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

int	ahaprobe __P((struct device *, void *, void *));
void	ahaattach __P((struct device *, struct device *, void *));
int	ahaprint __P((void *, char *));

struct cfdriver ahacd = {
	NULL, "aha", ahaprobe, ahaattach, DV_DULL, sizeof(struct aha_softc)
};

#define AHA_RESET_TIMEOUT	2000	/* time to wait for reset (mSec) */

/*
 * aha_cmd(aha,icnt, ocnt,wait, retval, opcode, args)
 * Activate Adapter command
 *    icnt:   number of args (outbound bytes written after opcode)
 *    ocnt:   number of expected returned bytes
 *    wait:   number of seconds to wait for response
 *    retval: buffer where to place returned bytes
 *    opcode: opcode AHA_NOP, AHA_MBX_INIT, AHA_START_SCSI ...
 *    args:   parameters
 *
 * Performs an adapter command through the ports. Not to be confused
 * with a scsi command, which is read in via the dma.  One of the adapter
 * commands tells it to read in a scsi command but that one is done
 * separately.  This is only called during set-up.
 */
int
aha_cmd(aha, icnt, ocnt, wait, retval, opcode, args)
	struct aha_softc *aha;
	int icnt, ocnt, wait;
	u_char *retval;
	unsigned opcode;
	u_char  args;
{
	unsigned *ic = &opcode;
	u_char oc;
	register i;
	int sts;

	/*
	 * multiply the wait argument by a big constant
	 * zero defaults to 1 sec..
	 * all wait loops are in 50uSec cycles
	 */
	if (wait)
		wait *= 20000;
	else
		wait = 20000;
	/*
	 * Wait for the adapter to go idle, unless it's one of
	 * the commands which don't need this
	 */
	if (opcode != AHA_MBX_INIT && opcode != AHA_START_SCSI) {
		i = 20000;	/*do this for upto about a second */
		while (--i) {
			sts = inb(AHA_CTRL_STAT_PORT);
			if (sts & AHA_IDLE)
				break;
			delay(50);
		}
		if (!i) {
			printf("%s: aha_cmd, host not idle(0x%x)\n",
				aha->sc_dev.dv_xname, sts);
			return ENXIO;
		}
	}
	/*
	 * Now that it is idle, if we expect output, preflush the
	 * queue feeding to us.
	 */
	if (ocnt) {
		while ((inb(AHA_CTRL_STAT_PORT)) & AHA_DF)
			inb(AHA_CMD_DATA_PORT);
	}
	/*
	 * Output the command and the number of arguments given
	 * for each byte, first check the port is empty.
	 */
	icnt++;
	/* include the command */
	while (icnt--) {
		sts = inb(AHA_CTRL_STAT_PORT);
		for (i = wait; i; i--) {
			sts = inb(AHA_CTRL_STAT_PORT);
			if (!(sts & AHA_CDF))
				break;
			delay(50);
		}
		if (!i) {
			if (opcode != AHA_INQUIRE)
				printf("%s: aha_cmd, cmd/data port full\n",
				    aha->sc_dev.dv_xname);
			outb(AHA_CTRL_STAT_PORT, AHA_SRST);
			return ENXIO;
		}
		outb(AHA_CMD_DATA_PORT, (u_char) (*ic++));
	}
	/*
	 * If we expect input, loop that many times, each time,
	 * looking for the data register to have valid data
	 */
	while (ocnt--) {
		sts = inb(AHA_CTRL_STAT_PORT);
		for (i = wait; i; i--) {
			sts = inb(AHA_CTRL_STAT_PORT);
			if (sts & AHA_DF)
				break;
			delay(50);
		}
		if (!i) {
			if (opcode != AHA_INQUIRE)
				printf("%s: aha_cmd, cmd/data port empty %d\n",
					aha->sc_dev.dv_xname, ocnt);
			outb(AHA_CTRL_STAT_PORT, AHA_SRST);
			return ENXIO;
		}
		oc = inb(AHA_CMD_DATA_PORT);
		if (retval)
			*retval++ = oc;
	}
	/*
	 * Wait for the board to report a finised instruction
	 */
	i = 20000;
	while (--i) {
		sts = inb(AHA_INTR_PORT);
		if (sts & AHA_HACC)
			break;
		delay(50);
	}
	if (!i) {
		printf("%s: aha_cmd, host not finished(0x%x)\n",
			aha->sc_dev.dv_xname, sts);
		return ENXIO;
	}
	outb(AHA_CTRL_STAT_PORT, AHA_IRST);
	return 0;
}

/*
 * Check if the device can be found at the port given
 * and if so, set it up ready for further work
 * as an argument, takes the isa_device structure from
 * autoconf.c
 */
int
ahaprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct aha_softc *aha = match;
	struct isa_attach_args *ia = aux;

#ifdef NEWCONFIG
	if (ia->ia_iobase == IOBASEUNK)
		return 0;
#endif

	aha->sc_iobase = ia->ia_iobase;

	/*
	 * Try initialise a unit at this location
	 * sets up dma and bus speed, loads aha->sc_irq
	 */
	if (aha_find(aha) != 0)
		return 0;

	if (ia->ia_irq != IRQUNK) {
		if (ia->ia_irq != aha->sc_irq) {
			printf("%s: irq mismatch; kernel configured %d != board configured %d\n",
			    aha->sc_dev.dv_xname, ia->ia_irq, aha->sc_irq);
			return 0;
		}
	} else
		ia->ia_irq = aha->sc_irq;

	if (ia->ia_drq != DRQUNK) {
		if (ia->ia_drq != aha->sc_drq) {
			printf("%s: drq mismatch; kernel configured %d != board configured %d\n",
			    aha->sc_dev.dv_xname, ia->ia_drq, aha->sc_drq);
			return 0;
		}
	} else
		ia->ia_drq = aha->sc_drq;

	ia->ia_msize = 0;
	ia->ia_iosize = 4;
	return 1;
}

int
ahaprint(aux, name)
	void *aux;
	char *name;
{
	if (name != NULL)       
		printf("%s: scsibus ", name);
	return UNCONF;
}

/*
 * Attach all the sub-devices we can find
 */
void
ahaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct aha_softc *aha = (void *)self;

	if (ia->ia_drq != DRQUNK)
		isa_dmacascade(ia->ia_drq);

	aha_init(aha);
	TAILQ_INIT(&aha->free_ccb);

	/*
	 * fill in the prototype scsi_link.
	 */
	aha->sc_link.adapter_softc = aha;
	aha->sc_link.adapter_target = aha->aha_scsi_dev;
	aha->sc_link.adapter = &aha_switch;
	aha->sc_link.device = &aha_dev;
	aha->sc_link.openings = 2;

	printf("\n");

#ifdef NEWCONFIG
	isa_establish(&aha->sc_id, &aha->sc_dev);
#endif
	aha->sc_ih = isa_intr_establish(ia->ia_irq, ISA_IST_EDGE, ISA_IPL_BIO,
	    ahaintr, aha);

	/*
	 * ask the adapter what subunits are present
	 */
	config_found(self, &aha->sc_link, ahaprint);
}

/*
 * Catch an interrupt from the adaptor
 */
int
ahaintr(arg)
	void *arg;
{
	struct aha_softc *aha = arg;
	struct aha_mbx_in *wmbi;
	struct aha_mbx *wmbx;
	struct aha_ccb *ccb;
	u_char stat;
	int i;
	int found = 0;

#ifdef AHADEBUG
	printf("%s: ahaintr ", aha->sc_dev.dv_xname);
#endif /*AHADEBUG */

	/*
	 * First acknowlege the interrupt, Then if it's not telling about
	 * a completed operation just return.
	 */
	stat = inb(AHA_INTR_PORT);
	if ((stat & (AHA_MBOA | AHA_MBIF)) == 0) {
		outb(AHA_CTRL_STAT_PORT, AHA_IRST);
		return -1;	/* XXX */
	}

	/* Mail box out empty? */
	if (stat & AHA_MBOA) {
		/* Disable MBO available interrupt. */
		outb(AHA_CMD_DATA_PORT, AHA_MBO_INTR_EN);
		for (i = 100000; i; i--) {
			if (!(inb(AHA_CTRL_STAT_PORT) & AHA_CDF))
				break;
			delay(10);
		}
		if (!i) {
			printf("%s: ahaintr, cmd/data port full\n",
			    aha->sc_dev.dv_xname);
			outb(AHA_CTRL_STAT_PORT, AHA_SRST);
			return 1;
		}
		outb(AHA_CMD_DATA_PORT, 0x00);	/* Disable */
		wakeup(&aha->aha_mbx);
	}

	/* Mail box in full? */
	if ((stat & AHA_MBIF) == 0)
		return 1;
	wmbx = &aha->aha_mbx;
	wmbi = wmbx->tmbi;
AGAIN:
	while (wmbi->stat != AHA_MBI_FREE) {
		ccb = aha_ccb_phys_kv(aha, _3btol(wmbi->ccb_addr));
		if (!ccb) {
			wmbi->stat = AHA_MBI_FREE;
			printf("%s: BAD CCB ADDR!\n", aha->sc_dev.dv_xname);
			continue;
		}
		found++;
		switch (wmbi->stat) {
		case AHA_MBI_OK:
		case AHA_MBI_ERROR:
			break;

		case AHA_MBI_ABORT:
			ccb->host_stat = AHA_ABORTED;
			break;

		case AHA_MBI_UNKNOWN:
			ccb = 0;
			break;

		default:
			panic("Impossible mbxi status");
		}
#ifdef AHADEBUG
		if (aha_debug && ccb) {
			u_char *cp = &ccb->scsi_cmd;
			printf("op=%x %x %x %x %x %x\n",
			    cp[0], cp[1], cp[2],
			    cp[3], cp[4], cp[5]);
			printf("stat %x for mbi addr = 0x%08x, ",
			    wmbi->stat, wmbi);
			printf("ccb addr = 0x%x\n", ccb);
		}
#endif /* AHADEBUG */
		wmbi->stat = AHA_MBI_FREE;
		if (ccb) {
			untimeout(aha_timeout, ccb);
			aha_done(aha, ccb);
		}
		aha_nextmbx(wmbi, wmbx, mbi);
	}
	if (!found) {
		for (i = 0; i < AHA_MBX_SIZE; i++) {
			if (wmbi->stat != AHA_MBI_FREE) {
				found++;
				break;
			}
			aha_nextmbx(wmbi, wmbx, mbi);
		}
		if (!found) {
#if 0
			printf("%s: mbi interrupt with no full mailboxes\n",
			    aha->sc_dev.dv_xname);
#endif
		} else {
			found = 0;
			goto AGAIN;
		}
	}
	wmbx->tmbi = wmbi;
	outb(AHA_CTRL_STAT_PORT, AHA_IRST);
	return 1;
}

/*
 * A ccb (and hence a mbx-out is put onto the
 * free list.
 */
void
aha_free_ccb(aha, ccb, flags)
	struct aha_softc *aha;
	struct aha_ccb *ccb;
	int flags;
{
	int s;

	s = splbio();

	ccb->flags = CCB_FREE;
	TAILQ_INSERT_HEAD(&aha->free_ccb, ccb, chain);

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (ccb->chain.tqe_next == 0)
		wakeup(&aha->free_ccb);

	splx(s);
}

static inline void
aha_init_ccb(aha, ccb)
	struct aha_softc *aha;
	struct aha_ccb *ccb;
{
	int hashnum;

	bzero(ccb, sizeof(struct aha_ccb));
	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	ccb->hashkey = KVTOPHYS(ccb);
	hashnum = CCB_HASH(ccb->hashkey);
	ccb->nexthash = aha->ccbhash[hashnum];
	aha->ccbhash[hashnum] = ccb;
}

static inline void
aha_reset_ccb(aha, ccb)
	struct aha_softc *aha;
	struct aha_ccb *ccb;
{

}

/*
 * Get a free ccb
 */
struct aha_ccb *
aha_get_ccb(aha, flags)
	struct aha_softc *aha;
	int flags;
{
	struct aha_ccb *ccb;
	int s;

	s = splbio();

	/*
	 * If we can and have to, sleep waiting for one
	 * to come free
	 */
	for (;;) {
		ccb = aha->free_ccb.tqh_first;
		if (ccb) {
			TAILQ_REMOVE(&aha->free_ccb, ccb, chain);
			break;
		}
		if (aha->numccbs < AHA_CCB_MAX) {
			if (ccb = (struct aha_ccb *) malloc(sizeof(struct aha_ccb),
			    M_TEMP, M_NOWAIT)) {
				aha_init_ccb(aha, ccb);
				aha->numccbs++;
			} else {
				printf("%s: can't malloc ccb\n",
				    aha->sc_dev.dv_xname);
				goto out;
			}
			break;
		}
		if ((flags & SCSI_NOSLEEP) != 0)
			goto out;
		tsleep(&aha->free_ccb, PRIBIO, "ahaccb", 0);
	}

	aha_reset_ccb(aha, ccb);
	ccb->flags = CCB_ACTIVE;

out:
	splx(s);
	return (ccb);
}

/*
 * given a physical address, find the ccb that it corresponds to.
 */
struct aha_ccb *
aha_ccb_phys_kv(aha, ccb_phys)
	struct aha_softc *aha;
	u_long ccb_phys;
{
	int hashnum = CCB_HASH(ccb_phys);
	struct aha_ccb *ccb = aha->ccbhash[hashnum];

	while (ccb) {
		if (ccb->hashkey == ccb_phys)
			break;
		ccb = ccb->nexthash;
	}
	return ccb;
}

/*
 * Get a mbo and send the ccb.
 */
struct aha_mbx_out *
aha_send_mbo(aha, cmd, ccb)
	struct aha_softc *aha;
	int cmd;
	struct aha_ccb *ccb;
{
	struct aha_mbx_out *wmbo;	/* Mail Box Out pointer */
	struct aha_mbx *wmbx;		/* Mail Box pointer specified unit */
	int i;

	/* Get the target out mail box pointer and increment. */
	wmbx = &aha->aha_mbx;
	wmbo = wmbx->tmbo;
	aha_nextmbx(wmbx->tmbo, wmbx, mbo);

	/*
	 * Check the outmail box is free or not.
	 * Note: Under the normal operation, it shuld NOT happen to wait.
	 */
	while (wmbo->cmd != AHA_MBO_FREE) {
		/* Enable mbo available interrupt. */
		outb(AHA_CMD_DATA_PORT, AHA_MBO_INTR_EN);
		for (i = 100000; i; i--) {
			if (!(inb(AHA_CTRL_STAT_PORT) & AHA_CDF))
				break;
			delay(10);
		}
		if (!i) {
			printf("%s: aha_send_mbo, cmd/data port full\n",
			    aha->sc_dev.dv_xname);
			outb(AHA_CTRL_STAT_PORT, AHA_SRST);
			return NULL;
		}
		outb(AHA_CMD_DATA_PORT, 0x01);	/* Enable */
		tsleep(wmbx, PRIBIO, "ahasnd", 0);/*XXX can't do this */
	}

	/* Link ccb to mbo. */
	lto3b(KVTOPHYS(ccb), wmbo->ccb_addr);
	ccb->mbx = wmbo;
	wmbo->cmd = cmd;

	/* Sent it! */
	outb(AHA_CMD_DATA_PORT, AHA_START_SCSI);

	return wmbo;
}

/*
 * We have a ccb which has been processed by the
 * adaptor, now we look to see how the operation
 * went. Wake up the owner if waiting
 */
void
aha_done(aha, ccb)
	struct aha_softc *aha;
	struct aha_ccb *ccb;
{
	struct scsi_sense_data *s1, *s2;
	struct scsi_xfer *xs = ccb->xs;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("aha_done\n"));
	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
	if ((xs->flags & INUSE) == 0) {
		printf("%s: exiting but not in use!\n", aha->sc_dev.dv_xname);
		Debugger();
	}
	if (xs->error == XS_NOERROR) {
		if (ccb->host_stat != AHA_OK) {
			switch (ccb->host_stat) {
			case AHA_ABORTED:
				xs->error = XS_DRIVER_STUFFUP;
				break;
			case AHA_SEL_TIMEOUT:	/* No response */
				xs->error = XS_SELTIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				printf("%s: host_stat %x\n",
				    aha->sc_dev.dv_xname, ccb->host_stat);
				xs->error = XS_DRIVER_STUFFUP;
			}
		} else if (ccb->target_stat != SCSI_OK) {
			switch (ccb->target_stat) {
			case SCSI_CHECK:
				s1 = (struct scsi_sense_data *) (((char *) (&ccb->scsi_cmd)) +
				    ccb->scsi_cmd_length);
				s2 = &xs->sense;
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case SCSI_BUSY:
				xs->error = XS_BUSY;
				break;
			default:
				printf("%s: target_stat %x\n",
				    aha->sc_dev.dv_xname, ccb->target_stat);
				xs->error = XS_DRIVER_STUFFUP;
			}
		} else
			xs->resid = 0;
	}
	xs->flags |= ITSDONE;
	aha_free_ccb(aha, ccb, xs->flags);
	scsi_done(xs);
}

/*
 * Find the board and find its irq/drq
 */
int
aha_find(aha)
	struct aha_softc *aha;
{
	u_char ad[3];
	volatile int i, sts;
	struct aha_config conf;
	struct aha_inquire inquire;
	struct aha_extbios extbios;

	/*
	 * reset board, If it doesn't respond, assume
	 * that it's not there.. good for the probe
	 */

	outb(AHA_CTRL_STAT_PORT, AHA_HRST | AHA_SRST);

	for (i = AHA_RESET_TIMEOUT; i; i--) {
		sts = inb(AHA_CTRL_STAT_PORT);
		if (sts == (AHA_IDLE | AHA_INIT))
			break;
		delay(1000);	/* calibrated in msec */
	}
	if (!i) {
#ifdef	AHADEBUG
		if (aha_debug)
			printf("aha_find: No answer from adaptec board\n");
#endif /*AHADEBUG */
		return ENXIO;
	}

	/*
	 * Assume we have a board at this stage, do an adapter inquire
	 * to find out what type of controller it is.  If the command
	 * fails, we assume it's either a crusty board or an old 1542
	 * clone, and skip the board-specific stuff.
	 */
	if (aha_cmd(aha, 0, sizeof(inquire), 1, &inquire, AHA_INQUIRE)) {
		/*
		 * aha_cmd() already started the reset.  It's not clear we
		 * even need to bother here.
		 */
		for (i = AHA_RESET_TIMEOUT; i; i--) {
			sts = inb(AHA_CTRL_STAT_PORT);
			if (sts == (AHA_IDLE | AHA_INIT))
				break;
			delay(1000);
		}
		if (!i) {
#ifdef AHADEBUG
			printf("aha_init: soft reset failed\n");
#endif /* AHADEBUG */
			return ENXIO;
		}
#ifdef AHADEBUG
		printf("aha_init: inquire command failed\n");
#endif /* AHADEBUG */
		goto noinquire;
	}
#ifdef AHADEBUG
	printf("%s: inquire %x, %x, %x, %x\n",
		aha->sc_dev.dv_xname,
		inquire.boardid, inquire.spec_opts,
		inquire.revision_1, inquire.revision_2);
#endif	/* AHADEBUG */
	/*
	 * If we are a 1542C or 1542CF disable the extended bios so that the
	 * mailbox interface is unlocked.
	 * No need to check the extended bios flags as some of the
	 * extensions that cause us problems are not flagged in that byte.
	 */
	if (inquire.boardid == 0x43 || inquire.boardid == 0x44 ||
	    inquire.boardid == 0x45) {
		aha_cmd(aha, 0, sizeof(extbios), 0, &extbios, AHA_EXT_BIOS);
#ifdef	AHADEBUG
		printf("%s: extended bios flags %x\n", aha->sc_dev.dv_xname,
			extbios.flags);
#endif	/* AHADEBUG */
		printf("%s: 1542C/CF detected, unlocking mailbox\n",
			aha->sc_dev.dv_xname);
		aha_cmd(aha, 2, 0, 0, 0, AHA_MBX_ENABLE,
			0, extbios.mailboxlock);
	}
noinquire:

	/*
	 * setup dma channel from jumpers and save int
	 * level
	 */
	delay(1000);		/* for Bustek 545 */
	aha_cmd(aha, 0, sizeof(conf), 0, &conf, AHA_CONF_GET);
	switch (conf.chan) {
	case CHAN0:
		aha->sc_drq = 0;
		break;
	case CHAN5:
		aha->sc_drq = 5;
		break;
	case CHAN6:
		aha->sc_drq = 6;
		break;
	case CHAN7:
		aha->sc_drq = 7;
		break;
	default:
		printf("illegal dma setting %x\n", conf.chan);
		return EIO;
	}

	switch (conf.intr) {
	case INT9:
		aha->sc_irq = 9;
		break;
	case INT10:
		aha->sc_irq = 10;
		break;
	case INT11:
		aha->sc_irq = 11;
		break;
	case INT12:
		aha->sc_irq = 12;
		break;
	case INT14:
		aha->sc_irq = 14;
		break;
	case INT15:
		aha->sc_irq = 15;
		break;
	default:
		printf("illegal int setting %x\n", conf.intr);
		return EIO;
	}

	/* who are we on the scsi bus? */
	aha->aha_scsi_dev = conf.scsi_dev;

	/*
	 * Change the bus on/off times to not clash with other dma users.
	 */
	aha_cmd(aha, 1, 0, 0, 0, AHA_BUS_ON_TIME_SET, 7);
	aha_cmd(aha, 1, 0, 0, 0, AHA_BUS_OFF_TIME_SET, 4);

#ifdef TUNE_1542
#error XXX Must deal with configuring the DRQ channel if we do this.
	/*
	 * Initialize memory transfer speed
	 * Not compiled in by default because it breaks some machines
	 */
	if (!aha_set_bus_speed(aha))
		return EIO;
#endif /* TUNE_1542 */

	return 0;
}

/*
 * Start the board, ready for normal operation
 */
void
aha_init(aha)
	struct aha_softc *aha;
{
	u_char ad[3];
	int i;

	/*
	 * Initialize mail box
	 */
	lto3b(KVTOPHYS(&aha->aha_mbx), ad);

	aha_cmd(aha, 4, 0, 0, 0, AHA_MBX_INIT, AHA_MBX_SIZE,
	    ad[0], ad[1], ad[2]);

	for (i = 0; i < AHA_MBX_SIZE; i++) {
		aha->aha_mbx.mbo[i].cmd = AHA_MBO_FREE;
		aha->aha_mbx.mbi[i].stat = AHA_MBO_FREE;
	}

	/*
	 * Set up initial mail box for round-robin operation.
	 */
	aha->aha_mbx.tmbo = &aha->aha_mbx.mbo[0];
	aha->aha_mbx.tmbi = &aha->aha_mbx.mbi[0];
}

void
ahaminphys(bp)
	struct buf *bp;
{

	if (bp->b_bcount > ((AHA_NSEG - 1) << PGSHIFT))
		bp->b_bcount = ((AHA_NSEG - 1) << PGSHIFT);
	minphys(bp);
}

/*
 * start a scsi operation given the command and the data address. Also needs
 * the unit, target and lu.
 */
int
aha_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct aha_softc *aha = sc_link->adapter_softc;
	struct aha_ccb *ccb;
	struct aha_scat_gath *sg;
	int seg;		/* scatter gather seg being worked on */
	int thiskv;
	u_long thisphys, nextphys;
	int bytes_this_seg, bytes_this_page, datalen, flags;
	struct iovec *iovp;
	struct aha_mbx_out *mbo;
	int s;

	SC_DEBUG(sc_link, SDEV_DB2, ("aha_scsi_cmd\n"));
	/*
	 * get a ccb to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if ((flags & (ITSDONE|INUSE)) != INUSE) {
		printf("%s: done or not in use?\n", aha->sc_dev.dv_xname);
		xs->flags &= ~ITSDONE;
		xs->flags |= INUSE;
	}
	if ((ccb = aha_get_ccb(aha, flags)) == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return TRY_AGAIN_LATER;
	}
	ccb->xs = xs;

	/*
	 * Put all the arguments for the xfer in the ccb
	 */
	if (flags & SCSI_RESET) {
		ccb->opcode = AHA_RESET_CCB;
	} else {
		/* can't use S/G if zero length */
		ccb->opcode = (xs->datalen ? AHA_INIT_SCAT_GATH_CCB
					   : AHA_INITIATOR_CCB);
	}
	ccb->data_out = 0;
	ccb->data_in = 0;
	ccb->target = sc_link->target;
	ccb->lun = sc_link->lun;
	ccb->scsi_cmd_length = xs->cmdlen;
	ccb->req_sense_length = sizeof(ccb->scsi_sense);
	ccb->host_stat = 0x00;
	ccb->target_stat = 0x00;

	if (xs->datalen && (flags & SCSI_RESET) == 0) {
		lto3b(KVTOPHYS(ccb->scat_gath), ccb->data_addr);
		sg = ccb->scat_gath;
		seg = 0;
#ifdef	TFS
		if (flags & SCSI_DATA_UIO) {
			iovp = ((struct uio *)xs->data)->uio_iov;
			datalen = ((struct uio *)xs->data)->uio_iovcnt;
			xs->datalen = 0;
			while (datalen && seg < AHA_NSEG) {
				lto3b(iovp->iov_base, sg->seg_addr);
				lto3b(iovp->iov_len, sg->seg_len);
				xs->datalen += iovp->iov_len;
				SC_DEBUGN(sc_link, SDEV_DB4, ("UIO(0x%x@0x%x)",
				    iovp->iov_len, iovp->iov_base));
				sg++;
				iovp++;
				seg++;
				datalen--;
			}
		} else
#endif /*TFS_ONLY */
		{
			/*
			 * Set up the scatter gather block
			 */

			SC_DEBUG(sc_link, SDEV_DB4,
				("%d @0x%x:- ", xs->datalen, xs->data));
			datalen = xs->datalen;
			thiskv = (int) xs->data;
			thisphys = KVTOPHYS(thiskv);

			while (datalen && seg < AHA_NSEG) {
				bytes_this_seg = 0;

				/* put in the base address */
				lto3b(thisphys, sg->seg_addr);

				SC_DEBUGN(sc_link, SDEV_DB4, ("0x%x", thisphys));

				/* do it at least once */
				nextphys = thisphys;
				while (datalen && thisphys == nextphys) {
					/*
					 * This page is contiguous (physically)
					 * with the the last, just extend the
					 * length
					 */
					/* check it fits on the ISA bus */
					if (thisphys > 0xFFFFFF) {
						printf("%s: DMA beyond"
							" end of ISA\n",
							aha->sc_dev.dv_xname);
						xs->error = XS_DRIVER_STUFFUP;
						aha_free_ccb(aha, ccb, flags);
						return COMPLETE;
					}
					/** how far to the end of the page ***/
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
						thisphys = KVTOPHYS(thiskv);
				}
				/*
				 * next page isn't contiguous, finish the seg
				 */
				SC_DEBUGN(sc_link, SDEV_DB4,
				    ("(0x%x)", bytes_this_seg));
				lto3b(bytes_this_seg, sg->seg_len);
				sg++;
				seg++;
			}
		}
		lto3b(seg * sizeof(struct aha_scat_gath), ccb->data_length);
		SC_DEBUGN(sc_link, SDEV_DB4, ("\n"));
		if (datalen) {
			/*
			 * there's still data, must have run out of segs!
			 */
			printf("%s: aha_scsi_cmd, more than %d dma segs\n",
			    aha->sc_dev.dv_xname, AHA_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			aha_free_ccb(aha, ccb, flags);
			return COMPLETE;
		}
	} else {		/* No data xfer, use non S/G values */
		lto3b(0, ccb->data_addr);
		lto3b(0, ccb->data_length);
	}
	ccb->link_id = 0;
	lto3b(0, ccb->link_addr);

	/*
	 * Put the scsi command in the ccb and start it
	 */
	if ((flags & SCSI_RESET) == 0)
		bcopy(xs->cmd, &ccb->scsi_cmd, ccb->scsi_cmd_length);

	s = splbio();

	if (aha_send_mbo(aha, AHA_MBO_START, ccb) == NULL) {
		splx(s);
		xs->error = XS_DRIVER_STUFFUP;
		aha_free_ccb(aha, ccb, flags);
		return TRY_AGAIN_LATER;
	}

	/*
	 * Usually return SUCCESSFULLY QUEUED
	 */
	SC_DEBUG(sc_link, SDEV_DB3, ("cmd_sent\n"));
	if ((flags & SCSI_POLL) == 0) {
		timeout(aha_timeout, ccb, (xs->timeout * hz) / 1000);
		splx(s);
		return SUCCESSFULLY_QUEUED;
	}

	splx(s);

	/*
	 * If we can't use interrupts, poll on completion
	 */
	if (aha_poll(aha, xs, xs->timeout)) {
		aha_timeout(ccb);
		if (aha_poll(aha, xs, 2000))
			aha_timeout(ccb);
	}
	return COMPLETE;
}

/*
 * Poll a particular unit, looking for a particular xs
 */
int
aha_poll(aha, xs, count)
	struct aha_softc *aha;
	struct scsi_xfer *xs;
	int count;
{

	/* timeouts are in msec, so we loop in 1000 usec cycles */
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (inb(AHA_INTR_PORT) & AHA_ANY_INTR)
			ahaintr(aha);
		if (xs->flags & ITSDONE)
			return 0;
		delay(1000);	/* only happens in boot so ok */
		count--;
	}
	return 1;
}

#ifdef TUNE_1542
/*
 * Try all the speeds from slowest to fastest.. if it finds a
 * speed that fails, back off one notch from the last working
 * speed (unless there is no other notch).
 * Returns the nSEC value of the time used
 * or 0 if it could get a working speed (or the NEXT speed
 * failed)
 */
static struct bus_speed {
	u_char arg;
	int nsecs;
} aha_bus_speeds[] = {
	{0x88, 100},
	{0x99, 150},
	{0xaa, 200},
	{0xbb, 250},
	{0xcc, 300},
	{0xdd, 350},
	{0xee, 400},
	{0xff, 450}
};

int
aha_set_bus_speed(aha)
	struct aha_softc *aha;
{
	int speed;
	int lastworking;

	lastworking = -1;
	for (speed = 7; speed >= 0; speed--) {
		if (!aha_bus_speed_check(aha, speed))
			break;
		lastworking = speed;
	}
	if (lastworking == -1) {
		printf("no working bus speed\n");
		return 0;
	}
	printf("%d nsec ", aha_bus_speeds[lastworking].nsecs);
	if (lastworking == 7)	/* is slowest already */
		printf("marginal\n");
	else {
		lastworking++;
		printf("ok, using %d nsec\n",
		    aha_bus_speeds[lastworking].nsecs);
	}
	if (!aha_bus_speed_check(aha, lastworking)) {
		printf("test retry failed.. aborting.\n");
		return 0;
	}
	return 1;
}

/*
 * Set the DMA speed to the Nth speed and try an xfer. If it
 * fails return 0, if it succeeds return the nSec value selected
 * If there is no such speed return COMPLETE.
 */
char aha_scratch_buf[256];
char aha_test_string[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz!@";

int
aha_bus_speed_check(aha, speed)
	struct aha_softc *aha;
	int speed;
{
	int numspeeds = sizeof(aha_bus_speeds) / sizeof(struct bus_speed);
	int loopcount;
	u_char ad[3];

	/*
	 * Set the dma-speed
	 */
	aha_cmd(aha, 1, 0, 0, 0, AHA_SPEED_SET, aha_bus_speeds[speed].arg);

	/*
	 * put the test data into the buffer and calculate
	 * it's address. Read it onto the board
	 */
	for (loopcount = 100; loopcount; loopcount--) {
		lto3b(KVTOPHYS(aha_test_string), ad);
		aha_cmd(aha, 3, 0, 0, 0, AHA_WRITE_FIFO, ad[0], ad[1], ad[2]);

		/*
		 * Clear the buffer then copy the contents back from the
		 * board.
		 */
		bzero(aha_scratch_buf, 54);

		lto3b(KVTOPHYS(aha_scratch_buf), ad);
		aha_cmd(aha, 3, 0, 0, 0, AHA_READ_FIFO, ad[0], ad[1], ad[2]);

		/*
		 * Compare the original data and the final data and return the
		 * correct value depending upon the result.  We only check the
		 * first 54 bytes, because that's all the board copies during
		 * WRITE_FIFO and READ_FIFO.
		 */
		if (bcmp(aha_test_string, aha_scratch_buf, 54))
			return 0; /* failed test */
	}

	/* copy succeeded; assume speed ok */
	return 1;
}
#endif /* TUNE_1542 */

void
aha_timeout(arg)
	void *arg;
{
	struct aha_ccb *ccb = arg;
	struct scsi_xfer *xs = ccb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct aha_softc *aha = sc_link->adapter_softc;
	int s;

	sc_print_addr(sc_link);
	printf("timed out");

	s = splbio();

	/*
	 * If The ccb's mbx is not free, then the board has gone south?
	 */
	if (aha_ccb_phys_kv(aha, _3btol(ccb->mbx->ccb_addr)) == ccb &&
	    ccb->mbx->cmd != AHA_MBO_FREE) {
		printf("%s: not taking commands!\n", aha->sc_dev.dv_xname);
		Debugger();
	}

	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (ccb->flags == CCB_ABORTED) {
		/* abort timed out */
		printf(" AGAIN\n");
		ccb->xs->retries = 0;
		aha_done(aha, ccb);
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		ccb->xs->error = XS_TIMEOUT;
		ccb->flags = CCB_ABORTED;
		aha_send_mbo(aha, AHA_MBO_ABORT, ccb);
		/* 2 secs for the abort */
		if ((xs->flags & SCSI_POLL) == 0)
			timeout(aha_timeout, ccb, 2 * hz);
	}

	splx(s);
}
