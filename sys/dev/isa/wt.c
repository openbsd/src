/*	$NetBSD: wt.c,v 1.28 1996/01/12 00:54:23 thorpej Exp $	*/

/*
 * Streamer tape driver.
 * Supports Archive and Wangtek compatible QIC-02/QIC-36 boards.
 *
 * Copyright (C) 1993 by:
 *	Sergey Ryzhkov <sir@kiae.su>
 *	Serge Vakulenko <vak@zebub.msk.su>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * This driver is derived from the old 386bsd Wangtek streamer tape driver,
 * made by Robert Baron at CMU, based on Intel sources.
 */

/*
 * Copyright (c) 1989 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Robert Baron
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 *  Copyright 1988, 1989 by Intel Corporation
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/device.h>

#include <vm/vm_param.h>

#include <machine/pio.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>
#include <dev/isa/wtreg.h>

/*
 * Uncomment this to enable internal device tracing.
 */
#define WTDBPRINT(x)		/* printf x */

#define WTPRI			(PZERO+10)	/* sleep priority */

/*
 * Wangtek controller ports
 */
#define WT_CTLPORT(base)	((base)+0)	/* control, write only */
#define WT_STATPORT(base)	((base)+0)	/* status, read only */
#define WT_CMDPORT(base)	((base)+1)	/* command, write only */
#define WT_DATAPORT(base)	((base)+1)	/* data, read only */
#define WT_NPORT		2		/* 2 i/o ports */

/* status port bits */
#define WT_BUSY			0x01		/* not ready bit define */
#define WT_NOEXCEP		0x02		/* no exception bit define */
#define WT_RESETMASK		0x07		/* to check after reset */
#define WT_RESETVAL		0x05		/* state after reset */

/* control port bits */
#define WT_ONLINE		0x01		/* device selected */
#define WT_RESET		0x02		/* reset command */
#define WT_REQUEST		0x04		/* request command */
#define WT_IEN			0x08		/* enable dma */

/*
 * Archive controller ports
 */
#define AV_DATAPORT(base)	((base)+0)	/* data, read only */
#define AV_CMDPORT(base)	((base)+0)	/* command, write only */
#define AV_STATPORT(base)	((base)+1)	/* status, read only */
#define AV_CTLPORT(base)	((base)+1)	/* control, write only */
#define AV_SDMAPORT(base)	((base)+2)	/* start dma */
#define AV_RDMAPORT(base)	((base)+3)	/* reset dma */
#define AV_NPORT		4		/* 4 i/o ports */

/* status port bits */
#define AV_BUSY			0x40		/* not ready bit define */
#define AV_NOEXCEP		0x20		/* no exception bit define */
#define AV_RESETMASK		0xf8		/* to check after reset */
#define AV_RESETVAL		0x50		/* state after reset */

/* control port bits */
#define AV_RESET		0x80		/* reset command */
#define AV_REQUEST		0x40		/* request command */
#define AV_IEN			0x20		/* enable interrupts */

enum wttype {
	UNKNOWN = 0,	/* unknown type, driver disabled */
	ARCHIVE,	/* Archive Viper SC499, SC402 etc */
	WANGTEK,	/* Wangtek */
};

struct wt_softc {
	struct device sc_dev;
	void *sc_ih;

	enum wttype type;	/* type of controller */
	int sc_iobase;		/* base i/o port */
	int chan;		/* dma channel number, 1..3 */
	int flags;		/* state of tape drive */
	unsigned dens;		/* tape density */
	int bsize;		/* tape block size */
	void *buf;		/* internal i/o buffer */

	void *dmavaddr;		/* virtual address of dma i/o buffer */
	size_t dmatotal;	/* size of i/o buffer */
	int dmaflags;		/* i/o direction, B_READ or B_WRITE */
	size_t dmacount;	/* resulting length of dma i/o */

	u_short error;		/* code for error encountered */
	u_short ercnt;		/* number of error blocks */
	u_short urcnt;		/* number of underruns */

	int DATAPORT, CMDPORT, STATPORT, CTLPORT, SDMAPORT, RDMAPORT;
	u_char BUSY, NOEXCEP, RESETMASK, RESETVAL, ONLINE, RESET, REQUEST, IEN;
};

int wtwait __P((struct wt_softc *sc, int catch, char *msg));
int wtcmd __P((struct wt_softc *sc, int cmd));
int wtstart __P((struct wt_softc *sc, int flag, void *vaddr, size_t len));
void wtdma __P((struct wt_softc *sc));
void wttimer __P((void *arg));
void wtclock __P((struct wt_softc *sc));
int wtreset __P((struct wt_softc *sc));
int wtsense __P((struct wt_softc *sc, int verbose, int ignore));
int wtstatus __P((struct wt_softc *sc));
void wtrewind __P((struct wt_softc *sc));
int wtreadfm __P((struct wt_softc *sc));
int wtwritefm __P((struct wt_softc *sc));
u_char wtpoll __P((struct wt_softc *sc, int mask, int bits));

int wtprobe __P((struct device *, void *, void *));
void wtattach __P((struct device *, struct device *, void *));
int wtintr __P((void *sc));

struct cfdriver wtcd = {
	NULL, "wt", wtprobe, wtattach, DV_TAPE, sizeof(struct wt_softc)
};

/*
 * Probe for the presence of the device.
 */
int
wtprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct wt_softc *sc = match;
	struct isa_attach_args *ia = aux;
	int iobase;

	sc->chan = ia->ia_drq;
	sc->sc_iobase = iobase = ia->ia_iobase;
	if (sc->chan < 1 || sc->chan > 3) {
		printf("%s: Bad drq=%d, should be 1..3\n", sc->sc_dev.dv_xname,
		    sc->chan);
		return 0;
	}

	/* Try Wangtek. */
	sc->type = WANGTEK;
	sc->CTLPORT = WT_CTLPORT(iobase);
	sc->STATPORT = WT_STATPORT(iobase);
	sc->CMDPORT = WT_CMDPORT(iobase);
	sc->DATAPORT = WT_DATAPORT(iobase);
	sc->SDMAPORT = sc->RDMAPORT = 0;
	sc->BUSY = WT_BUSY;		sc->NOEXCEP = WT_NOEXCEP;
	sc->RESETMASK = WT_RESETMASK;	sc->RESETVAL = WT_RESETVAL;
	sc->ONLINE = WT_ONLINE;		sc->RESET = WT_RESET;
	sc->REQUEST = WT_REQUEST;	sc->IEN = WT_IEN;
	if (wtreset(sc)) {
		ia->ia_iosize = WT_NPORT;
		return 1;
	}

	/* Try Archive. */
	sc->type = ARCHIVE;
	sc->CTLPORT = AV_CTLPORT(iobase);
	sc->STATPORT = AV_STATPORT(iobase);
	sc->CMDPORT = AV_CMDPORT(iobase);
	sc->DATAPORT = AV_DATAPORT(iobase);
	sc->SDMAPORT = AV_SDMAPORT(iobase);
	sc->RDMAPORT = AV_RDMAPORT(iobase);
	sc->BUSY = AV_BUSY;		sc->NOEXCEP = AV_NOEXCEP;
	sc->RESETMASK = AV_RESETMASK;	sc->RESETVAL = AV_RESETVAL;
	sc->ONLINE = 0;			sc->RESET = AV_RESET;
	sc->REQUEST = AV_REQUEST;	sc->IEN = AV_IEN;
	if (wtreset(sc)) {
		ia->ia_iosize = AV_NPORT;
		return 1;
	}

	/* Tape controller not found. */
	sc->type = UNKNOWN;
	return 0;
}

/*
 * Device is found, configure it.
 */
void
wtattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wt_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;

	if (sc->type == ARCHIVE) {
		printf(": type <Archive>\n");
		/* Reset DMA. */
		outb(sc->RDMAPORT, 0);
	} else
		printf(": type <Wangtek>\n");
	sc->flags = TPSTART;		/* tape is rewound */
	sc->dens = -1;			/* unknown density */

	sc->sc_ih = isa_intr_establish(ia->ia_irq, IST_EDGE, IPL_BIO, wtintr,
	    sc);
}

int
wtdump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{

	/* Not implemented. */
	return ENXIO;
}

int
wtsize(dev)
	dev_t dev;
{

	/* Not implemented. */
	return -1;
}

/*
 * Open routine, called on every device open.
 */
int
wtopen(dev, flag)
	dev_t dev;
	int flag;
{
	int unit = minor(dev) & T_UNIT;
	struct wt_softc *sc;
	int error;

	if (unit >= wtcd.cd_ndevs)
		return ENXIO;
	sc = wtcd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	/* Check that device is not in use */
	if (sc->flags & TPINUSE)
		return EBUSY;

	/* If the tape is in rewound state, check the status and set density. */
	if (sc->flags & TPSTART) {
		/* If rewind is going on, wait */
		if (error = wtwait(sc, PCATCH, "wtrew"))
			return error;

		/* Check the controller status */
		if (!wtsense(sc, 0, (flag & FWRITE) ? 0 : TP_WRP)) {
			/* Bad status, reset the controller. */
			if (!wtreset(sc))
				return EIO;
			if (!wtsense(sc, 1, (flag & FWRITE) ? 0 : TP_WRP))
				return EIO;
		}

		/* Set up tape density. */
		if (sc->dens != (minor(dev) & WT_DENSEL)) {
			int d = 0;

			switch (minor(dev) & WT_DENSEL) {
			case WT_DENSDFLT:
			default:
				break;			/* default density */
			case WT_QIC11:
				d = QIC_FMT11;  break;	/* minor 010 */
			case WT_QIC24:
				d = QIC_FMT24;  break;	/* minor 020 */
			case WT_QIC120:
				d = QIC_FMT120; break;	/* minor 030 */
			case WT_QIC150:
				d = QIC_FMT150; break;	/* minor 040 */
			case WT_QIC300:
				d = QIC_FMT300; break;	/* minor 050 */
			case WT_QIC600:
				d = QIC_FMT600; break;	/* minor 060 */
			}
			if (d) {
				/* Change tape density. */
				if (!wtcmd(sc, d))
					return EIO;
				if (!wtsense(sc, 1, TP_WRP | TP_ILL))
					return EIO;

				/* Check the status of the controller. */
				if (sc->error & TP_ILL) {
					printf("%s: invalid tape density\n",
					    sc->sc_dev.dv_xname);
					return ENODEV;
				}
			}
			sc->dens = minor(dev) & WT_DENSEL;
		}
		sc->flags &= ~TPSTART;
	} else if (sc->dens != (minor(dev) & WT_DENSEL))
		return ENXIO;

	sc->bsize = (minor(dev) & WT_BSIZE) ? 1024 : 512;
	sc->buf = malloc(sc->bsize, M_TEMP, M_WAITOK);

	sc->flags = TPINUSE;
	if (flag & FREAD)
		sc->flags |= TPREAD;
	if (flag & FWRITE)
		sc->flags |= TPWRITE;
	return 0;
}

/*
 * Close routine, called on last device close.
 */
int
wtclose(dev)
	dev_t dev;
{
	int unit = minor(dev) & T_UNIT;
	struct wt_softc *sc = wtcd.cd_devs[unit];

	/* If rewind is pending, do nothing */
	if (sc->flags & TPREW)
		goto done;

	/* If seek forward is pending and no rewind on close, do nothing */
	if (sc->flags & TPRMARK) {
		if (minor(dev) & T_NOREWIND)
			goto done;

		/* If read file mark is going on, wait */
		wtwait(sc, 0, "wtrfm");
	}

	if (sc->flags & TPWANY) {
		/* Tape was written.  Write file mark. */
		wtwritefm(sc);
	}

	if ((minor(dev) & T_NOREWIND) == 0) {
		/* Rewind to beginning of tape. */
		/* Don't wait until rewind, though. */
		wtrewind(sc);
		goto done;
	}
	if ((sc->flags & TPRANY) && (sc->flags & (TPVOL | TPWANY)) == 0) {
		/* Space forward to after next file mark if no writing done. */
		/* Don't wait for completion. */
		wtreadfm(sc);
	}

done:
	sc->flags &= TPREW | TPRMARK | TPSTART | TPTIMER;
	free(sc->buf, M_TEMP);
	return 0;
}

/*
 * Ioctl routine.  Compatible with BSD ioctls.
 * Direct QIC-02 commands ERASE and RETENSION added.
 * There are three possible ioctls:
 * ioctl(int fd, MTIOCGET, struct mtget *buf)	-- get status
 * ioctl(int fd, MTIOCTOP, struct mtop *buf)	-- do BSD-like op
 * ioctl(int fd, WTQICMD, int qicop)		-- do QIC op
 */
int
wtioctl(dev, cmd, addr, flag)
	dev_t dev;
	u_long cmd;
	void *addr;
	int flag;
{
	int unit = minor(dev) & T_UNIT;
	struct wt_softc *sc = wtcd.cd_devs[unit];
	int error, count, op;

	switch (cmd) {
	default:
		return EINVAL;
	case WTQICMD:	/* direct QIC command */
		op = *(int *)addr;
		switch (op) {
		default:
			return EINVAL;
		case QIC_ERASE:		/* erase the whole tape */
			if ((sc->flags & TPWRITE) == 0 || (sc->flags & TPWP))
				return EACCES;
			if (error = wtwait(sc, PCATCH, "wterase"))
				return error;
			break;
		case QIC_RETENS:	/* retension the tape */
			if (error = wtwait(sc, PCATCH, "wtretens"))
				return error;
			break;
		}
		/* Both ERASE and RETENS operations work like REWIND. */
		/* Simulate the rewind operation here. */
		sc->flags &= ~(TPRO | TPWO | TPVOL);
		if (!wtcmd(sc, op))
			return EIO;
		sc->flags |= TPSTART | TPREW;
		if (op == QIC_ERASE)
			sc->flags |= TPWANY;
		wtclock(sc);
		return 0;
	case MTIOCIEOT:	/* ignore EOT errors */
	case MTIOCEEOT:	/* enable EOT errors */
		return 0;
	case MTIOCGET:
		((struct mtget*)addr)->mt_type =
			sc->type == ARCHIVE ? MT_ISVIPER1 : 0x11;
		((struct mtget*)addr)->mt_dsreg = sc->flags;	/* status */
		((struct mtget*)addr)->mt_erreg = sc->error;	/* errors */
		((struct mtget*)addr)->mt_resid = 0;
		((struct mtget*)addr)->mt_fileno = 0;		/* file */
		((struct mtget*)addr)->mt_blkno = 0;		/* block */
		return 0;
	case MTIOCTOP:
		break;
	}

	switch ((short)((struct mtop*)addr)->mt_op) {
	default:
#if 0
	case MTFSR:	/* forward space record */
	case MTBSR:	/* backward space record */
	case MTBSF:	/* backward space file */
#endif
		return EINVAL;
	case MTNOP:	/* no operation, sets status only */
	case MTCACHE:	/* enable controller cache */
	case MTNOCACHE:	/* disable controller cache */
		return 0;
	case MTREW:	/* rewind */
	case MTOFFL:	/* rewind and put the drive offline */
		if (sc->flags & TPREW)   /* rewind is running */
			return 0;
		if (error = wtwait(sc, PCATCH, "wtorew"))
			return error;
		wtrewind(sc);
		return 0;
	case MTFSF:	/* forward space file */
		for (count = ((struct mtop*)addr)->mt_count; count > 0;
		    --count) {
			if (error = wtwait(sc, PCATCH, "wtorfm"))
				return error;
			if (error = wtreadfm(sc))
				return error;
		}
		return 0;
	case MTWEOF:	/* write an end-of-file record */
		if ((sc->flags & TPWRITE) == 0 || (sc->flags & TPWP))
			return EACCES;
		if (error = wtwait(sc, PCATCH, "wtowfm"))
			return error;
		if (error = wtwritefm(sc))
			return error;
		return 0;
	}

#ifdef DIAGNOSTIC
	panic("wtioctl: impossible");
#endif
}

/*
 * Strategy routine.
 */
void
wtstrategy(bp)
	struct buf *bp;
{
	int unit = minor(bp->b_dev) & T_UNIT;
	struct wt_softc *sc = wtcd.cd_devs[unit];
	int s;

	bp->b_resid = bp->b_bcount;

	/* at file marks and end of tape, we just return '0 bytes available' */
	if (sc->flags & TPVOL)
		goto xit;

	if (bp->b_flags & B_READ) {
		/* Check read access and no previous write to this tape. */
		if ((sc->flags & TPREAD) == 0 || (sc->flags & TPWANY))
			goto errxit;

		/* For now, we assume that all data will be copied out */
		/* If read command outstanding, just skip down */
		if ((sc->flags & TPRO) == 0) {
			if (!wtsense(sc, 1, TP_WRP)) {
				/* Clear status. */
				goto errxit;
			}
			if (!wtcmd(sc, QIC_RDDATA)) {
				/* Set read mode. */
				wtsense(sc, 1, TP_WRP);
				goto errxit;
			}
			sc->flags |= TPRO | TPRANY;
		}
	} else {
		/* Check write access and write protection. */
		/* No previous read from this tape allowed. */
		if ((sc->flags & TPWRITE) == 0 || (sc->flags & (TPWP | TPRANY)))
			goto errxit;

		/* If write command outstanding, just skip down */
		if ((sc->flags & TPWO) == 0) {
			if (!wtsense(sc, 1, 0)) {
				/* Clear status. */
				goto errxit;
			}
			if (!wtcmd(sc, QIC_WRTDATA)) {
				/* Set write mode. */
				wtsense(sc, 1, 0);
				goto errxit;
			}
			sc->flags |= TPWO | TPWANY;
		}
	}

	if (bp->b_bcount == 0)
		goto xit;

	sc->flags &= ~TPEXCEP;
	s = splbio();
	if (wtstart(sc, bp->b_flags, bp->b_data, bp->b_bcount)) {
		wtwait(sc, 0, (bp->b_flags & B_READ) ? "wtread" : "wtwrite");
		bp->b_resid -= sc->dmacount;
	}
	splx(s);

	if (sc->flags & TPEXCEP) {
errxit:
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
	}
xit:
	biodone(bp);
	return;
}

int
wtread(dev, uio)
	dev_t dev;
	struct uio *uio;
{

	return (physio(wtstrategy, NULL, dev, B_READ, minphys, uio));
}

int
wtwrite(dev, uio)
	dev_t dev;
	struct uio *uio;
{

	return (physio(wtstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * Interrupt routine.
 */
int
wtintr(arg)
	void *arg;
{
	struct wt_softc *sc = arg;
	u_char x;

	x = inb(sc->STATPORT);			/* get status */
	WTDBPRINT(("wtintr() status=0x%x -- ", x));
	if ((x & (sc->BUSY | sc->NOEXCEP)) == (sc->BUSY | sc->NOEXCEP)) {
		WTDBPRINT(("busy\n"));
		return 0;			/* device is busy */
	}

	/*
	 * Check if rewind finished.
	 */
	if (sc->flags & TPREW) {
		WTDBPRINT(((x & (sc->BUSY | sc->NOEXCEP)) == (sc->BUSY | sc->NOEXCEP) ?
		    "rewind busy?\n" : "rewind finished\n"));
		sc->flags &= ~TPREW;		/* rewind finished */
		wtsense(sc, 1, TP_WRP);
		wakeup((caddr_t)sc);
		return 1;
	}

	/*
	 * Check if writing/reading of file mark finished.
	 */
	if (sc->flags & (TPRMARK | TPWMARK)) {
		WTDBPRINT(((x & (sc->BUSY | sc->NOEXCEP)) == (sc->BUSY | sc->NOEXCEP) ?
		    "marker r/w busy?\n" : "marker r/w finished\n"));
		if ((x & sc->NOEXCEP) == 0)	/* operation failed */
			wtsense(sc, 1, (sc->flags & TPRMARK) ? TP_WRP : 0);
		sc->flags &= ~(TPRMARK | TPWMARK); /* operation finished */
		wakeup((caddr_t)sc);
		return 1;
	}

	/*
	 * Do we started any i/o?  If no, just return.
	 */
	if ((sc->flags & TPACTIVE) == 0) {
		WTDBPRINT(("unexpected interrupt\n"));
		return 0;
	}
	sc->flags &= ~TPACTIVE;
	sc->dmacount += sc->bsize;		/* increment counter */

	/*
	 * Clean up dma.
	 */
	if ((sc->dmaflags & B_READ) &&
	    (sc->dmatotal - sc->dmacount) < sc->bsize) {
		/* If reading short block, copy the internal buffer
		 * to the user memory. */
		isa_dmadone(sc->dmaflags, sc->buf, sc->bsize, sc->chan);
		bcopy(sc->buf, sc->dmavaddr, sc->dmatotal - sc->dmacount);
	} else
		isa_dmadone(sc->dmaflags, sc->dmavaddr, sc->bsize, sc->chan);

	/*
	 * On exception, check for end of file and end of volume.
	 */
	if ((x & sc->NOEXCEP) == 0) {
		WTDBPRINT(("i/o exception\n"));
		wtsense(sc, 1, (sc->dmaflags & B_READ) ? TP_WRP : 0);
		if (sc->error & (TP_EOM | TP_FIL))
			sc->flags |= TPVOL;	/* end of file */
		else
			sc->flags |= TPEXCEP;	/* i/o error */
		wakeup((caddr_t)sc);
		return 1;
	}

	if (sc->dmacount < sc->dmatotal) {
		/* Continue I/O. */
		sc->dmavaddr += sc->bsize;
		wtdma(sc);
		WTDBPRINT(("continue i/o, %d\n", sc->dmacount));
		return 1;
	}
	if (sc->dmacount > sc->dmatotal)	/* short last block */
		sc->dmacount = sc->dmatotal;
	/* Wake up user level. */
	wakeup((caddr_t)sc);
	WTDBPRINT(("i/o finished, %d\n", sc->dmacount));
	return 1;
}

/* start the rewind operation */
void
wtrewind(sc)
	struct wt_softc *sc;
{
	int rwmode = sc->flags & (TPRO | TPWO);

	sc->flags &= ~(TPRO | TPWO | TPVOL);
	/*
	 * Wangtek strictly follows QIC-02 standard:
	 * clearing ONLINE in read/write modes causes rewind.
	 * REWIND command is not allowed in read/write mode
	 * and gives `illegal command' error.
	 */
	if (sc->type == WANGTEK && rwmode) {
		outb(sc->CTLPORT, 0);
	} else if (!wtcmd(sc, QIC_REWIND))
		return;
	sc->flags |= TPSTART | TPREW;
	wtclock(sc);
}

/*
 * Start the `read marker' operation.
 */
int
wtreadfm(sc)
	struct wt_softc *sc;
{

	sc->flags &= ~(TPRO | TPWO | TPVOL);
	if (!wtcmd(sc, QIC_READFM)) {
		wtsense(sc, 1, TP_WRP);
		return EIO;
	}
	sc->flags |= TPRMARK | TPRANY;
	wtclock(sc);
	/* Don't wait for completion here. */
	return 0;
}

/*
 * Write marker to the tape.
 */
int
wtwritefm(sc)
	struct wt_softc *sc;
{

	tsleep((caddr_t)wtwritefm, WTPRI, "wtwfm", hz);
	sc->flags &= ~(TPRO | TPWO);
	if (!wtcmd(sc, QIC_WRITEFM)) {
		wtsense(sc, 1, 0);
		return EIO;
	}
	sc->flags |= TPWMARK | TPWANY;
	wtclock(sc);
	return wtwait(sc, 0, "wtwfm");
}

/*
 * While controller status & mask == bits continue waiting.
 */
u_char
wtpoll(sc, mask, bits)
	struct wt_softc *sc;
	int mask, bits;
{
	u_char x;
	int i;

	/* Poll status port, waiting for specified bits. */
	for (i = 0; i < 1000; ++i) {	/* up to 1 msec */
		x = inb(sc->STATPORT);
		if ((x & mask) != bits)
			return x;
		delay(1);
	}
	for (i = 0; i < 100; ++i) {	/* up to 10 msec */
		x = inb(sc->STATPORT);
		if ((x & mask) != bits)
			return x;
		delay(100);
	}
	for (;;) {			/* forever */
		x = inb(sc->STATPORT);
		if ((x & mask) != bits)
			return x;
		tsleep((caddr_t)wtpoll, WTPRI, "wtpoll", 1);
	}
}

/*
 * Execute QIC command.
 */
int
wtcmd(sc, cmd)
	struct wt_softc *sc;
	int cmd;
{
	u_char x;
	int s;

	WTDBPRINT(("wtcmd() cmd=0x%x\n", cmd));
	s = splbio();
	x = wtpoll(sc, sc->BUSY | sc->NOEXCEP, sc->BUSY | sc->NOEXCEP); /* ready? */
	if ((x & sc->NOEXCEP) == 0) {			/* error */
		splx(s);
		return 0;
	}
	
	outb(sc->CMDPORT, cmd);				/* output the command */

	outb(sc->CTLPORT, sc->REQUEST | sc->ONLINE);	/* set request */
	wtpoll(sc, sc->BUSY, sc->BUSY);			/* wait for ready */
	outb(sc->CTLPORT, sc->IEN | sc->ONLINE);	/* reset request */
	wtpoll(sc, sc->BUSY, 0);			/* wait for not ready */
	splx(s);
	return 1;
}

/* wait for the end of i/o, seeking marker or rewind operation */
int
wtwait(sc, catch, msg)
	struct wt_softc *sc;
	int catch;
	char *msg;
{
	int error;

	WTDBPRINT(("wtwait() `%s'\n", msg));
	while (sc->flags & (TPACTIVE | TPREW | TPRMARK | TPWMARK))
		if (error = tsleep((caddr_t)sc, WTPRI | catch, msg, 0))
			return error;
	return 0;
}

/* initialize dma for the i/o operation */
void
wtdma(sc)
	struct wt_softc *sc;
{

	sc->flags |= TPACTIVE;
	wtclock(sc);

	if (sc->type == ARCHIVE) {
		/* Set DMA. */
		outb(sc->SDMAPORT, 0);
	}

	if ((sc->dmaflags & B_READ) &&
	    (sc->dmatotal - sc->dmacount) < sc->bsize) {
		/* Reading short block; do it through the internal buffer. */
		isa_dmastart(sc->dmaflags, sc->buf, sc->bsize, sc->chan);
	} else
		isa_dmastart(sc->dmaflags, sc->dmavaddr, sc->bsize, sc->chan);
}

/* start i/o operation */
int
wtstart(sc, flag, vaddr, len)
	struct wt_softc *sc;
	int flag;
	void *vaddr;
	size_t len;
{
	u_char x;

	WTDBPRINT(("wtstart()\n"));
	x = wtpoll(sc, sc->BUSY | sc->NOEXCEP, sc->BUSY | sc->NOEXCEP); /* ready? */
	if ((x & sc->NOEXCEP) == 0) {
		sc->flags |= TPEXCEP;	/* error */
		return 0;
	}
	sc->flags &= ~TPEXCEP;		/* clear exception flag */
	sc->dmavaddr = vaddr;
	sc->dmatotal = len;
	sc->dmacount = 0;
	sc->dmaflags = flag;
	wtdma(sc);
	return 1;
}

/*
 * Start timer.
 */
void
wtclock(sc)
	struct wt_softc *sc;
{

	if (sc->flags & TPTIMER)
		return;
	sc->flags |= TPTIMER;
	/*
	 * Some controllers seem to lose dma interrupts too often.  To make the
	 * tape stream we need 1 tick timeout.
	 */
	timeout(wttimer, sc, (sc->flags & TPACTIVE) ? 1 : hz);
}

/*
 * Simulate an interrupt periodically while i/o is going.
 * This is necessary in case interrupts get eaten due to
 * multiple devices on a single IRQ line.
 */
void
wttimer(arg)
	void *arg;
{
	struct wt_softc *sc = (struct wt_softc *)arg;
	int s;

	sc->flags &= ~TPTIMER;
	if ((sc->flags & (TPACTIVE | TPREW | TPRMARK | TPWMARK)) == 0)
		return;

	/* If i/o going, simulate interrupt. */
	s = splbio();
	if ((inb(sc->STATPORT) & (sc->BUSY | sc->NOEXCEP)) != (sc->BUSY | sc->NOEXCEP)) {
		WTDBPRINT(("wttimer() -- "));
		wtintr(sc);
	}
	splx(s);

	/* Restart timer if i/o pending. */
	if (sc->flags & (TPACTIVE | TPREW | TPRMARK | TPWMARK))
		wtclock(sc);
}

/*
 * Perform QIC-02 and QIC-36 compatible reset sequence.
 */
int
wtreset(sc)
	struct wt_softc *sc;
{
	u_char x;
	int i;

	outb(sc->CTLPORT, sc->RESET | sc->ONLINE); /* send reset */
	delay(30);
	outb(sc->CTLPORT, sc->ONLINE);	/* turn off reset */
	delay(30);

	/* Read the controller status. */
	x = inb(sc->STATPORT);
	if (x == 0xff)			/* no port at this address? */
		return 0;

	/* Wait 3 sec for reset to complete. Needed for QIC-36 boards? */
	for (i = 0; i < 3000; ++i) {
		if ((x & sc->BUSY) == 0 || (x & sc->NOEXCEP) == 0)
			break;
		delay(1000);
		x = inb(sc->STATPORT);
	}
	return (x & sc->RESETMASK) == sc->RESETVAL;
}

/*
 * Get controller status information.  Return 0 if user i/o request should
 * receive an i/o error code.
 */
int
wtsense(sc, verbose, ignore)
	struct wt_softc *sc;
	int verbose, ignore;
{
	char *msg = 0;
	int error;

	WTDBPRINT(("wtsense() ignore=0x%x\n", ignore));
	sc->flags &= ~(TPRO | TPWO);
	if (!wtstatus(sc))
		return 0;
	if ((sc->error & TP_ST0) == 0)
		sc->error &= ~TP_ST0MASK;
	if ((sc->error & TP_ST1) == 0)
		sc->error &= ~TP_ST1MASK;
	sc->error &= ~ignore;	/* ignore certain errors */
	error = sc->error & (TP_FIL | TP_BNL | TP_UDA | TP_EOM | TP_WRP |
	    TP_USL | TP_CNI | TP_MBD | TP_NDT | TP_ILL);
	if (!error)
		return 1;
	if (!verbose)
		return 0;

	/* lifted from tdriver.c from Wangtek */
	if (error & TP_USL)
		msg = "Drive not online";
	else if (error & TP_CNI)
		msg = "No cartridge";
	else if ((error & TP_WRP) && (sc->flags & TPWP) == 0) {
		msg = "Tape is write protected";
		sc->flags |= TPWP;
	} else if (error & TP_FIL)
		msg = 0 /*"Filemark detected"*/;
	else if (error & TP_EOM)
		msg = 0 /*"End of tape"*/;
	else if (error & TP_BNL)
		msg = "Block not located";
	else if (error & TP_UDA)
		msg = "Unrecoverable data error";
	else if (error & TP_NDT)
		msg = "No data detected";
	else if (error & TP_ILL)
		msg = "Illegal command";
	if (msg)
		printf("%s: %s\n", sc->sc_dev.dv_xname, msg);
	return 0;
}

/*
 * Get controller status information.
 */
int
wtstatus(sc)
	struct wt_softc *sc;
{
	char *p;
	int s;

	s = splbio();
	wtpoll(sc, sc->BUSY | sc->NOEXCEP, sc->BUSY | sc->NOEXCEP); /* ready? */
	outb(sc->CMDPORT, QIC_RDSTAT);	/* send `read status' command */

	outb(sc->CTLPORT, sc->REQUEST | sc->ONLINE);	/* set request */
	wtpoll(sc, sc->BUSY, sc->BUSY);			/* wait for ready */
	outb(sc->CTLPORT, sc->ONLINE);			/* reset request */
	wtpoll(sc, sc->BUSY, 0);			/* wait for not ready */

	p = (char *)&sc->error;
	while (p < (char *)&sc->error + 6) {
		u_char x = wtpoll(sc, sc->BUSY | sc->NOEXCEP, sc->BUSY | sc->NOEXCEP);
		if ((x & sc->NOEXCEP) == 0) {	/* error */
			splx(s);
			return 0;
		}

		*p++ = inb(sc->DATAPORT);	/* read status byte */

		outb(sc->CTLPORT, sc->REQUEST | sc->ONLINE); /* set request */
		wtpoll(sc, sc->BUSY, 0);	/* wait for not ready */
		outb(sc->CTLPORT, sc->ONLINE);	/* unset request */
	}
	splx(s);
	return 1;
}
