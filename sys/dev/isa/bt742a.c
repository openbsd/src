/*	$NetBSD: bt742a.c,v 1.49 1995/10/03 20:58:58 mycroft Exp $	*/

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
 * bt742a SCSI driver
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
#include <dev/isa/isadmavar.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#ifndef DDB
#define Debugger() panic("should call debugger here (bt742a.c)")
#endif /* ! DDB */

typedef u_long physaddr;
typedef u_long physlen;

/*
 * I/O Port Interface
 */
#define	BT_BASE			bt->sc_iobase
#define	BT_CTRL_STAT_PORT	(BT_BASE + 0x0)		/* control & status */
#define	BT_CMD_DATA_PORT	(BT_BASE + 0x1)		/* cmds and datas */
#define	BT_INTR_PORT		(BT_BASE + 0x2)		/* Intr. stat */

/*
 * BT_CTRL_STAT bits (write)
 */
#define BT_HRST		0x80	/* Hardware reset */
#define BT_SRST		0x40	/* Software reset */
#define BT_IRST		0x20	/* Interrupt reset */
#define BT_SCRST	0x10	/* SCSI bus reset */

/*
 * BT_CTRL_STAT bits (read)
 */
#define BT_STST		0x80	/* Self test in Progress */
#define BT_DIAGF	0x40	/* Diagnostic Failure */
#define BT_INIT		0x20	/* Mbx Init required */
#define BT_IDLE		0x10	/* Host Adapter Idle */
#define BT_CDF		0x08	/* cmd/data out port full */
#define BT_DF		0x04	/* Data in port full */
#define BT_INVDCMD	0x01	/* Invalid command */

/*
 * BT_CMD_DATA bits (write)
 */
#define	BT_NOP			0x00	/* No operation */
#define BT_MBX_INIT		0x01	/* Mbx initialization */
#define BT_START_SCSI		0x02	/* start scsi command */
#define BT_START_BIOS		0x03	/* start bios command */
#define BT_INQUIRE		0x04	/* Adapter Inquiry */
#define BT_MBO_INTR_EN		0x05	/* Enable MBO available interrupt */
#define BT_SEL_TIMEOUT_SET	0x06	/* set selection time-out */
#define BT_BUS_ON_TIME_SET	0x07	/* set bus-on time */
#define BT_BUS_OFF_TIME_SET	0x08	/* set bus-off time */
#define BT_SPEED_SET		0x09	/* set transfer speed */
#define BT_DEV_GET		0x0a	/* return installed devices */
#define BT_CONF_GET		0x0b	/* return configuration data */
#define BT_TARGET_EN		0x0c	/* enable target mode */
#define BT_SETUP_GET		0x0d	/* return setup data */
#define BT_WRITE_CH2		0x1a	/* write channel 2 buffer */
#define BT_READ_CH2		0x1b	/* read channel 2 buffer */
#define BT_WRITE_FIFO		0x1c	/* write fifo buffer */
#define BT_READ_FIFO		0x1d	/* read fifo buffer */
#define BT_ECHO			0x1e	/* Echo command data */
#define BT_MBX_INIT_EXTENDED	0x81	/* Mbx initialization */
#define BT_INQUIRE_EXTENDED	0x8D	/* Adapter Setup Inquiry */

/* Follows command appeared at FirmWare 3.31 */
#define	BT_ROUND_ROBIN	0x8f	/* Enable/Disable(default) round robin */
#define   BT_DISABLE		0x00	/* Parameter value for Disable */
#define   BT_ENABLE		0x01	/* Parameter value for Enable */

/*
 * BT_INTR_PORT bits (read)
 */
#define BT_ANY_INTR	0x80	/* Any interrupt */
#define BT_SCRD		0x08	/* SCSI reset detected */
#define BT_HACC		0x04	/* Command complete */
#define BT_MBOA		0x02	/* MBX out empty */
#define BT_MBIF		0x01	/* MBX in full */

/*
 * Mail box defs  etc.
 * these could be bigger but we need the bt_softc to fit on a single page..
 */
#define BT_MBX_SIZE	32	/* mail box size  (MAX 255 MBxs) */
				/* don't need that many really */
#define BT_CCB_MAX	32	/* store up to 32 CCBs at one time */
#define	CCB_HASH_SIZE	32	/* hash table size for phystokv */
#define	CCB_HASH_SHIFT	9
#define CCB_HASH(x)	((((long)(x))>>CCB_HASH_SHIFT) & (CCB_HASH_SIZE - 1))

#define bt_nextmbx(wmb, mbx, mbio) \
	if ((wmb) == &(mbx)->mbio[BT_MBX_SIZE - 1])	\
		(wmb) = &(mbx)->mbio[0];		\
	else						\
		(wmb)++;

struct bt_mbx_out {
	physaddr ccb_addr;
	u_char dummy[3];
	u_char cmd;
};

struct bt_mbx_in {
	physaddr ccb_addr;
	u_char btstat;
	u_char sdstat;
	u_char dummy;
	u_char stat;
};

struct bt_mbx {
	struct bt_mbx_out mbo[BT_MBX_SIZE];
	struct bt_mbx_in mbi[BT_MBX_SIZE];
	struct bt_mbx_out *tmbo;	/* Target Mail Box out */
	struct bt_mbx_in *tmbi;		/* Target Mail Box in */
};

/*
 * mbo.cmd values
 */
#define BT_MBO_FREE	0x0	/* MBO entry is free */
#define BT_MBO_START	0x1	/* MBO activate entry */
#define BT_MBO_ABORT	0x2	/* MBO abort entry */

/*
 * mbi.stat values
 */
#define BT_MBI_FREE	0x0	/* MBI entry is free */
#define BT_MBI_OK	0x1	/* completed without error */
#define BT_MBI_ABORT	0x2	/* aborted ccb */
#define BT_MBI_UNKNOWN	0x3	/* Tried to abort invalid CCB */
#define BT_MBI_ERROR	0x4	/* Completed with error */

#if	defined(BIG_DMA)
WARNING...THIS WON'T WORK(won't fit on 1 page)
/* #define      BT_NSEG 2048    /* Number of scatter gather segments - to much vm */
#define	BT_NSEG	128
#else
#define	BT_NSEG	33
#endif /* BIG_DMA */

struct bt_scat_gath {
	physlen seg_len;
	physaddr seg_addr;
};

struct bt_ccb {
	u_char opcode;
	u_char:3, data_in:1, data_out:1,:3;
	u_char scsi_cmd_length;
	u_char req_sense_length;
	/*------------------------------------longword boundary */
	physlen data_length;
	/*------------------------------------longword boundary */
	physaddr data_addr;
	/*------------------------------------longword boundary */
	u_char dummy1[2];
	u_char host_stat;
	u_char target_stat;
	/*------------------------------------longword boundary */
	u_char target;
	u_char lun;
	struct scsi_generic scsi_cmd;
	u_char dummy2[1];
	u_char link_id;
	/*------------------------------------longword boundary */
	physaddr link_addr;
	/*------------------------------------longword boundary */
	physaddr sense_ptr;
/*-----end of HW fields-----------------------longword boundary */
	struct scsi_sense_data scsi_sense;
	/*------------------------------------longword boundary */
	struct bt_scat_gath scat_gath[BT_NSEG];
	/*------------------------------------longword boundary */
	TAILQ_ENTRY(bt_ccb) chain;
	struct bt_ccb *nexthash;
	long hashkey;
	struct scsi_xfer *xs;		/* the scsi_xfer for this cmd */
	int flags;
#define	CCB_FREE	0
#define CCB_ACTIVE	1
#define	CCB_ABORTED	2
	struct bt_mbx_out *mbx;		/* pointer to mail box */
};

/*
 * opcode fields
 */
#define BT_INITIATOR_CCB	0x00	/* SCSI Initiator CCB */
#define BT_TARGET_CCB		0x01	/* SCSI Target CCB */
#define BT_INIT_SCAT_GATH_CCB	0x02	/* SCSI Initiator with scattter gather */
#define BT_RESET_CCB		0x81	/* SCSI Bus reset */

/*
 * bt_ccb.host_stat values
 */
#define BT_OK		0x00	/* cmd ok */
#define BT_LINK_OK	0x0a	/* Link cmd ok */
#define BT_LINK_IT	0x0b	/* Link cmd ok + int */
#define BT_SEL_TIMEOUT	0x11	/* Selection time out */
#define BT_OVER_UNDER	0x12	/* Data over/under run */
#define BT_BUS_FREE	0x13	/* Bus dropped at unexpected time */
#define BT_INV_BUS	0x14	/* Invalid bus phase/sequence */
#define BT_BAD_MBO	0x15	/* Incorrect MBO cmd */
#define BT_BAD_CCB	0x16	/* Incorrect ccb opcode */
#define BT_BAD_LINK	0x17	/* Not same values of LUN for links */
#define BT_INV_TARGET	0x18	/* Invalid target direction */
#define BT_CCB_DUP	0x19	/* Duplicate CCB received */
#define BT_INV_CCB	0x1a	/* Invalid CCB or segment list */
#define BT_ABORTED	42	/* pseudo value from driver */

struct bt_extended_inquire {
	u_char	bus_type;	/* Type of bus connected to */
#define	BT_BUS_TYPE_24BIT	'A'	/* ISA bus */
#define	BT_BUS_TYPE_32BIT	'E'	/* EISA/VLB/PCI bus */
#define	BT_BUS_TYPE_MCA		'M'	/* MicroChannel bus */
	u_char	bios_address;	/* Address of adapter BIOS */
	u_short	max_segment;	/* ? */
};

struct bt_boardID {
	u_char  board_type;
	u_char  custom_feture;
	char    firm_revision;
	u_char  firm_version;
};

struct bt_setup {
	u_char  sync_neg:1;
	u_char  parity:1;
	u_char	:6;
	u_char  speed;
	u_char  bus_on;
	u_char  bus_off;
	u_char  num_mbx;
	u_char  mbx[3];		/*XXX */
	/* doesn't make sense with 32bit addresses */
	struct {
		u_char  offset:4;
		u_char  period:3;
		u_char  valid:1;
	} sync[8];
	u_char  disc_sts;
};

struct bt_config {
	u_char  chan;
	u_char  intr;
	u_char  scsi_dev:3;
	u_char	:5;
};

#define INT9	0x01
#define INT10	0x02
#define INT11	0x04
#define INT12	0x08
#define INT14	0x20
#define INT15	0x40

#define EISADMA	0x00
#define CHAN0	0x01
#define CHAN5	0x20
#define CHAN6	0x40
#define CHAN7	0x80

#define KVTOPHYS(x)	vtophys(x)

struct bt_softc {
        struct device sc_dev;
        struct isadev sc_id;
        void *sc_ih;

	int sc_iobase;
	int sc_irq, bt_drq;

	struct bt_mbx bt_mbx;		/* all our mailboxes */
	struct bt_ccb *ccbhash[CCB_HASH_SIZE];
	TAILQ_HEAD(, bt_ccb) free_ccb;
	int numccbs;
	int bt_scsi_dev;		/* adapters scsi id */
	struct scsi_link sc_link;	/* prototype for devs */
};

/***********debug values *************/
#define	BT_SHOWCCBS 0x01
#define	BT_SHOWINTS 0x02
#define	BT_SHOWCMDS 0x04
#define	BT_SHOWMISC 0x08
int     bt_debug = 0;

int bt_cmd();	/* XXX must be varargs to prototype */
int btintr __P((void *));
void bt_free_ccb __P((struct bt_softc *, struct bt_ccb *, int));
struct bt_ccb *bt_get_ccb __P((struct bt_softc *, int));
struct bt_ccb *bt_ccb_phys_kv __P((struct bt_softc *, u_long));
struct bt_mbx_out *bt_send_mbo __P((struct bt_softc *, int, struct bt_ccb *));
void bt_done __P((struct bt_softc *, struct bt_ccb *));
int bt_find __P((struct bt_softc *));
void bt_init __P((struct bt_softc *));
void bt_inquire_setup_information __P((struct bt_softc *));
void btminphys __P((struct buf *));
int bt_scsi_cmd __P((struct scsi_xfer *));
int bt_poll __P((struct bt_softc *, struct scsi_xfer *, int));
void bt_timeout __P((void *arg));
#ifdef UTEST
void bt_print_ccb __P((struct bt_ccb *));
void bt_print_active_ccbs __P((struct bt_softc *));
#endif

struct scsi_adapter bt_switch = {
	bt_scsi_cmd,
	btminphys,
	0,
	0,
};

/* the below structure is so we have a default dev struct for out link struct */
struct scsi_device bt_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

int	btprobe __P((struct device *, void *, void *));
void	btattach __P((struct device *, struct device *, void *));
int	btprint __P((void *, char *));

struct cfdriver btcd = {
	NULL, "bt", btprobe, btattach, DV_DULL, sizeof(struct bt_softc)
};

#define BT_RESET_TIMEOUT 1000

/*
 * bt_cmd(bt, icnt, ocnt,wait, retval, opcode, args)
 *
 * Activate Adapter command
 *    icnt:   number of args (outbound bytes written after opcode)
 *    ocnt:   number of expected returned bytes
 *    wait:   number of seconds to wait for response
 *    retval: buffer where to place returned bytes
 *    opcode: opcode BT_NOP, BT_MBX_INIT, BT_START_SCSI ...
 *    args:   parameters
 *
 * Performs an adapter command through the ports.  Not to be confused with a
 * scsi command, which is read in via the dma; one of the adapter commands
 * tells it to read in a scsi command.
 */
int
bt_cmd(bt, icnt, ocnt, wait, retval, opcode, args)
	struct bt_softc *bt;
	int icnt, ocnt, wait;
	u_char *retval;
	unsigned opcode;
	u_char args;
{
	unsigned *ic = &opcode;
	u_char oc;
	register i;
	int sts;

	/*
	 * multiply the wait argument by a big constant
	 * zero defaults to 1
	 */
	if (wait)
		wait *= 100000;
	else
		wait = 100000;
	/*
	 * Wait for the adapter to go idle, unless it's one of
	 * the commands which don't need this
	 */
	if (opcode != BT_MBX_INIT && opcode != BT_START_SCSI) {
		i = 100000;	/* 1 sec? */
		while (--i) {
			sts = inb(BT_CTRL_STAT_PORT);
			if (sts & BT_IDLE) {
				break;
			}
			delay(10);
		}
		if (!i) {
			printf("%s: bt_cmd, host not idle(0x%x)\n",
				bt->sc_dev.dv_xname, sts);
			return ENXIO;
		}
	}
	/*
	 * Now that it is idle, if we expect output, preflush the
	 * queue feeding to us.
	 */
	if (ocnt) {
		while ((inb(BT_CTRL_STAT_PORT)) & BT_DF)
			inb(BT_CMD_DATA_PORT);
	}
	/*
	 * Output the command and the number of arguments given
	 * for each byte, first check the port is empty.
	 */
	icnt++;
	/* include the command */
	while (icnt--) {
		sts = inb(BT_CTRL_STAT_PORT);
		for (i = wait; i; i--) {
			sts = inb(BT_CTRL_STAT_PORT);
			if (!(sts & BT_CDF))
				break;
			delay(10);
		}
		if (!i) {
			printf("%s: bt_cmd, cmd/data port full\n",
				bt->sc_dev.dv_xname);
			outb(BT_CTRL_STAT_PORT, BT_SRST);
			return ENXIO;
		}
		outb(BT_CMD_DATA_PORT, (u_char) (*ic++));
	}
	/*
	 * If we expect input, loop that many times, each time,
	 * looking for the data register to have valid data
	 */
	while (ocnt--) {
		sts = inb(BT_CTRL_STAT_PORT);
		for (i = wait; i; i--) {
			sts = inb(BT_CTRL_STAT_PORT);
			if (sts & BT_DF)
				break;
			delay(10);
		}
		if (!i) {
			printf("bt%d: bt_cmd, cmd/data port empty %d\n",
				bt->sc_dev.dv_xname, ocnt);
			return ENXIO;
		}
		oc = inb(BT_CMD_DATA_PORT);
		if (retval)
			*retval++ = oc;
	}
	/*
	 * Wait for the board to report a finised instruction
	 */
	i = 100000;	/* 1 sec? */
	while (--i) {
		sts = inb(BT_INTR_PORT);
		if (sts & BT_HACC)
			break;
		delay(10);
	}
	if (!i) {
		printf("%s: bt_cmd, host not finished(0x%x)\n",
			bt->sc_dev.dv_xname, sts);
		return ENXIO;
	}
	outb(BT_CTRL_STAT_PORT, BT_IRST);
	return 0;
}

/*
 * Check if the device can be found at the port given
 * and if so, set it up ready for further work
 * as an argument, takes the isa_device structure from
 * autoconf.c
 */
int
btprobe(parent, match, aux)
        struct device *parent;
        void *match, *aux;
{
	struct bt_softc *bt = match;
        register struct isa_attach_args *ia = aux;

#ifdef NEWCONFIG
        if (ia->ia_iobase == IOBASEUNK)
                return 0;
#endif

	bt->sc_iobase = ia->ia_iobase;

	/*
	 * Try initialise a unit at this location
	 * sets up dma and bus speed, loads bt->sc_irq
	 */
	if (bt_find(bt) != 0)
		return 0;

	if (ia->ia_irq != IRQUNK) {
		if (ia->ia_irq != bt->sc_irq) {
			printf("%s: irq mismatch; kernel configured %d != board configured %d\n",
			    bt->sc_dev.dv_xname, ia->ia_irq, bt->sc_irq);
                	return 0;
		}
	} else
		ia->ia_irq = bt->sc_irq;

	if (ia->ia_drq != DRQUNK) {
		if (ia->ia_drq != bt->bt_drq) {
			printf("%s: drq mismatch; kernel configured %d != board configured %d\n",
			    bt->sc_dev.dv_xname, ia->ia_drq, bt->bt_drq);
                	return 0;
		}
	} else
		ia->ia_drq = bt->bt_drq;

	ia->ia_msize = 0;
	ia->ia_iosize = 4;
	return 1;
}

int
btprint(aux, name)
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
btattach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
        struct isa_attach_args *ia = aux;
	struct bt_softc *bt = (void *)self;

	if (ia->ia_drq != DRQUNK)
		isa_dmacascade(ia->ia_drq);

	bt_init(bt);
	TAILQ_INIT(&bt->free_ccb);

	/*
	 * fill in the prototype scsi_link.
	 */
	bt->sc_link.adapter_softc = bt;
	bt->sc_link.adapter_target = bt->bt_scsi_dev;
	bt->sc_link.adapter = &bt_switch;
	bt->sc_link.device = &bt_dev;
	bt->sc_link.openings = 2;

	printf("\n");

#ifdef NEWCONFIG
	isa_establish(&bt->sc_id, &bt->sc_dev);
#endif
	bt->sc_ih = isa_intr_establish(ia->ia_irq, ISA_IST_EDGE, ISA_IPL_BIO,
	    btintr, bt);

	/*
	 * ask the adapter what subunits are present
	 */
	config_found(self, &bt->sc_link, btprint);
}

/*
 * Catch an interrupt from the adaptor
 */
int
btintr(arg)
	void *arg;
{
	struct bt_softc *bt = arg;
	struct bt_mbx_in *wmbi;
	struct bt_mbx *wmbx;
	struct bt_ccb *ccb;
	u_char stat;
	int i;
	int found = 0;

#ifdef BTDEBUG
	printf("%s: btintr ", bt->sc_dev.dv_xname);
#endif /* BTDEBUG */

	/*
	 * First acknowlege the interrupt, Then if it's
	 * not telling about a completed operation
	 * just return.
	 */
	stat = inb(BT_INTR_PORT);
	if ((stat & (BT_MBOA | BT_MBIF)) == 0) {
		outb(BT_CTRL_STAT_PORT, BT_IRST);
		return -1;	/* XXX */
	}

	/* Mail box out empty? */
	if (stat & BT_MBOA) {
		/* Disable MBO available interrupt. */
		outb(BT_CMD_DATA_PORT, BT_MBO_INTR_EN);
		for (i = 100000; i; i--) {
			if (!(inb(BT_CTRL_STAT_PORT) & BT_CDF))
				break;
			delay(10);
		}
		if (!i) {
			printf("%s: btintr, cmd/data port full\n",
			    bt->sc_dev.dv_xname);
			outb(BT_CTRL_STAT_PORT, BT_SRST);
			return 1;
		}
		outb(BT_CMD_DATA_PORT, 0x00);	/* Disable */
		wakeup(&bt->bt_mbx);
	}

	/* Mail box in full? */
	if ((stat & BT_MBIF) == 0)
		return 1;
	wmbx = &bt->bt_mbx;
	wmbi = wmbx->tmbi;
AGAIN:
	while (wmbi->stat != BT_MBI_FREE) {
		ccb = bt_ccb_phys_kv(bt, wmbi->ccb_addr);
		if (!ccb) {
			wmbi->stat = BT_MBI_FREE;
			printf("%s: BAD CCB ADDR!\n", bt->sc_dev.dv_xname);
			continue;
		}
		found++;
		switch (wmbi->stat) {
		case BT_MBI_OK:
		case BT_MBI_ERROR:
			break;

		case BT_MBI_ABORT:
			ccb->host_stat = BT_ABORTED;
			break;

		case BT_MBI_UNKNOWN:
			ccb = 0;
			break;

		default:
			panic("Impossible mbxi status");
		}
#ifdef BTDEBUG
		if (bt_debug && ccb) {
			u_char *cp = &ccb->scsi_cmd;
			printf("op=%x %x %x %x %x %x\n",
			    cp[0], cp[1], cp[2],
			    cp[3], cp[4], cp[5]);
			printf("stat %x for mbi addr = 0x%08x, ",
			    wmbi->stat, wmbi);
			printf("ccb addr = 0x%x\n", ccb);
		}
#endif /* BTDEBUG */
		wmbi->stat = BT_MBI_FREE;
		if (ccb) {
			untimeout(bt_timeout, ccb);
			bt_done(bt, ccb);
		}
		bt_nextmbx(wmbi, wmbx, mbi);
	}
	if (!found) {
		for (i = 0; i < BT_MBX_SIZE; i++) {
			if (wmbi->stat != BT_MBI_FREE) {
				found++;
				break;
			}
			bt_nextmbx(wmbi, wmbx, mbi);
		}
		if (!found) {
#if 0
			printf("%s: mbi interrupt with no full mailboxes\n",
			    bt->sc_dev.dv_xname);
#endif
		} else {
			found = 0;
			goto AGAIN;
		}
	}
	wmbx->tmbi = wmbi;
	outb(BT_CTRL_STAT_PORT, BT_IRST);
	return 1;
}

/*
 * A ccb is put onto the free list.
 */
void
bt_free_ccb(bt, ccb, flags)
	struct bt_softc *bt;
	struct bt_ccb *ccb;
	int flags;
{
	int s;

	s = splbio();

	ccb->flags = CCB_FREE;
	TAILQ_INSERT_HEAD(&bt->free_ccb, ccb, chain);

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (ccb->chain.tqe_next == 0)
		wakeup(&bt->free_ccb);

	splx(s);
}

static inline void
bt_init_ccb(bt, ccb)
	struct bt_softc *bt;
	struct bt_ccb *ccb;
{
	int hashnum;

	bzero(ccb, sizeof(struct bt_ccb));
	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	ccb->hashkey = KVTOPHYS(ccb);
	hashnum = CCB_HASH(ccb->hashkey);
	ccb->nexthash = bt->ccbhash[hashnum];
	bt->ccbhash[hashnum] = ccb;
}

static inline void
bt_reset_ccb(bt, ccb)
	struct bt_softc *bt;
	struct bt_ccb *ccb;
{

}

/*
 * Get a free ccb
 *
 * If there are none, see if we can allocate a new one.  If so, put it in
 * the hash table too otherwise either return an error or sleep.
 */
struct bt_ccb *
bt_get_ccb(bt, flags)
	struct bt_softc *bt;
	int flags;
{
	struct bt_ccb *ccb;
	int s;

	s = splbio();

	/*
	 * If we can and have to, sleep waiting for one to come free
	 * but only if we can't allocate a new one.
	 */
	for (;;) {
		ccb = bt->free_ccb.tqh_first;
		if (ccb) {
			TAILQ_REMOVE(&bt->free_ccb, ccb, chain);
			break;
		}
		if (bt->numccbs < BT_CCB_MAX) {
			if (ccb = (struct bt_ccb *) malloc(sizeof(struct bt_ccb),
			    M_TEMP, M_NOWAIT)) {
				bt_init_ccb(bt, ccb);
				bt->numccbs++;
			} else {
				printf("%s: can't malloc ccb\n",
				    bt->sc_dev.dv_xname);
				goto out;
			}
			break;
		}
		if ((flags & SCSI_NOSLEEP) != 0)
			goto out;
		tsleep(&bt->free_ccb, PRIBIO, "btccb", 0);
	}

	bt_reset_ccb(bt, ccb);
	ccb->flags = CCB_ACTIVE;

out:
	splx(s);
	return ccb;
}

/*
 * given a physical address, find the ccb that
 * it corresponds to:
 */
struct bt_ccb *
bt_ccb_phys_kv(bt, ccb_phys)
	struct bt_softc *bt;
	u_long ccb_phys;
{
	int hashnum = CCB_HASH(ccb_phys);
	struct bt_ccb *ccb = bt->ccbhash[hashnum];

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
struct bt_mbx_out *
bt_send_mbo(bt, cmd, ccb)
	struct bt_softc *bt;
	int cmd;
	struct bt_ccb *ccb;
{
	struct bt_mbx_out *wmbo;	/* Mail Box Out pointer */
	struct bt_mbx *wmbx;		/* Mail Box pointer specified unit */
	int i;

	/* Get the target out mail box pointer and increment. */
	wmbx = &bt->bt_mbx;
	wmbo = wmbx->tmbo;
	bt_nextmbx(wmbx->tmbo, wmbx, mbo);

	/*
	 * Check the outmail box is free or not.
	 * Note: Under the normal operation, it shuld NOT happen to wait.
	 */
	while (wmbo->cmd != BT_MBO_FREE) {
		/* Enable mbo available interrupt. */
		outb(BT_CMD_DATA_PORT, BT_MBO_INTR_EN);
		for (i = 100000; i; i--) {
			if (!(inb(BT_CTRL_STAT_PORT) & BT_CDF))
				break;
			delay(10);
		}
		if (!i) {
			printf("%s: bt_send_mbo, cmd/data port full\n",
			    bt->sc_dev.dv_xname);
			outb(BT_CTRL_STAT_PORT, BT_SRST);
			return NULL;
		}
		outb(BT_CMD_DATA_PORT, 0x01);	/* Enable */
		tsleep(wmbx, PRIBIO, "btsnd", 0);/*XXX can't do this */
	}

	/* Link ccb to mbo. */
	wmbo->ccb_addr = KVTOPHYS(ccb);
	ccb->mbx = wmbo;
	wmbo->cmd = cmd;

	/* Send it! */
	outb(BT_CMD_DATA_PORT, BT_START_SCSI);

	return wmbo;
}

/*
 * We have a ccb which has been processed by the
 * adaptor, now we look to see how the operation
 * went. Wake up the owner if waiting
 */
void
bt_done(bt, ccb)
	struct bt_softc *bt;
	struct bt_ccb *ccb;
{
	struct scsi_sense_data *s1, *s2;
	struct scsi_xfer *xs = ccb->xs;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("bt_done\n"));
	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
	if ((xs->flags & INUSE) == 0) {
		printf("%s: exiting but not in use!\n", bt->sc_dev.dv_xname);
		Debugger();
	}
	if (xs->error == XS_NOERROR) {
		if (ccb->host_stat != BT_OK) {
			switch (ccb->host_stat) {
			case BT_ABORTED:
				xs->error = XS_DRIVER_STUFFUP;
				break;
			case BT_SEL_TIMEOUT:	/* No response */
				xs->error = XS_SELTIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				printf("%s: host_stat %x\n",
				    bt->sc_dev.dv_xname, ccb->host_stat);
				xs->error = XS_DRIVER_STUFFUP;
			}
		} else if (ccb->target_stat != SCSI_OK) {
			switch (ccb->target_stat) {
			case SCSI_CHECK:
				s1 = &ccb->scsi_sense;
				s2 = &xs->sense;
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case SCSI_BUSY:
				xs->error = XS_BUSY;
				break;
			default:
				printf("%s: target_stat %x\n",
				    bt->sc_dev.dv_xname, ccb->target_stat);
				xs->error = XS_DRIVER_STUFFUP;
			}
		} else
			xs->resid = 0;
	}
	xs->flags |= ITSDONE;
	bt_free_ccb(bt, ccb, xs->flags);
	scsi_done(xs);
}

/*
 * Find the board and find it's irq/drq
 */
int
bt_find(bt)
	struct bt_softc *bt;
{
	u_char ad[4];
	volatile int i, sts;
	struct bt_extended_inquire info;
	struct bt_config conf;

	/*
	 * reset board, If it doesn't respond, assume
	 * that it's not there.. good for the probe
	 */

	outb(BT_CTRL_STAT_PORT, BT_HRST | BT_SRST);

	for (i = BT_RESET_TIMEOUT; i; i--) {
		sts = inb(BT_CTRL_STAT_PORT);
		if (sts == (BT_IDLE | BT_INIT))
			break;
		delay(1000);
	}
	if (!i) {
#ifdef	UTEST
		printf("bt_find: No answer from bt742a board\n");
#endif
		return ENXIO;
	}

	/*
	 * Check that we actually know how to use this board.
	 */
	delay(1000);
	bt_cmd(bt, 1, sizeof(info), 0, &info, BT_INQUIRE_EXTENDED, sizeof(info));
	switch (info.bus_type) {
	case BT_BUS_TYPE_24BIT:
		/* XXXX How do we avoid conflicting with the aha1542 probe? */
	case BT_BUS_TYPE_32BIT:
		break;
	case BT_BUS_TYPE_MCA:
		/* We don't grok MicroChannel (yet). */
		return EINVAL;
	default:
		printf("%s: illegal bus type %c\n", bt->sc_dev.dv_xname,
		    info.bus_type);
		return EINVAL;
	}

	/*
	 * Assume we have a board at this stage setup dma channel from
	 * jumpers and save int level
	 */
	delay(1000);
	bt_cmd(bt, 0, sizeof(conf), 0, &conf, BT_CONF_GET);
	switch (conf.chan) {
	case EISADMA:
		bt->bt_drq = DRQUNK;
		break;
	case CHAN0:
		bt->bt_drq = 0;
		break;
	case CHAN5:
		bt->bt_drq = 5;
		break;
	case CHAN6:
		bt->bt_drq = 6;
		break;
	case CHAN7:
		bt->bt_drq = 7;
		break;
	default:
		printf("%s: illegal dma setting %x\n", bt->sc_dev.dv_xname,
		    conf.chan);
		return EIO;
	}

	switch (conf.intr) {
	case INT9:
		bt->sc_irq = 9;
		break;
	case INT10:
		bt->sc_irq = 10;
		break;
	case INT11:
		bt->sc_irq = 11;
		break;
	case INT12:
		bt->sc_irq = 12;
		break;
	case INT14:
		bt->sc_irq = 14;
		break;
	case INT15:
		bt->sc_irq = 15;
		break;
	default:
		printf("%s: illegal int setting %x\n", bt->sc_dev.dv_xname,
		    conf.intr);
		return EIO;
	}

	/* who are we on the scsi bus? */
	bt->bt_scsi_dev = conf.scsi_dev;

	return 0;
}

/*
 * Start the board, ready for normal operation
 */
void
bt_init(bt)
	struct bt_softc *bt;
{
	u_char ad[4];
	int i;

	/*
	 * Initialize mail box
	 */
	*((physaddr *)ad) = KVTOPHYS(&bt->bt_mbx);

	bt_cmd(bt, 5, 0, 0, 0, BT_MBX_INIT_EXTENDED, BT_MBX_SIZE,
	    ad[0], ad[1], ad[2], ad[3]);

	for (i = 0; i < BT_MBX_SIZE; i++) {
		bt->bt_mbx.mbo[i].cmd = BT_MBO_FREE;
		bt->bt_mbx.mbi[i].stat = BT_MBI_FREE;
	}

	/*
	 * Set up initial mail box for round-robin operation.
	 */
	bt->bt_mbx.tmbo = &bt->bt_mbx.mbo[0];
	bt->bt_mbx.tmbi = &bt->bt_mbx.mbi[0];

	bt_inquire_setup_information(bt);
}

void
bt_inquire_setup_information(bt)
	struct bt_softc *bt;
{
	struct bt_boardID bID;
	char dummy[8];
	struct bt_setup setup;
	int i;

	/* Inquire Board ID to Bt742 for firmware version */
	bt_cmd(bt, 0, sizeof(bID), 0, &bID, BT_INQUIRE);
	printf(": version %c.%c, ", bID.firm_revision, bID.firm_version);

	if (bID.firm_revision != '2') {	/* XXXX */
		/* Enable round-robin scheme - appeared at firmware rev. 3.31 */
		bt_cmd(bt, 1, 0, 0, 0, BT_ROUND_ROBIN, BT_ENABLE);
	}

	/* Inquire Installed Devices (to force synchronous negotiation) */
	bt_cmd(bt, 0, sizeof(dummy), 10, dummy, BT_DEV_GET);

	/* Obtain setup information from Bt742. */
	bt_cmd(bt, 1, sizeof(setup), 0, &setup, BT_SETUP_GET, sizeof(setup));

	printf("%s, %s, %d mbxs",
	    setup.sync_neg ? "sync" : "async",
	    setup.parity ? "parity" : "no parity",
	    setup.num_mbx);

	for (i = 0; i < 8; i++) {
		if (!setup.sync[i].valid ||
		    (!setup.sync[i].offset && !setup.sync[i].period))
			continue;
		printf("\n%s targ %d: sync, offset %d, period %dnsec",
		    bt->sc_dev.dv_xname, i,
		    setup.sync[i].offset, setup.sync[i].period * 50 + 200);
	}
}

void
btminphys(bp)
	struct buf *bp;
{

	if (bp->b_bcount > ((BT_NSEG - 1) << PGSHIFT))
		bp->b_bcount = ((BT_NSEG - 1) << PGSHIFT);
	minphys(bp);
}

/*
 * start a scsi operation given the command and the data address.  Also needs
 * the unit, target and lu.
 */
int
bt_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct bt_softc *bt = sc_link->adapter_softc;
	struct bt_ccb *ccb;
	struct bt_scat_gath *sg;
	int seg;		/* scatter gather seg being worked on */
	int thiskv;
	physaddr thisphys, nextphys;
	int bytes_this_seg, bytes_this_page, datalen, flags;
	struct iovec *iovp;
	struct bt_mbx_out *mbo;
	int s;

	SC_DEBUG(sc_link, SDEV_DB2, ("bt_scsi_cmd\n"));
	/*
	 * get a ccb to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if ((flags & (ITSDONE|INUSE)) != INUSE) {
		printf("%s: done or not in use?\n", bt->sc_dev.dv_xname);
		xs->flags &= ~ITSDONE;
		xs->flags |= INUSE;
	}
	if ((ccb = bt_get_ccb(bt, flags)) == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return TRY_AGAIN_LATER;
	}
	ccb->xs = xs;

	/*
	 * Put all the arguments for the xfer in the ccb
	 */
	if (flags & SCSI_RESET) {
		ccb->opcode = BT_RESET_CCB;
	} else {
		/* can't use S/G if zero length */
		ccb->opcode = (xs->datalen ? BT_INIT_SCAT_GATH_CCB
					   : BT_INITIATOR_CCB);
	}
	ccb->data_out = 0;
	ccb->data_in = 0;
	ccb->target = sc_link->target;
	ccb->lun = sc_link->lun;
	ccb->scsi_cmd_length = xs->cmdlen;
	ccb->sense_ptr = KVTOPHYS(&ccb->scsi_sense);
	ccb->req_sense_length = sizeof(ccb->scsi_sense);
	ccb->host_stat = 0x00;
	ccb->target_stat = 0x00;

	if (xs->datalen && (flags & SCSI_RESET) == 0) {
		ccb->data_addr = KVTOPHYS(ccb->scat_gath);
		sg = ccb->scat_gath;
		seg = 0;
#ifdef	TFS
		if (flags & SCSI_DATA_UIO) {
			iovp = ((struct uio *)xs->data)->uio_iov;
			datalen = ((struct uio *)xs->data)->uio_iovcnt;
			xs->datalen = 0;
			while (datalen && seg < BT_NSEG) {
				sg->seg_addr = (physaddr)iovp->iov_base;
				sg->seg_len = iovp->iov_len;
				xs->datalen += iovp->iov_len;
				SC_DEBUGN(sc_link, SDEV_DB4, ("(0x%x@0x%x)",
				    iovp->iov_len, iovp->iov_base));
				sg++;
				iovp++;
				seg++;
				datalen--;
			}
		} else
#endif	/* TFS */
		{
			/*
			 * Set up the scatter gather block
			 */
			SC_DEBUG(sc_link, SDEV_DB4,
			    ("%d @0x%x:- ", xs->datalen, xs->data));
			datalen = xs->datalen;
			thiskv = (int) xs->data;
			thisphys = KVTOPHYS(thiskv);

			while (datalen && seg < BT_NSEG) {
				bytes_this_seg = 0;

				/* put in the base address */
				sg->seg_addr = thisphys;

				SC_DEBUGN(sc_link, SDEV_DB4, ("0x%x", thisphys));

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
						thisphys = KVTOPHYS(thiskv);
				}
				/*
				 * next page isn't contiguous, finish the seg
				 */
				SC_DEBUGN(sc_link, SDEV_DB4,
				    ("(0x%x)", bytes_this_seg));
				sg->seg_len = bytes_this_seg;
				sg++;
				seg++;
			}
		}
		/* end of iov/kv decision */
		ccb->data_length = seg * sizeof(struct bt_scat_gath);
		SC_DEBUGN(sc_link, SDEV_DB4, ("\n"));
		if (datalen) {
			/*
			 * there's still data, must have run out of segs!
			 */
			printf("%s: bt_scsi_cmd, more than %d dma segs\n",
			    bt->sc_dev.dv_xname, BT_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			bt_free_ccb(bt, ccb, flags);
			return COMPLETE;
		}
	} else {		/* No data xfer, use non S/G values */
		ccb->data_addr = (physaddr)0;
		ccb->data_length = 0;
	}
	ccb->link_id = 0;
	ccb->link_addr = (physaddr)0;

	/*
	 * Put the scsi command in the ccb and start it
	 */
	if ((flags & SCSI_RESET) == 0)
		bcopy(xs->cmd, &ccb->scsi_cmd, ccb->scsi_cmd_length);

	s = splbio();

	if (bt_send_mbo(bt, BT_MBO_START, ccb) == NULL) {
		splx(s);
		xs->error = XS_DRIVER_STUFFUP;
		bt_free_ccb(bt, ccb, flags);
		return TRY_AGAIN_LATER;
	}

	/*
	 * Usually return SUCCESSFULLY QUEUED
	 */
	SC_DEBUG(sc_link, SDEV_DB3, ("cmd_sent\n"));
	if ((flags & SCSI_POLL) == 0) {
		timeout(bt_timeout, ccb, (xs->timeout * hz) / 1000);
		splx(s);
		return SUCCESSFULLY_QUEUED;
	}

	splx(s);

	/*
	 * If we can't use interrupts, poll on completion
	 */
	if (bt_poll(bt, xs, xs->timeout)) {
		bt_timeout(ccb);
		if (bt_poll(bt, xs, 2000))
			bt_timeout(ccb);
	}
	return COMPLETE;
}

/*
 * Poll a particular unit, looking for a particular xs
 */
int
bt_poll(bt, xs, count)
	struct bt_softc *bt;
	struct scsi_xfer *xs;
	int count;
{

	/* timeouts are in msec, so we loop in 1000 usec cycles */
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (inb(BT_INTR_PORT) & BT_ANY_INTR)
			btintr(bt);
		if (xs->flags & ITSDONE)
			return 0;
		delay(1000);	/* only happens in boot so ok */
		count--;
	}
	return 1;
}

void
bt_timeout(arg)
	void *arg;
{
	struct bt_ccb *ccb = arg;
	struct scsi_xfer *xs = ccb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct bt_softc *bt = sc_link->adapter_softc;
	int s;

	sc_print_addr(sc_link);
	printf("timed out");

	s = splbio();

	/*
	 * If the ccb's mbx is not free, then the board has gone Far East?
	 */
	if (bt_ccb_phys_kv(bt, ccb->mbx->ccb_addr) == ccb &&
	    ccb->mbx->cmd != BT_MBO_FREE) {
		printf("%s: not taking commands!\n", bt->sc_dev.dv_xname);
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
		bt_done(bt, ccb);
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		ccb->xs->error = XS_TIMEOUT;
		ccb->flags = CCB_ABORTED;
		bt_send_mbo(bt, BT_MBO_ABORT, ccb);
		/* 2 secs for the abort */
		if ((xs->flags & SCSI_POLL) == 0)
			timeout(bt_timeout, ccb, 2 * hz);
	}

	splx(s);
}

#ifdef	UTEST
void
bt_print_ccb(ccb)
	struct bt_ccb *ccb;
{

	printf("ccb:%x op:%x cmdlen:%d senlen:%d\n",
		ccb, ccb->opcode, ccb->scsi_cmd_length, ccb->req_sense_length);
	printf("	datlen:%d hstat:%x tstat:%x flags:%x\n",
		ccb->data_length, ccb->host_stat, ccb->target_stat, ccb->flags);
}

void
bt_print_active_ccbs(bt)
	struct bt_softc *bt;
{
	struct bt_ccb *ccb;
	int i = 0;

	while (i < CCB_HASH_SIZE) {
		ccb = bt->ccbhash[i];
		while (ccb) {
			if (ccb->flags != CCB_FREE)
				bt_print_ccb(ccb);
			ccb = ccb->nexthash;
		}
		i++;
	}
}
#endif /*UTEST */
