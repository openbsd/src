/*	$OpenBSD: bt.c,v 1.23 1999/01/07 06:14:47 niklas Exp $	*/
/*	$NetBSD: bt.c,v 1.10 1996/05/12 23:51:54 mycroft Exp $	*/

#undef BTDIAG
#define integrate

/*
 * Copyright (c) 1994, 1996 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
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

#include <machine/intr.h>
#include <machine/pio.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>
#include <dev/isa/btreg.h>

#ifndef DDB
#define Debugger() panic("should call debugger here (bt742a.c)")
#endif /* ! DDB */

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

struct bt_mbx {
	struct bt_mbx_out mbo[BT_MBX_SIZE];
	struct bt_mbx_in mbi[BT_MBX_SIZE];
	struct bt_mbx_out *cmbo;	/* Collection Mail Box out */
	struct bt_mbx_out *tmbo;	/* Target Mail Box out */
	struct bt_mbx_in *tmbi;		/* Target Mail Box in */
};

#define KVTOPHYS(x)	vtophys(x)

#include "aha.h"
#include "bt.h"
#if NAHA > 0
int btports[NBT];
int nbtports;
#endif

struct bt_softc {
	struct device sc_dev;
	struct isadev sc_id;
	void *sc_ih;

	int sc_iobase;
	int sc_irq, sc_drq;

	char sc_model[7],
	     sc_firmware[6];

	struct bt_mbx sc_mbx;		/* all our mailboxes */
#define	wmbx	(&sc->sc_mbx)
	struct bt_ccb *sc_ccbhash[CCB_HASH_SIZE];
	TAILQ_HEAD(, bt_ccb) sc_free_ccb, sc_waiting_ccb;
	int sc_numccbs, sc_mbofull;
	int sc_scsi_dev;		/* adapters scsi id */
	struct scsi_link sc_link;	/* prototype for devs */
};

#ifdef BTDEBUG
int     bt_debug = 0;
#endif /* BTDEBUG */

int bt_cmd __P((int, struct bt_softc *, int, u_char *, int, u_char *));
integrate void bt_finish_ccbs __P((struct bt_softc *));
int btintr __P((void *));
integrate void bt_reset_ccb __P((struct bt_softc *, struct bt_ccb *));
void bt_free_ccb __P((struct bt_softc *, struct bt_ccb *));
integrate void bt_init_ccb __P((struct bt_softc *, struct bt_ccb *));
struct bt_ccb *bt_get_ccb __P((struct bt_softc *, int));
struct bt_ccb *bt_ccb_phys_kv __P((struct bt_softc *, u_long));
void bt_queue_ccb __P((struct bt_softc *, struct bt_ccb *));
void bt_collect_mbo __P((struct bt_softc *));
void bt_start_ccbs __P((struct bt_softc *));
void bt_done __P((struct bt_softc *, struct bt_ccb *));
int bt_find __P((struct isa_attach_args *, struct bt_softc *));
void bt_init __P((struct bt_softc *));
void bt_inquire_setup_information __P((struct bt_softc *));
void btminphys __P((struct buf *));
int bt_scsi_cmd __P((struct scsi_xfer *));
int bt_poll __P((struct bt_softc *, struct scsi_xfer *, int));
void bt_timeout __P((void *arg));

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

struct cfattach bt_ca = {
	sizeof(struct bt_softc), btprobe, btattach
};

struct cfdriver bt_cd = {
	NULL, "bt", DV_DULL
};

#define BT_RESET_TIMEOUT	2000	/* time to wait for reset (mSec) */
#define	BT_ABORT_TIMEOUT	2000	/* time to wait for abort (mSec) */

/*
 * bt_cmd(iobase, sc, icnt, ibuf, ocnt, obuf)
 *
 * Activate Adapter command
 *    icnt:   number of args (outbound bytes including opcode)
 *    ibuf:   argument buffer
 *    ocnt:   number of expected returned bytes
 *    obuf:   result buffer
 *    wait:   number of seconds to wait for response
 *
 * Performs an adapter command through the ports.  Not to be confused with a
 * scsi command, which is read in via the dma; one of the adapter commands
 * tells it to read in a scsi command.
 */
int
bt_cmd(iobase, sc, icnt, ibuf, ocnt, obuf)
	int iobase;
	struct bt_softc *sc;
	int icnt, ocnt;
	u_char *ibuf, *obuf;
{
	const char *name;
	register int i;
	int wait;
	u_char sts;
	u_char opcode = ibuf[0];

	if (sc != NULL)
		name = sc->sc_dev.dv_xname;
	else
		name = "(bt probe)";

	/*
	 * Calculate a reasonable timeout for the command.
	 */
	switch (opcode) {
	case BT_INQUIRE_DEVICES:
		wait = 15 * 20000;
		break;
	default:
		wait = 1 * 20000;
		break;
	}

	/*
	 * Wait for the adapter to go idle, unless it's one of
	 * the commands which don't need this
	 */
	if (opcode != BT_MBO_INTR_EN) {
		for (i = 20000; i; i--) {	/* 1 sec? */
			sts = inb(iobase + BT_STAT_PORT);
			if (sts & BT_STAT_IDLE)
				break;
			delay(50);
		}
		if (!i) {
			printf("%s: bt_cmd, host not idle(0x%x)\n",
			    name, sts);
			return ENXIO;
		}
	}
	/*
	 * Now that it is idle, if we expect output, preflush the
	 * queue feeding to us.
	 */
	if (ocnt) {
		while ((inb(iobase + BT_STAT_PORT)) & BT_STAT_DF)
			inb(iobase + BT_DATA_PORT);
	}
	/*
	 * Output the command and the number of arguments given
	 * for each byte, first check the port is empty.
	 */
	while (icnt--) {
		for (i = wait; i; i--) {
			sts = inb(iobase + BT_STAT_PORT);
			if (!(sts & BT_STAT_CDF))
				break;
			delay(50);
		}
		if (!i) {
			if (opcode != BT_INQUIRE_REVISION &&
			    opcode != BT_INQUIRE_REVISION_3)
				printf("%s: bt_cmd, cmd/data port full\n", name);
			outb(iobase + BT_CTRL_PORT, BT_CTRL_SRST);
			return ENXIO;
		}
		outb(iobase + BT_CMD_PORT, *ibuf++);
	}
	/*
	 * If we expect input, loop that many times, each time,
	 * looking for the data register to have valid data
	 */
	while (ocnt--) {
		for (i = wait; i; i--) {
			sts = inb(iobase + BT_STAT_PORT);
			if (sts & BT_STAT_DF)
				break;
			delay(50);
		}
		if (!i) {
			if (opcode != BT_INQUIRE_REVISION &&
			    opcode != BT_INQUIRE_REVISION_3)
				printf("%s: bt_cmd, cmd/data port empty %d\n",
				    name, ocnt);
			outb(iobase + BT_CTRL_PORT, BT_CTRL_SRST);
			return ENXIO;
		}
		*obuf++ = inb(iobase + BT_DATA_PORT);
	}
	/*
	 * Wait for the board to report a finished instruction.
	 * We may get an extra interrupt for the HACC signal, but this is
	 * unimportant.
	 */
	if (opcode != BT_MBO_INTR_EN) {
		for (i = 20000; i; i--) {	/* 1 sec? */
			sts = inb(iobase + BT_INTR_PORT);
			/* XXX Need to save this in the interrupt handler? */
			if (sts & BT_INTR_HACC)
				break;
			delay(50);
		}
		if (!i) {
			printf("%s: bt_cmd, host not finished(0x%x)\n",
			    name, sts);
			return ENXIO;
		}
	}
	outb(iobase + BT_CTRL_PORT, BT_CTRL_IRST);
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
	register struct isa_attach_args *ia = aux;

	/* See if there is a unit at this location. */
	if (bt_find(ia, NULL) != 0)
		return 0;

	ia->ia_msize = 0;
	ia->ia_iosize = 4;
	/* IRQ and DRQ set by bt_find(). */
	return 1;
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
	struct bt_softc *sc = (void *)self;

	if (bt_find(ia, sc) != 0)
		panic("btattach: bt_find of %s failed", self->dv_xname);
	sc->sc_iobase = ia->ia_iobase;

	if (sc->sc_drq != DRQUNK)
		isadma_cascade(sc->sc_drq);

	bt_inquire_setup_information(sc);
	bt_init(sc);
	TAILQ_INIT(&sc->sc_free_ccb);
	TAILQ_INIT(&sc->sc_waiting_ccb);

	/*
	 * fill in the prototype scsi_link.
	 */
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->sc_scsi_dev;
	sc->sc_link.adapter = &bt_switch;
	sc->sc_link.device = &bt_dev;
	sc->sc_link.openings = 4;

	sc->sc_ih = isa_intr_establish(ia->ia_ic, sc->sc_irq, IST_EDGE,
	    IPL_BIO, btintr, sc, sc->sc_dev.dv_xname);

	/*
	 * ask the adapter what subunits are present
	 */
	config_found(self, &sc->sc_link, scsiprint);
}

integrate void
bt_finish_ccbs(sc)
	struct bt_softc *sc;
{
	struct bt_mbx_in *wmbi;
	struct bt_ccb *ccb;
	int i;

	wmbi = wmbx->tmbi;

	if (wmbi->stat == BT_MBI_FREE) {
		for (i = 0; i < BT_MBX_SIZE; i++) {
			if (wmbi->stat != BT_MBI_FREE) {
				printf("%s: mbi not in round-robin order\n",
				    sc->sc_dev.dv_xname);
				goto AGAIN;
			}
			bt_nextmbx(wmbi, wmbx, mbi);
		}
#ifdef BTDIAGnot
		printf("%s: mbi interrupt with no full mailboxes\n",
		    sc->sc_dev.dv_xname);
#endif
		return;
	}

AGAIN:
	do {
		ccb = bt_ccb_phys_kv(sc, phystol(wmbi->ccb_addr));
		if (!ccb) {
			printf("%s: bad mbi ccb pointer; skipping\n",
			    sc->sc_dev.dv_xname);
			goto next;
		}

#ifdef BTDEBUG
		if (bt_debug) {
			u_char *cp = &ccb->scsi_cmd;
			printf("op=%x %x %x %x %x %x\n",
			    cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
			printf("stat %x for mbi addr = 0x%08x, ",
			    wmbi->stat, wmbi);
			printf("ccb addr = 0x%x\n", ccb);
		}
#endif /* BTDEBUG */

		switch (wmbi->stat) {
		case BT_MBI_OK:
		case BT_MBI_ERROR:
			if ((ccb->flags & CCB_ABORT) != 0) {
				/*
				 * If we already started an abort, wait for it
				 * to complete before clearing the CCB.  We
				 * could instead just clear CCB_SENDING, but
				 * what if the mailbox was already received?
				 * The worst that happens here is that we clear
				 * the CCB a bit later than we need to.  BFD.
				 */
				goto next;
			}
			break;

		case BT_MBI_ABORT:
		case BT_MBI_UNKNOWN:
			/*
			 * Even if the CCB wasn't found, we clear it anyway.
			 * See preceeding comment.
			 */
			break;

		default:
			printf("%s: bad mbi status %02x; skipping\n",
			    sc->sc_dev.dv_xname, wmbi->stat);
			goto next;
		}

		untimeout(bt_timeout, ccb);
		bt_done(sc, ccb);

	next:
		wmbi->stat = BT_MBI_FREE;
		bt_nextmbx(wmbi, wmbx, mbi);
	} while (wmbi->stat != BT_MBI_FREE);

	wmbx->tmbi = wmbi;
}

/*
 * Catch an interrupt from the adaptor
 */
int
btintr(arg)
	void *arg;
{
	struct bt_softc *sc = arg;
	int iobase = sc->sc_iobase;
	u_char sts;

#ifdef BTDEBUG
	printf("%s: btintr ", sc->sc_dev.dv_xname);
#endif /* BTDEBUG */

	/*
	 * First acknowlege the interrupt, Then if it's not telling about
	 * a completed operation just return.
	 */
	sts = inb(iobase + BT_INTR_PORT);
	if ((sts & BT_INTR_ANYINTR) == 0)
		return 0;
	outb(iobase + BT_CTRL_PORT, BT_CTRL_IRST);

#ifdef BTDIAG
	/* Make sure we clear CCB_SENDING before finishing a CCB. */
	bt_collect_mbo(sc);
#endif

	/* Mail box out empty? */
	if (sts & BT_INTR_MBOA) {
		struct bt_toggle toggle;

		toggle.cmd.opcode = BT_MBO_INTR_EN;
		toggle.cmd.enable = 0;
		bt_cmd(iobase, sc, sizeof(toggle.cmd), (u_char *)&toggle.cmd, 0,
		    (u_char *)0);
		bt_start_ccbs(sc);
	}

	/* Mail box in full? */
	if (sts & BT_INTR_MBIF)
		bt_finish_ccbs(sc);

	return 1;
}

integrate void
bt_reset_ccb(sc, ccb)
	struct bt_softc *sc;
	struct bt_ccb *ccb;
{

	ccb->flags = 0;
}

/*
 * A ccb is put onto the free list.
 */
void
bt_free_ccb(sc, ccb)
	struct bt_softc *sc;
	struct bt_ccb *ccb;
{
	int s;

	s = splbio();

	bt_reset_ccb(sc, ccb);
	TAILQ_INSERT_HEAD(&sc->sc_free_ccb, ccb, chain);

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (ccb->chain.tqe_next == 0)
		wakeup(&sc->sc_free_ccb);

	splx(s);
}

integrate void
bt_init_ccb(sc, ccb)
	struct bt_softc *sc;
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
	ccb->nexthash = sc->sc_ccbhash[hashnum];
	sc->sc_ccbhash[hashnum] = ccb;
	bt_reset_ccb(sc, ccb);
}

/*
 * Get a free ccb
 *
 * If there are none, see if we can allocate a new one.  If so, put it in
 * the hash table too otherwise either return an error or sleep.
 */
struct bt_ccb *
bt_get_ccb(sc, flags)
	struct bt_softc *sc;
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
		ccb = sc->sc_free_ccb.tqh_first;
		if (ccb) {
			TAILQ_REMOVE(&sc->sc_free_ccb, ccb, chain);
			break;
		}
		if (sc->sc_numccbs < BT_CCB_MAX) {
			ccb = (struct bt_ccb *) malloc(sizeof(struct bt_ccb),
			    M_TEMP, M_NOWAIT);
			if (!ccb) {
				printf("%s: can't malloc ccb\n",
				    sc->sc_dev.dv_xname);
				goto out;
			}
			bt_init_ccb(sc, ccb);
			sc->sc_numccbs++;
			break;
		}
		if ((flags & SCSI_NOSLEEP) != 0)
			goto out;
		tsleep(&sc->sc_free_ccb, PRIBIO, "btccb", 0);
	}

	ccb->flags |= CCB_ALLOC;

out:
	splx(s);
	return ccb;
}

/*
 * Given a physical address, find the ccb that it corresponds to.
 */
struct bt_ccb *
bt_ccb_phys_kv(sc, ccb_phys)
	struct bt_softc *sc;
	u_long ccb_phys;
{
	int hashnum = CCB_HASH(ccb_phys);
	struct bt_ccb *ccb = sc->sc_ccbhash[hashnum];

	while (ccb) {
		if (ccb->hashkey == ccb_phys)
			break;
		ccb = ccb->nexthash;
	}
	return ccb;
}

/*
 * Queue a CCB to be sent to the controller, and send it if possible.
 */
void
bt_queue_ccb(sc, ccb)
	struct bt_softc *sc;
	struct bt_ccb *ccb;
{

	TAILQ_INSERT_TAIL(&sc->sc_waiting_ccb, ccb, chain);
	bt_start_ccbs(sc);
}

/*
 * Garbage collect mailboxes that are no longer in use.
 */
void
bt_collect_mbo(sc)
	struct bt_softc *sc;
{
	struct bt_mbx_out *wmbo;	/* Mail Box Out pointer */
#ifdef BTDIAG
	struct bt_ccb *ccb;
#endif
	wmbo = wmbx->cmbo;

	while (sc->sc_mbofull > 0) {
		if (wmbo->cmd != BT_MBO_FREE)
			break;

#ifdef BTDIAG
		ccb = bt_ccb_phys_kv(sc, phystol(wmbo->ccb_addr));
		ccb->flags &= ~CCB_SENDING;
#endif

		--sc->sc_mbofull;
		bt_nextmbx(wmbo, wmbx, mbo);
	}

	wmbx->cmbo = wmbo;
}

/*
 * Send as many CCBs as we have empty mailboxes for.
 */
void
bt_start_ccbs(sc)
	struct bt_softc *sc;
{
	int iobase = sc->sc_iobase;
	struct bt_mbx_out *wmbo;	/* Mail Box Out pointer */
	struct bt_ccb *ccb;

	wmbo = wmbx->tmbo;

	while ((ccb = sc->sc_waiting_ccb.tqh_first) != NULL) {
		if (sc->sc_mbofull >= BT_MBX_SIZE) {
			bt_collect_mbo(sc);
			if (sc->sc_mbofull >= BT_MBX_SIZE) {
				struct bt_toggle toggle;

				toggle.cmd.opcode = BT_MBO_INTR_EN;
				toggle.cmd.enable = 1;
				bt_cmd(iobase, sc, sizeof(toggle.cmd),
				    (u_char *)&toggle.cmd, 0, (u_char *)0);
				break;
			}
		}

		TAILQ_REMOVE(&sc->sc_waiting_ccb, ccb, chain);
#ifdef BTDIAG
		ccb->flags |= CCB_SENDING;
#endif

		/* Link ccb to mbo. */
		ltophys(KVTOPHYS(ccb), wmbo->ccb_addr);
		if (ccb->flags & CCB_ABORT)
			wmbo->cmd = BT_MBO_ABORT;
		else
			wmbo->cmd = BT_MBO_START;

		/* Tell the card to poll immediately. */
		outb(iobase + BT_CMD_PORT, BT_START_SCSI);

		if ((ccb->xs->flags & SCSI_POLL) == 0)
			timeout(bt_timeout, ccb, (ccb->timeout * hz) / 1000);

		++sc->sc_mbofull;
		bt_nextmbx(wmbo, wmbx, mbo);
	}

	wmbx->tmbo = wmbo;
}

/*
 * We have a ccb which has been processed by the
 * adaptor, now we look to see how the operation
 * went. Wake up the owner if waiting
 */
void
bt_done(sc, ccb)
	struct bt_softc *sc;
	struct bt_ccb *ccb;
{
	struct scsi_sense_data *s1, *s2;
	struct scsi_xfer *xs = ccb->xs;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("bt_done\n"));
	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
#ifdef BTDIAG
	if (ccb->flags & CCB_SENDING) {
		printf("%s: exiting ccb still in transit!\n", sc->sc_dev.dv_xname);
		Debugger();
		return;
	}
#endif
	if ((ccb->flags & CCB_ALLOC) == 0) {
		printf("%s: exiting ccb not allocated!\n", sc->sc_dev.dv_xname);
		Debugger();
		return;
	}
	if (xs->error == XS_NOERROR) {
		if (ccb->host_stat != BT_OK) {
			switch (ccb->host_stat) {
			case BT_SEL_TIMEOUT:	/* No response */
				xs->error = XS_SELTIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				printf("%s: host_stat %x\n",
				    sc->sc_dev.dv_xname, ccb->host_stat);
				xs->error = XS_DRIVER_STUFFUP;
				break;
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
				    sc->sc_dev.dv_xname, ccb->target_stat);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		} else
			xs->resid = 0;
	}
	bt_free_ccb(sc, ccb);
	xs->flags |= ITSDONE;
	scsi_done(xs);
}

/*
 * Find the board and find it's irq/drq
 */
int
bt_find(ia, sc)
	struct isa_attach_args *ia;
	struct bt_softc *sc;
{
	int iobase = ia->ia_iobase;
	int i;
	u_char sts;
	struct bt_extended_inquire inquire;
	struct bt_config config;
#if NAHA > 0
	struct bt_digit digit;
#endif
	int irq, drq;

	/*
	 * reset board, If it doesn't respond, assume
	 * that it's not there.. good for the probe
	 */

	outb(iobase + BT_CTRL_PORT, BT_CTRL_HRST | BT_CTRL_SRST);

	delay(100);
	for (i = BT_RESET_TIMEOUT; i; i--) {
		sts = inb(iobase + BT_STAT_PORT);
		if (sts == (BT_STAT_IDLE | BT_STAT_INIT))
			break;
		delay(1000);
	}
	if (!i) {
#ifdef BTDEBUG
		if (bt_debug)
			printf("bt_find: No answer from buslogic board\n");
#endif /* BTDEBUG */
		return 1;
	}

	/*
	 * Check that we actually know how to use this board.
	 */
	delay(1000);
	bzero(&inquire, sizeof inquire);
	inquire.cmd.opcode = BT_INQUIRE_EXTENDED;
	inquire.cmd.len = sizeof(inquire.reply);
	bt_cmd(iobase, sc, sizeof(inquire.cmd), (u_char *)&inquire.cmd,
	    sizeof(inquire.reply), (u_char *)&inquire.reply);
	switch (inquire.reply.bus_type) {
	case BT_BUS_TYPE_24BIT:
	case BT_BUS_TYPE_32BIT:
		break;
	case BT_BUS_TYPE_MCA:
		/* We don't grok MicroChannel (yet). */
		return 1;
	default:
		if (inquire.reply.bus_type != 'F')
			printf("bt_find: illegal bus type %c\n",
			    inquire.reply.bus_type);
		return 1;
	}

#if NAHA > 0
	/* Adaptec 1542 cards do not support this */
	digit.reply.digit = '@';
	digit.cmd.opcode = BT_INQUIRE_REVISION_3;
	bt_cmd(iobase, sc, sizeof(digit.cmd), (u_char *)&digit.cmd,
	    sizeof(digit.reply), (u_char *)&digit.reply);
	if (digit.reply.digit == '@')
		return 1;
#endif

	/*
	 * Assume we have a board at this stage setup dma channel from
	 * jumpers and save int level
	 */
	delay(1000);
	config.cmd.opcode = BT_INQUIRE_CONFIG;
	bt_cmd(iobase, sc, sizeof(config.cmd), (u_char *)&config.cmd,
	    sizeof(config.reply), (u_char *)&config.reply);
	switch (config.reply.chan) {
	case EISADMA:
		drq = DRQUNK;
		break;
	case CHAN0:
		drq = 0;
		break;
	case CHAN5:
		drq = 5;
		break;
	case CHAN6:
		drq = 6;
		break;
	case CHAN7:
		drq = 7;
		break;
	default:
		printf("bt_find: illegal drq setting %x\n", config.reply.chan);
		return 1;
	}

	switch (config.reply.intr) {
	case INT9:
		irq = 9;
		break;
	case INT10:
		irq = 10;
		break;
	case INT11:
		irq = 11;
		break;
	case INT12:
		irq = 12;
		break;
	case INT14:
		irq = 14;
		break;
	case INT15:
		irq = 15;
		break;
	default:
		printf("bt_find: illegal irq setting %x\n", config.reply.intr);
		return 1;
	}

	if (sc != NULL) {
		/* who are we on the scsi bus? */
		sc->sc_scsi_dev = config.reply.scsi_dev;

		sc->sc_iobase = iobase;
		sc->sc_irq = irq;
		sc->sc_drq = drq;
	} else {
		if (ia->ia_irq == IRQUNK)
			ia->ia_irq = irq;
		else if (ia->ia_irq != irq)
			return 1;
		if (ia->ia_drq == DRQUNK)
			ia->ia_drq = drq;
		else if (ia->ia_drq != drq)
			return 1;
	}

#if NAHA > 0
	/* XXXX To avoid conflicting with the aha1542 probe */
	btports[nbtports++] = iobase;
#endif
	return 0;
}

/*
 * Start the board, ready for normal operation
 */
void
bt_init(sc)
	struct bt_softc *sc;
{
	int iobase = sc->sc_iobase;
	struct bt_devices devices;
	struct bt_setup setup;
	struct bt_mailbox mailbox;
	struct bt_period period;
	int i;

	/* Enable round-robin scheme - appeared at firmware rev. 3.31. */
	if (strcmp(sc->sc_firmware, "3.31") >= 0) {
		struct bt_toggle toggle;

		toggle.cmd.opcode = BT_ROUND_ROBIN;
		toggle.cmd.enable = 1;
		bt_cmd(iobase, sc, sizeof(toggle.cmd), (u_char *)&toggle.cmd,
		    0, (u_char *)0);
	}

	/* Inquire Installed Devices (to force synchronous negotiation). */
	devices.cmd.opcode = BT_INQUIRE_DEVICES;
	bt_cmd(iobase, sc, sizeof(devices.cmd), (u_char *)&devices.cmd,
	    sizeof(devices.reply), (u_char *)&devices.reply);

	/* Obtain setup information from. */
	setup.cmd.opcode = BT_INQUIRE_SETUP;
	setup.cmd.len = sizeof(setup.reply);
	bt_cmd(iobase, sc, sizeof(setup.cmd), (u_char *)&setup.cmd,
	    sizeof(setup.reply), (u_char *)&setup.reply);

	printf("%s: %s, %s\n",
	    sc->sc_dev.dv_xname,
	    setup.reply.sync_neg ? "sync" : "async",
	    setup.reply.parity ? "parity" : "no parity");

	for (i = 0; i < 8; i++)
		period.reply.period[i] = setup.reply.sync[i].period * 5 + 20;

	if (sc->sc_firmware[0] >= '3') {
		period.cmd.opcode = BT_INQUIRE_PERIOD;
		period.cmd.len = sizeof(period.reply);
		bt_cmd(iobase, sc, sizeof(period.cmd), (u_char *)&period.cmd,
		    sizeof(period.reply), (u_char *)&period.reply);
	}

	for (i = 0; i < 8; i++) {
		if (!setup.reply.sync[i].valid ||
		    (!setup.reply.sync[i].offset && !setup.reply.sync[i].period))
			continue;
		printf("%s targ %d: sync, offset %d, period %dnsec\n",
		    sc->sc_dev.dv_xname, i,
		    setup.reply.sync[i].offset, period.reply.period[i] * 10);
	}

	/*
	 * Set up initial mail box for round-robin operation.
	 */
	for (i = 0; i < BT_MBX_SIZE; i++) {
		wmbx->mbo[i].cmd = BT_MBO_FREE;
		wmbx->mbi[i].stat = BT_MBI_FREE;
	}
	wmbx->cmbo = wmbx->tmbo = &wmbx->mbo[0];
	wmbx->tmbi = &wmbx->mbi[0];
	sc->sc_mbofull = 0;

	/* Initialize mail box. */
	mailbox.cmd.opcode = BT_MBX_INIT_EXTENDED;
	mailbox.cmd.nmbx = BT_MBX_SIZE;
	ltophys(KVTOPHYS(wmbx), mailbox.cmd.addr);
	bt_cmd(iobase, sc, sizeof(mailbox.cmd), (u_char *)&mailbox.cmd,
	    0, (u_char *)0);
}

void
bt_inquire_setup_information(sc)
	struct bt_softc *sc;
{
	int iobase = sc->sc_iobase;
	struct bt_model model;
	struct bt_revision revision;
	struct bt_digit digit;
	char *p;

	/*
	 * Get the firmware revision.
	 */
	p = sc->sc_firmware;
	revision.cmd.opcode = BT_INQUIRE_REVISION;
	bt_cmd(iobase, sc, sizeof(revision.cmd), (u_char *)&revision.cmd,
	    sizeof(revision.reply), (u_char *)&revision.reply);
	*p++ = revision.reply.firm_revision;
	*p++ = '.';
	*p++ = revision.reply.firm_version;
	digit.cmd.opcode = BT_INQUIRE_REVISION_3;
	bt_cmd(iobase, sc, sizeof(digit.cmd), (u_char *)&digit.cmd,
	    sizeof(digit.reply), (u_char *)&digit.reply);
	*p++ = digit.reply.digit;
	if (revision.reply.firm_revision >= '3' ||
	    (revision.reply.firm_revision == '3' && revision.reply.firm_version >= '3')) {
		digit.cmd.opcode = BT_INQUIRE_REVISION_4;
		bt_cmd(iobase, sc, sizeof(digit.cmd), (u_char *)&digit.cmd,
		    sizeof(digit.reply), (u_char *)&digit.reply);
		*p++ = digit.reply.digit;
	}
	while (p > sc->sc_firmware && (p[-1] == ' ' || p[-1] == '\0'))
		p--;
	*p = '\0';

	/*
	 * Get the model number.
	 */
	if (revision.reply.firm_revision >= '3') {
		p = sc->sc_model;
		model.cmd.opcode = BT_INQUIRE_MODEL;
		model.cmd.len = sizeof(model.reply);
		bt_cmd(iobase, sc, sizeof(model.cmd), (u_char *)&model.cmd,
		    sizeof(model.reply), (u_char *)&model.reply);
		*p++ = model.reply.id[0];
		*p++ = model.reply.id[1];
		*p++ = model.reply.id[2];
		*p++ = model.reply.id[3];
		while (p > sc->sc_model && (p[-1] == ' ' || p[-1] == '\0'))
			p--;
		*p++ = model.reply.version[0];
		*p++ = model.reply.version[1];
		while (p > sc->sc_model && (p[-1] == ' ' || p[-1] == '\0'))
			p--;
		*p = '\0';
	} else
		strcpy(sc->sc_model, "542B");

	printf(": model BT-%s, firmware %s\n", sc->sc_model, sc->sc_firmware);
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
	struct bt_softc *sc = sc_link->adapter_softc;
	struct bt_ccb *ccb;
	struct bt_scat_gath *sg;
	int seg;		/* scatter gather seg being worked on */
	u_long thiskv, thisphys, nextphys;
	int bytes_this_seg, bytes_this_page, datalen, flags;
#ifdef TFS
	struct iovec *iovp;
#endif
	int s;

	SC_DEBUG(sc_link, SDEV_DB2, ("bt_scsi_cmd\n"));
	/*
	 * get a ccb to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if ((ccb = bt_get_ccb(sc, flags)) == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return TRY_AGAIN_LATER;
	}
	ccb->xs = xs;
	ccb->timeout = xs->timeout;

	/*
	 * Put all the arguments for the xfer in the ccb
	 */
	if (flags & SCSI_RESET) {
		ccb->opcode = BT_RESET_CCB;
		ccb->scsi_cmd_length = 0;
	} else {
		/* can't use S/G if zero length */
		ccb->opcode = (xs->datalen ? BT_INIT_SCAT_GATH_CCB
					   : BT_INITIATOR_CCB);
		bcopy(xs->cmd, &ccb->scsi_cmd,
		    ccb->scsi_cmd_length = xs->cmdlen);
	}

	if (xs->datalen) {
		sg = ccb->scat_gath;
		seg = 0;
#ifdef	TFS
		if (flags & SCSI_DATA_UIO) {
			iovp = ((struct uio *)xs->data)->uio_iov;
			datalen = ((struct uio *)xs->data)->uio_iovcnt;
			xs->datalen = 0;
			while (datalen && seg < BT_NSEG) {
				ltophys(iovp->iov_base, sg->seg_addr);
				ltophys(iovp->iov_len, sg->seg_len);
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
			 * Set up the scatter-gather block.
			 */
			SC_DEBUG(sc_link, SDEV_DB4,
			    ("%d @0x%x:- ", xs->datalen, xs->data));

			datalen = xs->datalen;
			thiskv = (int)xs->data;
			thisphys = KVTOPHYS(thiskv);

			while (datalen && seg < BT_NSEG) {
				bytes_this_seg = 0;

				/* put in the base address */
				ltophys(thisphys, sg->seg_addr);

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
				ltophys(bytes_this_seg, sg->seg_len);
				sg++;
				seg++;
			}
		}
		/* end of iov/kv decision */
		SC_DEBUGN(sc_link, SDEV_DB4, ("\n"));
		if (datalen) {
			/*
			 * there's still data, must have run out of segs!
			 */
			printf("%s: bt_scsi_cmd, more than %d dma segs\n",
			    sc->sc_dev.dv_xname, BT_NSEG);
			goto bad;
		}
		ltophys(KVTOPHYS(ccb->scat_gath), ccb->data_addr);
		ltophys(seg * sizeof(struct bt_scat_gath), ccb->data_length);
	} else {		/* No data xfer, use non S/G values */
		ltophys(0, ccb->data_addr);
		ltophys(0, ccb->data_length);
	}

	ccb->data_out = 0;
	ccb->data_in = 0;
	ccb->target = sc_link->target;
	ccb->lun = sc_link->lun;
	ltophys(KVTOPHYS(&ccb->scsi_sense), ccb->sense_ptr);
	ccb->req_sense_length = sizeof(ccb->scsi_sense);
	ccb->host_stat = 0x00;
	ccb->target_stat = 0x00;
	ccb->link_id = 0;
	ltophys(0, ccb->link_addr);

	s = splbio();
	bt_queue_ccb(sc, ccb);
	splx(s);

	/*
	 * Usually return SUCCESSFULLY QUEUED
	 */
	SC_DEBUG(sc_link, SDEV_DB3, ("cmd_sent\n"));
	if ((flags & SCSI_POLL) == 0)
		return SUCCESSFULLY_QUEUED;

	/*
	 * If we can't use interrupts, poll on completion
	 */
	if (bt_poll(sc, xs, ccb->timeout)) {
		bt_timeout(ccb);
		if (bt_poll(sc, xs, ccb->timeout))
			bt_timeout(ccb);
	}
	return COMPLETE;

bad:
	xs->error = XS_DRIVER_STUFFUP;
	bt_free_ccb(sc, ccb);
	return COMPLETE;
}

/*
 * Poll a particular unit, looking for a particular xs
 */
int
bt_poll(sc, xs, count)
	struct bt_softc *sc;
	struct scsi_xfer *xs;
	int count;
{
	int iobase = sc->sc_iobase;

	/* timeouts are in msec, so we loop in 1000 usec cycles */
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (inb(iobase + BT_INTR_PORT) & BT_INTR_ANYINTR)
			btintr(sc);
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
	struct bt_softc *sc = sc_link->adapter_softc;
	int s;

	sc_print_addr(sc_link);
	printf("timed out");

	s = splbio();

#ifdef BTDIAG
	/*
	 * If the ccb's mbx is not free, then the board has gone Far East?
	 */
	bt_collect_mbo(sc);
	if (ccb->flags & CCB_SENDING) {
		printf("%s: not taking commands!\n", sc->sc_dev.dv_xname);
		Debugger();
	}
#endif

	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (ccb->flags & CCB_ABORT) {
		/* abort timed out */
		printf(" AGAIN\n");
		/* XXX Must reset! */
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		ccb->xs->error = XS_TIMEOUT;
		ccb->timeout = BT_ABORT_TIMEOUT;
		ccb->flags |= CCB_ABORT;
		bt_queue_ccb(sc, ccb);
	}

	splx(s);
}
