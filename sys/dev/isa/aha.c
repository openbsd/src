/*	$OpenBSD: aha.c,v 1.31 1998/08/13 04:36:50 downsj Exp $	*/
/*	$NetBSD: aha.c,v 1.11 1996/05/12 23:51:23 mycroft Exp $	*/

#undef AHADIAG
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
#include <dev/isa/ahareg.h>

#ifndef DDB
#define Debugger() panic("should call debugger here (aha1542.c)")
#endif /* ! DDB */

/* XXX fixme:
 * on i386 at least, xfers to/from user memory
 * cannot be serviced at interrupt time.
 */
#ifdef i386
#define VOLATILE_XS(xs) \
	((xs)->datalen > 0 && (xs)->bp == NULL && \
	((xs)->flags & SCSI_POLL) == 0)
#else
#define VOLATILE_XS(xs)        0
#endif

/*
 * Mail box defs  etc.
 * these could be bigger but we need the aha_softc to fit on a single page..
 */
#define AHA_MBX_SIZE	16	/* mail box size */

#define	AHA_CCB_MAX	16	/* store up to 32 CCBs at one time */
#define	CCB_HASH_SIZE	16	/* hash table size for phystokv */
#define	CCB_HASH_SHIFT	9
#define	CCB_HASH(x)	((((long)(x))>>CCB_HASH_SHIFT) & (CCB_HASH_SIZE - 1))

#define aha_nextmbx(wmb, mbx, mbio) \
	if ((wmb) == &(mbx)->mbio[AHA_MBX_SIZE - 1])	\
		(wmb) = &(mbx)->mbio[0];		\
	else						\
		(wmb)++;

struct aha_mbx {
	struct aha_mbx_out mbo[AHA_MBX_SIZE];
	struct aha_mbx_in mbi[AHA_MBX_SIZE];
	struct aha_mbx_out *cmbo;	/* Collection Mail Box out */
	struct aha_mbx_out *tmbo;	/* Target Mail Box out */
	struct aha_mbx_in *tmbi;	/* Target Mail Box in */
};

struct aha_softc {
	struct device sc_dev;
	struct isadev sc_id;
	void *sc_ih;

	int sc_iobase;
	int sc_irq, sc_drq;

	char sc_model[18],
	     sc_firmware[4];

	struct aha_mbx sc_mbx;		/* all the mailboxes */
#define	wmbx	(&sc->sc_mbx)
	struct aha_ccb *sc_ccbhash[CCB_HASH_SIZE];
	TAILQ_HEAD(, aha_ccb) sc_free_ccb, sc_waiting_ccb;
	int sc_numccbs, sc_mbofull;
	int sc_scsi_dev;		/* our scsi id */
	struct scsi_link sc_link;
};

#ifdef AHADEBUG
int	aha_debug = 1;
#endif /* AHADEBUG */

int aha_cmd __P((int, struct aha_softc *, int, u_char *, int, u_char *));
integrate void aha_finish_ccbs __P((struct aha_softc *));
int ahaintr __P((void *));
integrate void aha_reset_ccb __P((struct aha_softc *, struct aha_ccb *));
void aha_free_ccb __P((struct aha_softc *, struct aha_ccb *));
integrate void aha_init_ccb __P((struct aha_softc *, struct aha_ccb *));
struct aha_ccb *aha_get_ccb __P((struct aha_softc *, int));
struct aha_ccb *aha_ccb_phys_kv __P((struct aha_softc *, u_long));
void aha_queue_ccb __P((struct aha_softc *, struct aha_ccb *));
void aha_collect_mbo __P((struct aha_softc *));
void aha_start_ccbs __P((struct aha_softc *));
void aha_done __P((struct aha_softc *, struct aha_ccb *));
int aha_find __P((struct isa_attach_args *, struct aha_softc *));
void aha_init __P((struct aha_softc *));
void aha_inquire_setup_information __P((struct aha_softc *));
void ahaminphys __P((struct buf *));
int aha_scsi_cmd __P((struct scsi_xfer *));
int aha_poll __P((struct aha_softc *, struct scsi_xfer *, int));
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

struct cfattach aha_ca = {
	sizeof(struct aha_softc), ahaprobe, ahaattach
};

struct cfdriver aha_cd = {
	NULL, "aha", DV_DULL
};

#define AHA_RESET_TIMEOUT	2000	/* time to wait for reset (mSec) */
#define	AHA_ABORT_TIMEOUT	2000	/* time to wait for abort (mSec) */

#include "bt.h"

/*
 * aha_cmd(iobase, sc, icnt, ibuf, ocnt, obuf)
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
aha_cmd(iobase, sc, icnt, ibuf, ocnt, obuf)
	int iobase;
	struct aha_softc *sc;
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
		name = "(aha probe)";

	/*
	 * Calculate a reasonable timeout for the command.
	 */
	switch (opcode) {
	case AHA_INQUIRE_DEVICES:
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
	if (opcode != AHA_MBO_INTR_EN) {
		for (i = 20000; i; i--) {	/* 1 sec? */
			sts = inb(iobase + AHA_STAT_PORT);
			if (sts & AHA_STAT_IDLE)
				break;
			delay(50);
		}
		if (!i) {
			printf("%s: aha_cmd, host not idle(0x%x)\n",
			    name, sts);
			return ENXIO;
		}
	}
	/*
	 * Now that it is idle, if we expect output, preflush the
	 * queue feeding to us.
	 */
	if (ocnt) {
		while ((inb(iobase + AHA_STAT_PORT)) & AHA_STAT_DF)
			inb(iobase + AHA_DATA_PORT);
	}
	/*
	 * Output the command and the number of arguments given
	 * for each byte, first check the port is empty.
	 */
	while (icnt--) {
		for (i = wait; i; i--) {
			sts = inb(iobase + AHA_STAT_PORT);
			if (!(sts & AHA_STAT_CDF))
				break;
			delay(50);
		}
		if (!i) {
			if (opcode != AHA_INQUIRE_REVISION)
				printf("%s: aha_cmd, cmd/data port full\n",
				    name);
			outb(iobase + AHA_CTRL_PORT, AHA_CTRL_SRST);
			return ENXIO;
		}
		outb(iobase + AHA_CMD_PORT, *ibuf++);
	}
	/*
	 * If we expect input, loop that many times, each time,
	 * looking for the data register to have valid data
	 */
	while (ocnt--) {
		for (i = wait; i; i--) {
			sts = inb(iobase + AHA_STAT_PORT);
			if (sts & AHA_STAT_DF)
				break;
			delay(50);
		}
		if (!i) {
			if (opcode != AHA_INQUIRE_REVISION)
				printf("%s: aha_cmd, cmd/data port empty %d\n",
				    name, ocnt);
			outb(iobase + AHA_CTRL_PORT, AHA_CTRL_SRST);
			return ENXIO;
		}
		*obuf++ = inb(iobase + AHA_DATA_PORT);
	}
	/*
	 * Wait for the board to report a finished instruction.
	 * We may get an extra interrupt for the HACC signal, but this is
	 * unimportant.
	 */
	if (opcode != AHA_MBO_INTR_EN) {
		for (i = 20000; i; i--) {	/* 1 sec? */
			sts = inb(iobase + AHA_INTR_PORT);
			/* XXX Need to save this in the interrupt handler? */
			if (sts & AHA_INTR_HACC)
				break;
			delay(50);
		}
		if (!i) {
			printf("%s: aha_cmd, host not finished(0x%x)\n",
			    name, sts);
			return ENXIO;
		}
	}
	outb(iobase + AHA_CTRL_PORT, AHA_CTRL_IRST);
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
	register struct isa_attach_args *ia = aux;
#if NBT > 0
	extern int btports[], nbtports;
	int i;

	for (i = 0; i < nbtports; i++)
		if (btports[i] == ia->ia_iobase)
			return 0;
#endif

#ifdef NEWCONFIG
	if (ia->ia_iobase == IOBASEUNK)
		return 0;
#endif

	/* See if there is a unit at this location. */
	if (aha_find(ia, NULL) != 0)
		return 0;

	ia->ia_msize = 0;
	ia->ia_iosize = 4;
	/* IRQ and DRQ set by aha_find(). */
	return 1;
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
	struct aha_softc *sc = (void *)self;

	if (aha_find(ia, sc) != 0)
		panic("ahaattach: aha_find of %s failed", self->dv_xname);
	sc->sc_iobase = ia->ia_iobase;

	if (sc->sc_drq != DRQUNK)
		isadma_cascade(sc->sc_drq);

	aha_inquire_setup_information(sc);
	aha_init(sc);
	TAILQ_INIT(&sc->sc_free_ccb);
	TAILQ_INIT(&sc->sc_waiting_ccb);

	/*
	 * fill in the prototype scsi_link.
	 */
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->sc_scsi_dev;
	sc->sc_link.adapter = &aha_switch;
	sc->sc_link.device = &aha_dev;
	sc->sc_link.openings = 2;

#ifdef NEWCONFIG
	isa_establish(&sc->sc_id, &sc->sc_dev);
#endif
	sc->sc_ih = isa_intr_establish(ia->ia_ic, sc->sc_irq, IST_EDGE,
	    IPL_BIO, ahaintr, sc, sc->sc_dev.dv_xname);

	/*
	 * ask the adapter what subunits are present
	 */
	config_found(self, &sc->sc_link, scsiprint);
}

integrate void
aha_finish_ccbs(sc)
	struct aha_softc *sc;
{
	struct aha_mbx_in *wmbi;
	struct aha_ccb *ccb;
	int i;

	wmbi = wmbx->tmbi;

	if (wmbi->stat == AHA_MBI_FREE) {
		for (i = 0; i < AHA_MBX_SIZE; i++) {
			if (wmbi->stat != AHA_MBI_FREE) {
				printf("%s: mbi not in round-robin order\n",
				    sc->sc_dev.dv_xname);
				goto AGAIN;
			}
			aha_nextmbx(wmbi, wmbx, mbi);
		}
#ifdef AHADIAGnot
		printf("%s: mbi interrupt with no full mailboxes\n",
		    sc->sc_dev.dv_xname);
#endif
		return;
	}

AGAIN:
	do {
		ccb = aha_ccb_phys_kv(sc, phystol(wmbi->ccb_addr));
		if (!ccb) {
			printf("%s: bad mbi ccb pointer; skipping\n",
			    sc->sc_dev.dv_xname);
			goto next;
		}

#ifdef AHADEBUG
		if (aha_debug) {
			u_char *cp = (u_char*)&ccb->scsi_cmd;
			printf("op=%x %x %x %x %x %x\n",
			    cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
			printf("stat %x for mbi addr = 0x%08x, ",
			    wmbi->stat, wmbi);
			printf("ccb addr = 0x%x\n", ccb);
		}
#endif /* AHADEBUG */

		switch (wmbi->stat) {
		case AHA_MBI_OK:
		case AHA_MBI_ERROR:
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

		case AHA_MBI_ABORT:
		case AHA_MBI_UNKNOWN:
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

		untimeout(aha_timeout, ccb);
		isadma_copyfrombuf((caddr_t)ccb, CCB_PHYS_SIZE,
		    1, ccb->ccb_phys);
		aha_done(sc, ccb);

	next:
		wmbi->stat = AHA_MBI_FREE;
		aha_nextmbx(wmbi, wmbx, mbi);
	} while (wmbi->stat != AHA_MBI_FREE);

	wmbx->tmbi = wmbi;
}

/*
 * Catch an interrupt from the adaptor
 */
int
ahaintr(arg)
	void *arg;
{
	struct aha_softc *sc = arg;
	int iobase = sc->sc_iobase;
	u_char sts;

#ifdef AHADEBUG
	if (aha_debug)
		printf("%s: ahaintr ", sc->sc_dev.dv_xname);
#endif /*AHADEBUG */

	/*
	 * First acknowlege the interrupt, Then if it's not telling about
	 * a completed operation just return.
	 */
	sts = inb(iobase + AHA_INTR_PORT);
	if ((sts & AHA_INTR_ANYINTR) == 0)
		return 0;
	outb(iobase + AHA_CTRL_PORT, AHA_CTRL_IRST);

#ifdef AHADIAG
	/* Make sure we clear CCB_SENDING before finishing a CCB. */
	aha_collect_mbo(sc);
#endif

	/* Mail box out empty? */
	if (sts & AHA_INTR_MBOA) {
		struct aha_toggle toggle;

		toggle.cmd.opcode = AHA_MBO_INTR_EN;
		toggle.cmd.enable = 0;
		aha_cmd(iobase, sc, sizeof(toggle.cmd), (u_char *)&toggle.cmd,
		    0, (u_char *)0);
		aha_start_ccbs(sc);
	}

	/* Mail box in full? */
	if (sts & AHA_INTR_MBIF)
		aha_finish_ccbs(sc);

	return 1;
}

integrate void
aha_reset_ccb(sc, ccb)
	struct aha_softc *sc;
	struct aha_ccb *ccb;
{

	ccb->flags = 0;
}

/*
 * A ccb is put onto the free list.
 */
void
aha_free_ccb(sc, ccb)
	struct aha_softc *sc;
	struct aha_ccb *ccb;
{
	int s, hashnum;
	struct aha_ccb **hashccb;

	s = splbio();

	if (ccb->ccb_phys[0].addr)
	        isadma_unmap((caddr_t)ccb, CCB_PHYS_SIZE, 1, ccb->ccb_phys);

	/* remove from hash table */

	hashnum = CCB_HASH(ccb->ccb_phys[0].addr);
	hashccb = &sc->sc_ccbhash[hashnum];

	while (*hashccb) {
		if ((*hashccb)->ccb_phys[0].addr == ccb->ccb_phys[0].addr) {
			*hashccb = (*hashccb)->nexthash;
			break;
		}
		hashccb = &(*hashccb)->nexthash;
	}

	aha_reset_ccb(sc, ccb);
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
aha_init_ccb(sc, ccb)
	struct aha_softc *sc;
	struct aha_ccb *ccb;
{
	bzero(ccb, sizeof(struct aha_ccb));
	aha_reset_ccb(sc, ccb);
}

/*
 * Get a free ccb
 *
 * If there are none, see if we can allocate a new one.  If so, put it in
 * the hash table too otherwise either return an error or sleep.
 */
struct aha_ccb *
aha_get_ccb(sc, flags)
	struct aha_softc *sc;
	int flags;
{
	struct aha_ccb *ccb;
	int hashnum, mflags, s;

	s = splbio();

	if (flags & SCSI_NOSLEEP)
		mflags = ISADMA_MAP_BOUNCE;
	else
		mflags = ISADMA_MAP_BOUNCE | ISADMA_MAP_WAITOK;

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
		if (sc->sc_numccbs < AHA_CCB_MAX) {
			ccb = (struct aha_ccb *) malloc(sizeof(struct aha_ccb),
			    M_TEMP, M_NOWAIT);
			if (!ccb) {
				printf("%s: can't malloc ccb\n",
				    sc->sc_dev.dv_xname);
				goto out;
			}
			aha_init_ccb(sc, ccb);
			sc->sc_numccbs++;
			break;
		}
		if ((flags & SCSI_NOSLEEP) != 0)
			goto out;
		tsleep(&sc->sc_free_ccb, PRIBIO, "ahaccb", 0);
	}

	ccb->flags |= CCB_ALLOC;

	if (isadma_map((caddr_t)ccb, CCB_PHYS_SIZE, ccb->ccb_phys,
	    mflags | ISADMA_MAP_CONTIG) == 1) {
		hashnum = CCB_HASH(ccb->ccb_phys[0].addr);
		ccb->nexthash = sc->sc_ccbhash[hashnum];
		sc->sc_ccbhash[hashnum] = ccb;
	} else {
		ccb->ccb_phys[0].addr = 0;
		aha_free_ccb(sc, ccb);
		ccb = 0;
	}

out:
	splx(s);
	return (ccb);
}

/*
 * Given a physical address, find the ccb that it corresponds to.
 */
struct aha_ccb *
aha_ccb_phys_kv(sc, ccb_phys)
	struct aha_softc *sc;
	u_long ccb_phys;
{
	int hashnum = CCB_HASH(ccb_phys);
	struct aha_ccb *ccb = sc->sc_ccbhash[hashnum];

	while (ccb) {
		if (ccb->ccb_phys[0].addr == ccb_phys)
			break;
		ccb = ccb->nexthash;
	}
	return ccb;
}

/*
 * Queue a CCB to be sent to the controller, and send it if possible.
 */
void
aha_queue_ccb(sc, ccb)
	struct aha_softc *sc;
	struct aha_ccb *ccb;
{

	TAILQ_INSERT_TAIL(&sc->sc_waiting_ccb, ccb, chain);
	aha_start_ccbs(sc);
}

/*
 * Garbage collect mailboxes that are no longer in use.
 */
void
aha_collect_mbo(sc)
	struct aha_softc *sc;
{
	struct aha_mbx_out *wmbo;	/* Mail Box Out pointer */
#ifdef AHADIAG
	struct aha_ccb *ccb;
#endif

	wmbo = wmbx->cmbo;

	while (sc->sc_mbofull > 0) {
		if (wmbo->cmd != AHA_MBO_FREE)
			break;

#ifdef AHADIAG
		ccb = aha_ccb_phys_kv(sc, phystol(wmbo->ccb_addr));
		if (!ccb) {
			printf("%s: bad mbo ccb pointer; skipping\n",
			    sc->sc_dev.dv_xname);
		} else
			ccb->flags &= ~CCB_SENDING;
#endif

		--sc->sc_mbofull;
		aha_nextmbx(wmbo, wmbx, mbo);
	}

	wmbx->cmbo = wmbo;
}

/*
 * Send as many CCBs as we have empty mailboxes for.
 */
void
aha_start_ccbs(sc)
	struct aha_softc *sc;
{
	int iobase = sc->sc_iobase;
	struct aha_mbx_out *wmbo;	/* Mail Box Out pointer */
	struct aha_ccb *ccb;

	wmbo = wmbx->tmbo;

	while ((ccb = sc->sc_waiting_ccb.tqh_first) != NULL) {
		if (sc->sc_mbofull >= AHA_MBX_SIZE) {
			aha_collect_mbo(sc);
			if (sc->sc_mbofull >= AHA_MBX_SIZE) {
				struct aha_toggle toggle;

				toggle.cmd.opcode = AHA_MBO_INTR_EN;
				toggle.cmd.enable = 1;
				aha_cmd(iobase, sc, sizeof(toggle.cmd),
				    (u_char *)&toggle.cmd, 0, (u_char *)0);
				break;
			}
		}

		TAILQ_REMOVE(&sc->sc_waiting_ccb, ccb, chain);
#ifdef AHADIAG
		ccb->flags |= CCB_SENDING;
#endif

		/* Link ccb to mbo. */
		isadma_copytobuf((caddr_t)ccb, CCB_PHYS_SIZE,
		    1, ccb->ccb_phys);
		ltophys(ccb->ccb_phys[0].addr, wmbo->ccb_addr);
		if (ccb->flags & CCB_ABORT)
			wmbo->cmd = AHA_MBO_ABORT;
		else
			wmbo->cmd = AHA_MBO_START;

		/* Tell the card to poll immediately. */
		outb(iobase + AHA_CMD_PORT, AHA_START_SCSI);

		if ((ccb->xs->flags & SCSI_POLL) == 0)
			timeout(aha_timeout, ccb, (ccb->timeout * hz) / 1000);

		++sc->sc_mbofull;
		aha_nextmbx(wmbo, wmbx, mbo);
	}

	wmbx->tmbo = wmbo;
}

/*
 * We have a ccb which has been processed by the
 * adaptor, now we look to see how the operation
 * went. Wake up the owner if waiting
 */
void
aha_done(sc, ccb)
	struct aha_softc *sc;
	struct aha_ccb *ccb;
{
	struct scsi_sense_data *s1, *s2;
	struct scsi_xfer *xs = ccb->xs;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("aha_done\n"));
	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
#ifdef AHADIAG
	if (ccb->flags & CCB_SENDING) {
		printf("%s: exiting ccb still in transit!\n",
		    sc->sc_dev.dv_xname);
		Debugger();
		return;
	}
#endif
	if ((ccb->flags & CCB_ALLOC) == 0) {
		printf("%s: exiting ccb not allocated!\n",
		    sc->sc_dev.dv_xname);
		Debugger();
		return;
	}
	if (xs->error == XS_NOERROR) {
		if (ccb->host_stat != AHA_OK) {
			switch (ccb->host_stat) {
			case AHA_SEL_TIMEOUT:	/* No response */
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
				s1 = (struct scsi_sense_data *)
				    (((char *)(&ccb->scsi_cmd)) +
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
				    sc->sc_dev.dv_xname, ccb->target_stat);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		} else
			xs->resid = 0;
	}
	xs->flags |= ITSDONE;

	if (VOLATILE_XS(xs)) {
		wakeup(ccb);
		return;
	}

	if (ccb->data_nseg) {
		if (xs->flags & SCSI_DATA_IN)
			isadma_copyfrombuf(xs->data, xs->datalen,
			    ccb->data_nseg, ccb->data_phys);
		isadma_unmap(xs->data, xs->datalen,
		    ccb->data_nseg, ccb->data_phys);
	}
	aha_free_ccb(sc, ccb);
	scsi_done(xs);
}

/*
 * Find the board and find its irq/drq
 */
int
aha_find(ia, sc)
	struct isa_attach_args *ia;
	struct aha_softc *sc;
{
	int iobase = ia->ia_iobase;
	int i;
	u_char sts;
	struct aha_config config;
	int irq, drq;

	/*
	 * reset board, If it doesn't respond, assume
	 * that it's not there.. good for the probe
	 */

	outb(iobase + AHA_CTRL_PORT, AHA_CTRL_HRST | AHA_CTRL_SRST);

	delay(100);
	for (i = AHA_RESET_TIMEOUT; i; i--) {
		sts = inb(iobase + AHA_STAT_PORT);
		if (sts == (AHA_STAT_IDLE | AHA_STAT_INIT))
			break;
		delay(1000);	/* calibrated in msec */
	}
	if (!i) {
#ifdef AHADEBUG
		if (aha_debug)
			printf("aha_find: No answer from adaptec board\n");
#endif /* AHADEBUG */
		return 1;
	}

	/*
	 * setup dma channel from jumpers and save int
	 * level
	 */
	delay(1000);		/* for Bustek 545 */
	config.cmd.opcode = AHA_INQUIRE_CONFIG;
	aha_cmd(iobase, sc, sizeof(config.cmd), (u_char *)&config.cmd,
	    sizeof(config.reply), (u_char *)&config.reply);
	switch (config.reply.chan) {
	case EISADMA:
		drq = DRQUNK;	/* for EISA/VLB/PCI clones */
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
		printf("aha_find: illegal drq setting %x\n",
		    config.reply.chan);
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
		printf("aha_find: illegal irq setting %x\n",
		    config.reply.intr);
		return EIO;
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

	return 0;
}

/*
 * Start the board, ready for normal operation
 */
void
aha_init(sc)
	struct aha_softc *sc;
{
	int iobase = sc->sc_iobase;
	struct aha_devices devices;
	struct aha_setup setup;
	struct aha_mailbox mailbox;
	struct isadma_seg mbx_phys[1];
	int i;

	/*
	 * XXX
	 * If we are a 1542C or later, disable the extended BIOS so that the
	 * mailbox interface is unlocked.
	 * No need to check the extended BIOS flags as some of the
	 * extensions that cause us problems are not flagged in that byte.
	 */
	if (!strncmp(sc->sc_model, "1542C", 5)) {
		struct aha_extbios extbios;
		struct aha_unlock unlock;

		printf("%s: unlocking mailbox interface\n",
		    sc->sc_dev.dv_xname);
		extbios.cmd.opcode = AHA_EXT_BIOS;
		aha_cmd(iobase, sc, sizeof(extbios.cmd),
		    (u_char *)&extbios.cmd, sizeof(extbios.reply),
		    (u_char *)&extbios.reply);

#ifdef AHADEBUG
		printf("%s: flags=%02x, mailboxlock=%02x\n",
		    sc->sc_dev.dv_xname,
		    extbios.reply.flags, extbios.reply.mailboxlock);
#endif /* AHADEBUG */

		unlock.cmd.opcode = AHA_MBX_ENABLE;
		unlock.cmd.junk = 0;
		unlock.cmd.magic = extbios.reply.mailboxlock;
		aha_cmd(iobase, sc, sizeof(unlock.cmd), (u_char *)&unlock.cmd,
		    0, (u_char *)0);
	}

#if 0
	/*
	 * Change the bus on/off times to not clash with other dma users.
	 */
	aha_cmd(sc, 1, 0, 0, 0, AHA_BUS_ON_TIME_SET, 7);
	aha_cmd(sc, 1, 0, 0, 0, AHA_BUS_OFF_TIME_SET, 4);
#endif

	/* Inquire Installed Devices (to force synchronous negotiation). */
	devices.cmd.opcode = AHA_INQUIRE_DEVICES;
	aha_cmd(iobase, sc, sizeof(devices.cmd), (u_char *)&devices.cmd,
	    sizeof(devices.reply), (u_char *)&devices.reply);

	/* Obtain setup information from. */
	setup.cmd.opcode = AHA_INQUIRE_SETUP;
	setup.cmd.len = sizeof(setup.reply);
	aha_cmd(iobase, sc, sizeof(setup.cmd), (u_char *)&setup.cmd,
	    sizeof(setup.reply), (u_char *)&setup.reply);

	printf("%s: %s, %s\n",
	    sc->sc_dev.dv_xname,
	    setup.reply.sync_neg ? "sync" : "async",
	    setup.reply.parity ? "parity" : "no parity");

	for (i = 0; i < 8; i++) {
		if (!setup.reply.sync[i].valid ||
		    (!setup.reply.sync[i].offset &&
		    !setup.reply.sync[i].period))
			continue;
		printf("%s targ %d: sync, offset %d, period %dnsec\n",
		    sc->sc_dev.dv_xname, i, setup.reply.sync[i].offset,
		    setup.reply.sync[i].period * 50 + 200);
	}

	/*
	 * Set up initial mail box for round-robin operation.
	 */
	for (i = 0; i < AHA_MBX_SIZE; i++) {
		wmbx->mbo[i].cmd = AHA_MBO_FREE;
		wmbx->mbi[i].stat = AHA_MBI_FREE;
	}
	wmbx->cmbo = wmbx->tmbo = &wmbx->mbo[0];
	wmbx->tmbi = &wmbx->mbi[0];
	sc->sc_mbofull = 0;

	/* Initialize mail box. */
	mailbox.cmd.opcode = AHA_MBX_INIT;
	mailbox.cmd.nmbx = AHA_MBX_SIZE;
	if (isadma_map((caddr_t)(wmbx), sizeof(struct aha_mbx),
	    mbx_phys, ISADMA_MAP_CONTIG) != 1)
		panic("aha_init: cannot map mail box");
	ltophys(mbx_phys[0].addr, mailbox.cmd.addr);
	aha_cmd(iobase, sc, sizeof(mailbox.cmd), (u_char *)&mailbox.cmd,
	    0, (u_char *)0);
}

void
aha_inquire_setup_information(sc)
	struct aha_softc *sc;
{
	int iobase = sc->sc_iobase;
	struct aha_revision revision;
	u_char sts;
	int i;
	char *p;

	strcpy(sc->sc_model, "unknown");

	/*
	 * Assume we have a board at this stage, do an adapter inquire
	 * to find out what type of controller it is.  If the command
	 * fails, we assume it's either a crusty board or an old 1542
	 * clone, and skip the board-specific stuff.
	 */
	revision.cmd.opcode = AHA_INQUIRE_REVISION;
	if (aha_cmd(iobase, sc, sizeof(revision.cmd), (u_char *)&revision.cmd,
	    sizeof(revision.reply), (u_char *)&revision.reply)) {
		/*
		 * aha_cmd() already started the reset.  It's not clear we
		 * even need to bother here.
		 */
		for (i = AHA_RESET_TIMEOUT; i; i--) {
			sts = inb(iobase + AHA_STAT_PORT);
			if (sts == (AHA_STAT_IDLE | AHA_STAT_INIT))
				break;
			delay(1000);
		}
		if (!i) {
#ifdef AHADEBUG
			printf("aha_init: soft reset failed\n");
#endif /* AHADEBUG */
			return;
		}
#ifdef AHADEBUG
		printf("aha_init: inquire command failed\n");
#endif /* AHADEBUG */
		goto noinquire;
	}

#ifdef AHADEBUG
	printf("%s: inquire %x, %x, %x, %x\n",
	    sc->sc_dev.dv_xname,
	    revision.reply.boardid, revision.reply.spec_opts,
	    revision.reply.revision_1, revision.reply.revision_2);
#endif /* AHADEBUG */

	switch (revision.reply.boardid) {
	case 0x31:
		strcpy(sc->sc_model, "1540");
		break;
	case 0x41:
		strcpy(sc->sc_model, "1540A/1542A/1542B");
		break;
	case 0x42:
		strcpy(sc->sc_model, "1640");
		break;
	case 0x43:
	case 0x44:		/* Is this 1542C or -CF? */
		strcpy(sc->sc_model, "1542C");
		break;
	case 0x45:
		strcpy(sc->sc_model, "1542CF");
		break;
	case 0x46:
		strcpy(sc->sc_model, "1542CP");
		break;
	}

	p = sc->sc_firmware;
	*p++ = revision.reply.revision_1;
	*p++ = '.';
	*p++ = revision.reply.revision_2;
	*p = '\0';

noinquire:
	printf(": model AHA-%s, firmware %s\n", sc->sc_model, sc->sc_firmware);
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
	struct aha_softc *sc = sc_link->adapter_softc;
	struct aha_ccb *ccb;
	struct aha_scat_gath *sg;
	int seg, flags, mflags;
#ifdef	TFS
	struct iovec *iovp;
	int datalen;
#endif
	int s;

	SC_DEBUG(sc_link, SDEV_DB2, ("aha_scsi_cmd\n"));
	/*
	 * get a ccb to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if (flags & SCSI_NOSLEEP)
		mflags = ISADMA_MAP_BOUNCE;
	else
		mflags = ISADMA_MAP_BOUNCE | ISADMA_MAP_WAITOK;
	if ((ccb = aha_get_ccb(sc, flags)) == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return TRY_AGAIN_LATER;
	}
	ccb->xs = xs;
	ccb->timeout = xs->timeout;

	/*
	 * Put all the arguments for the xfer in the ccb
	 */
	if (flags & SCSI_RESET) {
		ccb->opcode = AHA_RESET_CCB;
		ccb->scsi_cmd_length = 0;
	} else {
		/* can't use S/G if zero length */
		ccb->opcode =
		    (xs->datalen ? AHA_INIT_SCAT_GATH_CCB : AHA_INITIATOR_CCB);
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
			while (datalen && seg < AHA_NSEG) {
				ltophys(iovp->iov_base, sg->seg_addr);
				ltophys(iovp->iov_len, sg->seg_len);
				xs->datalen += iovp->iov_len;
				SC_DEBUGN(sc_link, SDEV_DB4, ("UIO(0x%x@0x%x)",
				    iovp->iov_len, iovp->iov_base));
				sg++;
				iovp++;
				seg++;
				datalen--;
			}
		} else
#endif /* TFS */
		{
			/*
			 * Set up the scatter-gather block.
			 */
			ccb->data_nseg = isadma_map(xs->data, xs->datalen,
			    ccb->data_phys, mflags);
			for (seg = 0; seg < ccb->data_nseg; seg++) {
				ltophys(ccb->data_phys[seg].addr,
				    sg[seg].seg_addr);
				ltophys(ccb->data_phys[seg].length,
				    sg[seg].seg_len);
			}
		}
		/* end of iov/kv decision */
		if (ccb->data_nseg == 0) {
			printf("%s: aha_scsi_cmd, cannot map\n",
			    sc->sc_dev.dv_xname);
			goto bad;
		} else if (flags & SCSI_DATA_OUT)
			isadma_copytobuf(xs->data, xs->datalen,
			    ccb->data_nseg, ccb->data_phys);
		ltophys((unsigned)
		    ((struct aha_ccb *)(ccb->ccb_phys[0].addr))->scat_gath,
		    ccb->data_addr);
		ltophys(ccb->data_nseg * sizeof(struct aha_scat_gath),
		    ccb->data_length);
	} else {		/* No data xfer, use non S/G values */
		ltophys(0, ccb->data_addr);
		ltophys(0, ccb->data_length);
	}

	ccb->data_out = 0;
	ccb->data_in = 0;
	ccb->target = sc_link->target;
	ccb->lun = sc_link->lun;
	ccb->req_sense_length = sizeof(ccb->scsi_sense);
	ccb->host_stat = 0x00;
	ccb->target_stat = 0x00;
	ccb->link_id = 0;
	ltophys(0, ccb->link_addr);

	s = splbio();
	aha_queue_ccb(sc, ccb);

	/*
	 * Usually return SUCCESSFULLY QUEUED
	 */
	SC_DEBUG(sc_link, SDEV_DB3, ("cmd_sent\n"));

	if (VOLATILE_XS(xs)) {
		while ((ccb->xs->flags & ITSDONE) == 0) {
			tsleep(ccb, PRIBIO, "ahawait", 0);
		}
		if (ccb->data_nseg) {
			if (flags & SCSI_DATA_IN)
				isadma_copyfrombuf(xs->data, xs->datalen,
				    ccb->data_nseg, ccb->data_phys);
			isadma_unmap(xs->data, xs->datalen,
			    ccb->data_nseg, ccb->data_phys);
		}
		aha_free_ccb(sc, ccb);
		scsi_done(xs);
		splx(s);
		return COMPLETE;
	}
	splx(s);

	if ((flags & SCSI_POLL) == 0)
		return SUCCESSFULLY_QUEUED;

	/*
	 * If we can't use interrupts, poll on completion
	 */
	if (aha_poll(sc, xs, ccb->timeout)) {
		aha_timeout(ccb);
		if (aha_poll(sc, xs, ccb->timeout))
			aha_timeout(ccb);
	}
	return COMPLETE;

bad:
	xs->error = XS_DRIVER_STUFFUP;
	aha_free_ccb(sc, ccb);
	return COMPLETE;
}

/*
 * Poll a particular unit, looking for a particular xs
 */
int
aha_poll(sc, xs, count)
	struct aha_softc *sc;
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
		if (inb(iobase + AHA_INTR_PORT) & AHA_INTR_ANYINTR)
			ahaintr(sc);
		if (xs->flags & ITSDONE)
			return 0;
		delay(1000);	/* only happens in boot so ok */
		count--;
	}
	return 1;
}

void
aha_timeout(arg)
	void *arg;
{
	struct aha_ccb *ccb = arg;
	struct scsi_xfer *xs;
	struct scsi_link *sc_link;
	struct aha_softc *sc;
	int s;

	s = splbio();
	isadma_copyfrombuf((caddr_t)ccb, CCB_PHYS_SIZE, 1, ccb->ccb_phys);
	xs = ccb->xs;
	sc_link = xs->sc_link;
	sc = sc_link->adapter_softc;

	sc_print_addr(sc_link);
	printf("timed out");

#ifdef AHADIAG
	/*
	 * If The ccb's mbx is not free, then the board has gone south?
	 */
	aha_collect_mbo(sc);
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
		ccb->timeout = AHA_ABORT_TIMEOUT;
		ccb->flags |= CCB_ABORT;
		aha_queue_ccb(sc, ccb);
	}

	splx(s);
}

