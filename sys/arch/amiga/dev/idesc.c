/*	$NetBSD: idesc.c,v 1.15 1996/01/07 22:01:53 thorpej Exp $	*/

/*
 * Copyright (c) 1994 Michael L. Hitch
 * Copyright (c) 1993, 1994 Charles Hannum.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Don Ahn.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)wd.c	7.4 (Berkeley) 5/25/91
 */
/*
 * Copyright (c) 1994 Michael L. Hitch
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
 *      This product includes software developed by Brad Pepers
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * A4000 IDE interface, emulating a SCSI controller
 */

#include "idesc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/disklabel.h>
#include <sys/dkstat.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/cia.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/zbusvar.h>

#define	b_cylin		b_resid

/* defines */

struct regs {
	volatile u_short	ide_data;		/* 00 */
	char	____pad0[4];
	volatile u_char		ide_error;		/* 06 */
#define ide_precomp		ide_error
	char	____pad1[3];
	volatile u_char		ide_seccnt;		/* 0a */
	char	____pad2[3];
	volatile u_char		ide_sector;		/* 0e */
	char	____pad3[3];
	volatile u_char		ide_cyl_lo;		/* 12 */
	char	____pad4[3];
	volatile u_char		ide_cyl_hi;		/* 16 */
	char	____pad5[3];
	volatile u_char		ide_sdh;		/* 1a */
	char	____pad6[3];
	volatile u_char		ide_command;		/* 1e */
#define ide_status		ide_command
	char	____pad7;
	char	____pad8[0xfe0];
	volatile short		ide_intpnd;		/* 1000 */
	char	____pad9[24];
	volatile u_char		ide_altsts;		/* 101a */
#define ide_ctlr		ide_altsts
};
typedef volatile struct regs *ide_regmap_p;

#define	IDES_BUSY	0x80	/* controller busy bit */
#define	IDES_READY	0x40	/* selected drive is ready */
#define	IDES_WRTFLT	0x20	/* Write fault */
#define	IDES_SEEKCMPLT	0x10	/* Seek complete */
#define	IDES_DRQ	0x08	/* Data request bit */
#define	IDES_ECCCOR	0x04	/* ECC correction made in data */
#define	IDES_INDEX	0x02	/* Index pulse from selected drive */
#define	IDES_ERR	0x01	/* Error detect bit */

#define	IDEC_RESTORE	0x10

#define	IDEC_READ	0x20
#define	IDEC_WRITE	0x30

#define	IDEC_XXX	0x40
#define	IDEC_FORMAT	0x50
#define	IDEC_XXXX	0x70
#define	IDEC_DIAGNOSE	0x90
#define	IDEC_IDC	0x91

#define	IDEC_READP	0xec

#define IDECTL_IDS	0x02		/* Interrupt disable */

struct ideparams {
	/* drive info */
	short	idep_config;		/* general configuration */
	short	idep_fixedcyl;		/* number of non-removable cylinders */
	short	idep_removcyl;		/* number of removable cylinders */
	short	idep_heads;		/* number of heads */
	short	idep_unfbytespertrk;	/* number of unformatted bytes/track */
	short	idep_unfbytes;		/* number of unformatted bytes/sector */
	short	idep_sectors;		/* number of sectors */
	short	idep_minisg;		/* minimum bytes in inter-sector gap*/
	short	idep_minplo;		/* minimum bytes in postamble */
	short	idep_vendstat;		/* number of words of vendor status */
	/* controller info */
	char	idep_cnsn[20];		/* controller serial number */
	short	idep_cntype;		/* controller type */
#define	IDETYPE_SINGLEPORTSECTOR	1	 /* single port, single sector buffer */
#define	IDETYPE_DUALPORTMULTI	2	 /* dual port, multiple sector buffer */
#define	IDETYPE_DUALPORTMULTICACHE 3	 /* above plus track cache */
	short	idep_cnsbsz;		/* sector buffer size, in sectors */
	short	idep_necc;		/* ecc bytes appended */
	char	idep_rev[8];		/* firmware revision */
	char	idep_model[40];		/* model name */
	short	idep_nsecperint;		/* sectors per interrupt */
	short	idep_usedmovsd;		/* can use double word read/write? */
};

/*
 * Per drive structure.
 * N per controller (presently 2) (DRVS_PER_CTLR)
 */
struct ide_softc {
	struct device sc_dev;
	long	sc_bcount;	/* byte count left */
	long	sc_mbcount;	/* total byte count left */
	short	sc_skip;	/* blocks already transferred */
	short	sc_mskip;	/* blocks already transfereed for multi */
	long	sc_blknum;	/* starting block of active request */
	u_char	*sc_buf;	/* buffer address of active request */
	long	sc_blkcnt;	/* block count of active request */
	int	sc_flags;
#define	IDEF_ALIVE	0x01	/* it's a valid device	*/
	short	sc_error;
	char	sc_drive;
	char	sc_state;
	long	sc_secpercyl;
	long	sc_sectors;
	struct buf sc_dq;
	struct ideparams sc_params;
};

struct	ide_pending {
	TAILQ_ENTRY(ide_pending) link;
	struct scsi_xfer *xs;
};

/*
 * Per controller structure.
 */
struct idec_softc
{
	struct device sc_dev;
	struct isr sc_isr;

	struct	scsi_link sc_link;	/* proto for sub devices */
	ide_regmap_p	sc_cregs;	/* driver specific regs */
	volatile u_char *sc_a1200;	/* A1200 interrupt control */
	TAILQ_HEAD(,ide_pending) sc_xslist;	/* LIFO */
	struct	ide_pending sc_xsstore[8][8];	/* one for every unit */
	struct	scsi_xfer *sc_xs;	/* transfer from high level code */
	int	sc_flags;
#define	IDECF_ALIVE	0x01	/* Controller is alive */
#define	IDECF_ACTIVE	0x02
#define	IDECF_SINGLE	0x04	/* sector at a time mode */
#define	IDECF_READ	0x08	/* Current operation is read */
#define	IDECF_A1200	0x10	/* A1200 IDE */
	struct ide_softc *sc_cur; /* drive we are currently doing work for */
	int	state;
	int	saved;
	int	retry;
	char	sc_stat[2];
	struct ide_softc	sc_ide[2];
};

int ide_scsicmd __P((struct scsi_xfer *));

int idescprint __P((void *auxp, char *));
void idescattach __P((struct device *, struct device *, void *));
int idescmatch __P((struct device *, struct cfdata *, void *));

int  ideicmd __P((struct idec_softc *, int, void *, int, void *, int));
int  idego __P((struct idec_softc *, struct scsi_xfer *));
int  idegetsense __P((struct idec_softc *, struct scsi_xfer *));
void ideabort __P((struct idec_softc *, ide_regmap_p, char *));
void ideerror __P((struct idec_softc *, ide_regmap_p, u_char));
int idestart __P((struct idec_softc *));
int idereset __P((struct idec_softc *));
void idesetdelay __P((int));
void ide_scsidone __P((struct idec_softc *, int));
void ide_donextcmd __P((struct idec_softc *));
int  idesc_intr __P((struct idec_softc *));

struct scsi_adapter idesc_scsiswitch = {
	ide_scsicmd,
	minphys,		/* no max transfer len, at this level */
	0,			/* no lun support */
	0,			/* no lun support */
};

struct scsi_device idesc_scsidev = {
	NULL,		/* use default error handler */
	NULL,		/* do not have a start functio */
	NULL,		/* have no async handler */
	NULL,		/* Use default done routine */
};

struct cfdriver idesccd = {
	NULL, "idesc", (cfmatch_t)idescmatch, idescattach, 
	DV_DULL, sizeof(struct idec_softc), NULL, 0 };

struct {
	short	ide_err;
	char	scsi_sense_key;
	char	scsi_sense_qual;
} sense_convert[] = {
	{ 0x0001, 0x03, 0x13},	/* Data address mark not found */
	{ 0x0002, 0x04, 0x06},	/* Reference position not found */
	{ 0x0004, 0x05, 0x20},	/* Invalid command */
	{ 0x0010, 0x03, 0x12},	/* ID address mark not found */
	{ 0x0040, 0x03, 0x11},	/* Unrecovered read error */
	{ 0x0080, 0x03, 0x11},	/* Bad block mark detected */
	{ 0x0000, 0x05, 0x00}	/* unknown */
};

/*
 * protos.
 */

int idecommand __P((struct ide_softc *, int, int, int, int, int));
int idewait __P((struct idec_softc *, int));
int idegetctlr __P((struct ide_softc *));

#define wait_for_drq(ide) idewait(ide, IDES_DRQ)
#define wait_for_ready(ide) idewait(ide, IDES_READY | IDES_SEEKCMPLT)
#define wait_for_unbusy(ide) idewait(ide,0)

int ide_no_int = 0;

#ifdef DEBUG 

int ide_debug = 0;

#define TRACE0(arg) if (ide_debug > 1) printf(arg)
#define TRACE1(arg1,arg2) if (ide_debug > 1) printf(arg1,arg2)
#define QPRINTF(a) if (ide_debug > 1) printf a

#else	/* !DEBUG */

#define TRACE0(arg)
#define TRACE1(arg1,arg2)
#define QPRINTF

#endif	/* !DEBUG */


/*
 * if we are an A4000 we are here.
 */
int
idescmatch(pdp, cdp, auxp)
	struct device *pdp;
	struct cfdata *cdp;
	void *auxp;
{
	char *mbusstr;

	mbusstr = auxp;
	if ((is_a4000() || is_a1200()) && matchname(auxp, "idesc"))
		return(1);
	return(0);
}

void
idescattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	ide_regmap_p rp;
	struct idec_softc *sc;
	int i;

	sc = (struct idec_softc *)dp;
	if (is_a4000())
		sc->sc_cregs = rp = (ide_regmap_p) ztwomap(0xdd2020);
	else {
		/* Let's hope the A1200 will work with the same regs */
		sc->sc_cregs = rp = (ide_regmap_p) ztwomap(0xda0000);
		sc->sc_a1200 = ztwomap(0xda8000 + 0x1000);
		sc->sc_flags |= IDECF_A1200;
		printf(" A1200 @ %x:%x", rp, sc->sc_a1200);
	}

#ifdef DEBUG
	if (ide_debug)
		ide_dump_regs(rp);
#endif
	rp->ide_error = 0x5a;
	rp->ide_cyl_lo = 0xa5;
	if (rp->ide_error == 0x5a || rp->ide_cyl_lo != 0xa5)
		return;
	/* test if controller will reset */
	if (idereset(sc) != 0) {
		delay (500000);
		if (idereset(sc) != 0) {
			printf (" IDE controller did not reset\n");
			return;
		}
	}
	/* Dummy up the unit structures */
	sc->sc_ide[0].sc_dev.dv_parent = (void *) sc;
	sc->sc_ide[1].sc_dev.dv_parent = (void *) sc;
#if 0	/* Amiga ROM does this; it also takes a lot of time on the Seacrate */
	/* Execute a controller only command. */
	if (idecommand(&sc->sc_ide[0], 0, 0, 0, 0, IDEC_DIAGNOSE) != 0 ||
	    wait_for_unbusy(sc) != 0) {
		printf (" ide attach failed\n");
		return;
	}
#endif
#ifdef DEBUG
	if (ide_debug)
		ide_dump_regs(rp);
#endif

	idereset(sc);

	for (i = 0; i < 2; ++i) {
		rp->ide_sdh = 0xa0 | (i << 4);
		sc->sc_ide[i].sc_drive = i;
		if ((rp->ide_status & IDES_READY) == 0)
			continue;
		sc->sc_ide[i].sc_flags |= IDEF_ALIVE;
		rp->ide_ctlr = 0;
	}

	printf ("\n");

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = 7;
	sc->sc_link.adapter = &idesc_scsiswitch;
	sc->sc_link.device = &idesc_scsidev;
	sc->sc_link.openings = 1;
	TAILQ_INIT(&sc->sc_xslist);

	sc->sc_isr.isr_intr = idesc_intr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = 2;
	add_isr (&sc->sc_isr);

	/*
	 * attach all "scsi" units on us
	 */
	config_found(dp, &sc->sc_link, idescprint);
}

/*
 * print diag if pnp is NULL else just extra
 */
int
idescprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	if (pnp == NULL)
		return(UNCONF);
	return(QUIET);
}

/*
 * used by specific ide controller
 *
 */
int
ide_scsicmd(xs)
	struct scsi_xfer *xs;
{
	struct ide_pending *pendp;
	struct idec_softc *dev;
	struct scsi_link *slp;
	int flags, s;

	slp = xs->sc_link;
	dev = slp->adapter_softc;
	flags = xs->flags;

	if (flags & SCSI_DATA_UIO)
		panic("ide: scsi data uio requested");
	
	if (dev->sc_xs && flags & SCSI_POLL)
		panic("ide_scsicmd: busy");

	s = splbio();
	pendp = &dev->sc_xsstore[slp->target][slp->lun];
	if (pendp->xs) {
		splx(s);
		return(TRY_AGAIN_LATER);
	}

	if (dev->sc_xs) {
		pendp->xs = xs;
		TAILQ_INSERT_TAIL(&dev->sc_xslist, pendp, link);
		splx(s);
		return(SUCCESSFULLY_QUEUED);
	}
	pendp->xs = NULL;
	dev->sc_xs = xs;
	splx(s);

	/*
	 * nothing is pending do it now.
	 */
	ide_donextcmd(dev);

	if (flags & SCSI_POLL)
		return(COMPLETE);
	return(SUCCESSFULLY_QUEUED);
}

/*
 * entered with dev->sc_xs pointing to the next xfer to perform
 */
void
ide_donextcmd(dev)
	struct idec_softc *dev;
{
	struct scsi_xfer *xs;
	struct scsi_link *slp;
	int flags, phase, stat;

	xs = dev->sc_xs;
	slp = xs->sc_link;
	flags = xs->flags;

	if (flags & SCSI_RESET)
		idereset(dev);

	dev->sc_stat[0] = -1;
	/* Weed out invalid targets & LUNs here */
	if (slp->target > 1 || slp->lun != 0) {
		ide_scsidone(dev, -1);
		return;
	}
	if (flags & SCSI_POLL || ide_no_int)
		stat = ideicmd(dev, slp->target, xs->cmd, xs->cmdlen, 
		    xs->data, xs->datalen/*, phase*/);
	else if (idego(dev, xs) == 0)
		return;
	else 
		stat = dev->sc_stat[0];
	
	ide_scsidone(dev, stat);
}

void
ide_scsidone(dev, stat)
	struct idec_softc *dev;
	int stat;
{
	struct ide_pending *pendp;
	struct scsi_xfer *xs;
	int s, donext;

	xs = dev->sc_xs;
#ifdef DIAGNOSTIC
	if (xs == NULL)
		panic("ide_scsidone");
#endif
#if 1
	/*
	 * XXX Support old-style instrumentation for now.
	 * IS THIS REALLY THE RIGHT PLACE FOR THIS?  --thorpej
	 */
	if (xs->sc_link && xs->sc_link->device_softc &&
	    ((struct device *)(xs->sc_link->device_softc))->dv_unit < dk_ndrive)
		++dk_xfer[((struct device *)(xs->sc_link->device_softc))->dv_unit];
#endif
	/*
	 * is this right?
	 */
	xs->status = stat;

	if (stat == 0)
		xs->resid = 0;
	else {
		switch(stat) {
		case SCSI_CHECK:
			if (stat = idegetsense(dev, xs))
				goto bad_sense;
			xs->error = XS_SENSE;
			break;
		case SCSI_BUSY:
			xs->error = XS_BUSY;
			break;
		bad_sense:
		default:
			xs->error = XS_DRIVER_STUFFUP;
			QPRINTF(("ide_scsicmd() bad %x\n", stat));
			break;
		}
	}
	xs->flags |= ITSDONE;

	/*
	 * grab next command before scsi_done()
	 * this way no single device can hog scsi resources.
	 */
	s = splbio();
	pendp = dev->sc_xslist.tqh_first;
	if (pendp == NULL) {
		donext = 0;
		dev->sc_xs = NULL;
	} else {
		donext = 1;
		TAILQ_REMOVE(&dev->sc_xslist, pendp, link);
		dev->sc_xs = pendp->xs;
		pendp->xs = NULL;
	}
	splx(s);
	scsi_done(xs);

	if (donext)
		ide_donextcmd(dev);
}

int
idegetsense(dev, xs)
	struct idec_softc *dev;
	struct scsi_xfer *xs;
{
	struct scsi_sense rqs;
	struct scsi_link *slp;
	int stat;

	slp = xs->sc_link;
	
	rqs.opcode = REQUEST_SENSE;
	rqs.byte2 = slp->lun << 5;
#ifdef not_yet
	rqs.length = xs->req_sense_length ? xs->req_sense_length : 
	    sizeof(xs->sense);
#else
	rqs.length = sizeof(xs->sense);
#endif
	    
	rqs.unused[0] = rqs.unused[1] = rqs.control = 0;
	
	return(ideicmd(dev, slp->target, &rqs, sizeof(rqs), &xs->sense,
	    rqs.length));
}

#ifdef DEBUG
ide_dump_regs(regs)
	ide_regmap_p regs;
{
	printf ("ide regs: %04x %02x %02x %02x %02x %02x %02x %02x\n",
	    regs->ide_data, regs->ide_error, regs->ide_seccnt,
	    regs->ide_sector, regs->ide_cyl_lo, regs->ide_cyl_hi,
	    regs->ide_sdh, regs->ide_command);
}
#endif

int
idereset(sc)
	struct idec_softc *sc;
{
	return (0);
}

int
idewait (sc, mask)
	struct idec_softc *sc;
	int mask;
{
	ide_regmap_p regs = sc->sc_cregs;
	int timeout = 0;

	if ((regs->ide_status & IDES_BUSY) == 0 &&
	    (regs->ide_status & mask) == mask)
		return (0);
#ifdef DEBUG
	if (ide_debug)
		printf ("idewait busy: %02x\n", regs->ide_status);
#endif
	for (;;) {
		if ((regs->ide_status & IDES_BUSY) == 0 &&
		    (regs->ide_status & mask) == mask)
			break;
		if (regs->ide_status & IDES_ERR)
			break;
		if (++timeout > 10000) {
			printf ("idewait timeout %02x\n", regs->ide_status);
			return (-1);
		}
		delay (1000);
	}
	if (regs->ide_status & IDES_ERR)
		printf ("idewait: error %02x %02x\n", regs->ide_error,
		    regs->ide_status);
#ifdef DEBUG
	else if (ide_debug)
		printf ("idewait delay %d %02x\n", timeout, regs->ide_status);
#endif
	return (regs->ide_status & IDES_ERR);
}

int
idecommand (ide, cylin, head, sector, count, cmd)
	struct ide_softc *ide;
	int cylin, head, sector, count;
	int cmd;
{
	struct idec_softc *idec = (void *)ide->sc_dev.dv_parent;
	ide_regmap_p regs = idec->sc_cregs;
	int stat;

#ifdef DEBUG
	if (ide_debug)
		printf ("idecommand: cmd = %02x\n", cmd);
#endif
	if (wait_for_unbusy(idec) < 0)
		return (-1);
	regs->ide_sdh = 0xa0 | (ide->sc_drive << 4) | head;
	if (cmd == IDEC_DIAGNOSE || cmd == IDEC_IDC)
		stat = wait_for_unbusy(idec);
	else
		stat = idewait(idec, IDES_READY);
	if (stat < 0) printf ("idecommand:%d stat %d\n", ide->sc_drive, stat);
	if (stat < 0)
		return (-1);
	regs->ide_precomp = 0;
	regs->ide_cyl_lo = cylin;
	regs->ide_cyl_hi = cylin >> 8;
	regs->ide_sector = sector;
	regs->ide_seccnt = count;
	regs->ide_command = cmd;
	return (0);
}

int
idegetctlr(dev)
	struct ide_softc *dev;
{
	struct idec_softc *idec = (void *)dev->sc_dev.dv_parent;
	ide_regmap_p regs = idec->sc_cregs;
	char tb[DEV_BSIZE];
	short *tbp = (short *) tb;
	int i;

	if (idecommand(dev, 0, 0, 0, 0, IDEC_READP) != 0 ||
	    wait_for_drq(idec) != 0) {
		return (-1);
	} else {
		for (i = 0; i < DEV_BSIZE / 2; ++i)
			*tbp++ = ntohs(regs->ide_data);
		for (i = 0; i < DEV_BSIZE; i += 2) {
			char temp;
			temp = tb[i];
			tb[i] = tb[i + 1];
			tb[i + 1] = temp;
		}
		bcopy (tb, &dev->sc_params, sizeof (struct ideparams));
		dev->sc_sectors = dev->sc_params.idep_sectors;
		dev->sc_secpercyl = dev->sc_sectors *
		    dev->sc_params.idep_heads;
	}
	return (0);
}

int
ideiread(ide, block, buf, nblks)
	struct ide_softc *ide;
	long block;
	u_char *buf;
	int nblks;
{
	int cylin, head, sector;
	int stat;
	u_short *bufp = (u_short *) buf;
	int i;
	struct idec_softc *idec = (void *) ide->sc_dev.dv_parent;
	ide_regmap_p regs = idec->sc_cregs;

	cylin = block / ide->sc_secpercyl;
	head = (block % ide->sc_secpercyl) / ide->sc_sectors;
	sector = block % ide->sc_sectors + 1;
	stat = idecommand(ide, cylin, head, sector, nblks, IDEC_READ);
	if (stat != 0)
		return (-1);
	while (nblks--) {
		if (wait_for_drq(idec) != 0)
			return (-1);
		for (i = 0; i < DEV_BSIZE / 2 / 16; ++i) {
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
		}
	}
	idec->sc_stat[0] = 0;
	return (0);
}

int
ideiwrite(ide, block, buf, nblks)
	struct ide_softc *ide;
	long block;
	u_char *buf;
	int nblks;
{
	int cylin, head, sector;
	int stat;
	u_short *bufp = (u_short *) buf;
	int i;
	struct idec_softc *idec = (void *) ide->sc_dev.dv_parent;
	ide_regmap_p regs = idec->sc_cregs;

	cylin = block / ide->sc_secpercyl;
	head = (block % ide->sc_secpercyl) / ide->sc_sectors;
	sector = block % ide->sc_sectors + 1;
	stat = idecommand(ide, cylin, head, sector, nblks, IDEC_WRITE);
	if (stat != 0)
		return (-1);
	while (nblks--) {
		if (wait_for_drq(idec) != 0)
			return (-1);
		for (i = 0; i < DEV_BSIZE / 2 / 16; ++i) {
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
		}
		if (wait_for_unbusy(idec) != 0)
			printf ("ideiwrite: timeout waiting for unbusy\n");
	}
	idec->sc_stat[0] = 0;
	return (0);
}

int
ideicmd(dev, target, cbuf, clen, buf, len)
	struct idec_softc *dev;
	int target;
	void *cbuf;
	int clen;
	void *buf;
	int len;
{
	struct ide_softc *ide;
	int i;
	int lba;
	int nblks;
	struct scsi_inquiry_data *inqbuf;
	struct {
		struct scsi_mode_header header;
		struct scsi_blk_desc blk_desc;
		union disk_pages pages;
	} *mdsnbuf;

#ifdef DEBUG
	if (ide_debug > 1)
		printf ("ideicmd: target %d cmd %02x\n", target,
		    *((u_char *)cbuf));
#endif
	if (target > 1)
		return (-1);		/* invalid unit */

	ide = &dev->sc_ide[target];
	if ((ide->sc_flags & IDEF_ALIVE) == 0)
		return (-1);

	if (*((u_char *)cbuf) != REQUEST_SENSE)
		ide->sc_error = 0;
	switch (*((u_char *)cbuf)) {
	case TEST_UNIT_READY:
		dev->sc_stat[0] = 0;
		return (0);

	case INQUIRY:
		dev->sc_stat[0] = idegetctlr(ide);
		if (dev->sc_stat[0] != 0)
			return (dev->sc_stat[0]);
		inqbuf = (void *) buf;
		bzero (buf, len);
		inqbuf->device = 0;	/* XXX fixed disk */
		inqbuf->dev_qual2 = 0;	/* XXX check RMB */
		inqbuf->version = 2;
		inqbuf->response_format = 2;
		inqbuf->additional_length = 31;
		for (i = 0; i < 8; ++i)
			inqbuf->vendor[i] = ide->sc_params.idep_model[i];
		for (i = 0; i < 16; ++i)
			inqbuf->product[i] = ide->sc_params.idep_model[i+8];
		for (i = 0; i < 4; ++i)
			inqbuf->revision[i] = ide->sc_params.idep_rev[i];
		return (0);

	case READ_CAPACITY:
		*((long *)buf) = ide->sc_params.idep_sectors *
		    ide->sc_params.idep_heads *
		    ide->sc_params.idep_fixedcyl - 1;
		*((long *)buf + 1) = 512;	/* XXX 512 byte blocks */
		dev->sc_stat[0] = 0;
		return (0);

	case READ_BIG:
		lba = *((long *)(cbuf + 2));
		nblks = *((u_short *)(cbuf + 7));
		return (ideiread(ide, lba, buf, nblks));

	case READ_COMMAND:
		lba = *((long *)cbuf) & 0x000fffff;
		nblks = *((u_char *)(cbuf + 4));
		return (ideiread(ide, lba, buf, nblks));

	case WRITE_BIG:
		lba = *((long *)(cbuf + 2));
		nblks = *((u_short *)(cbuf + 7));
		return (ideiwrite(ide, lba, buf, nblks));

	case WRITE_COMMAND:
		lba = *((long *)cbuf) & 0x000fffff;
		nblks = *((u_char *)(cbuf + 4));
		return (ideiwrite(ide, lba, buf, nblks));

	case PREVENT_ALLOW:
	case START_STOP:	/* and LOAD */
		dev->sc_stat[0] = 0;
		return (0);

	case MODE_SENSE:
		mdsnbuf = (void*) buf;
		bzero(buf, *((u_char *)cbuf + 4));
		switch (*((u_char *)cbuf + 2) & 0x3f) {
		case 4:
			mdsnbuf->header.data_length = 27;
			mdsnbuf->header.blk_desc_len = 8;
			mdsnbuf->blk_desc.blklen[1] = 512 >> 8;
			mdsnbuf->pages.rigid_geometry.pg_code = 4;
			mdsnbuf->pages.rigid_geometry.pg_length = 16;
			mdsnbuf->pages.rigid_geometry.ncyl_2 =
			    ide->sc_params.idep_fixedcyl >> 16;
			mdsnbuf->pages.rigid_geometry.ncyl_1 =
			    ide->sc_params.idep_fixedcyl >> 8;
			mdsnbuf->pages.rigid_geometry.ncyl_0 =
			    ide->sc_params.idep_fixedcyl;
			mdsnbuf->pages.rigid_geometry.nheads =
			    ide->sc_params.idep_heads;
			dev->sc_stat[0] = 0;
			return (0);
		default:
			printf ("ide: mode sense page %x not simulated\n",
			   *((u_char *)cbuf + 2) & 0x3f);
			return (-1);
		}

	case REQUEST_SENSE:
		/* convert sc_error to SCSI sense */
		bzero (buf, *((u_char *)cbuf + 4));
		*((u_char *) buf) = 0x70;
		*((u_char *) buf + 7) = 10;
		i = 0;
		while (sense_convert[i].ide_err) {
			if (sense_convert[i].ide_err & ide->sc_error)
				break;
			++i;
		}
		*((u_char *) buf + 2) = sense_convert[i].scsi_sense_key;
		*((u_char *) buf + 12) = sense_convert[i].scsi_sense_qual;
		dev->sc_stat[0] = 0;
printf("ide: request sense %02x -> %02x %02x\n", ide->sc_error,
    sense_convert[i].scsi_sense_key, sense_convert[i].scsi_sense_qual);
		return (0);

	case 0x01 /*REWIND*/:
	case 0x04 /*CMD_FORMAT_UNIT*/:
	case 0x05 /*READ_BLOCK_LIMITS*/:
	case REASSIGN_BLOCKS:
	case 0x10 /*WRITE_FILEMARKS*/:
	case 0x11 /*SPACE*/:
	case MODE_SELECT:
	default:
		printf ("ide: unhandled SCSI command %02x\n", *((u_char *)cbuf));
		ide->sc_error = 0x04;
		dev->sc_stat[0] = SCSI_CHECK;
		return (SCSI_CHECK);
	}
}

int
idego(dev, xs)
	struct idec_softc *dev;
	struct scsi_xfer *xs;
{
	struct ide_softc *ide = &dev->sc_ide[xs->sc_link->target];
	char *addr;
	int count;
	long lba;
	int nblks;

#if 0
	cdb->cdb[1] |= unit << 5;
#endif

	ide->sc_buf = xs->data;
	ide->sc_bcount = xs->datalen;
#ifdef DEBUG
	if (ide_debug > 1)
		printf ("ide_go: %02x\n", xs->cmd->opcode);
#endif
	if (xs->cmd->opcode != READ_COMMAND && xs->cmd->opcode != READ_BIG &&
	    xs->cmd->opcode != WRITE_COMMAND && xs->cmd->opcode != WRITE_BIG) {
		ideicmd (dev, xs->sc_link->target, xs->cmd, xs->cmdlen,
		    xs->data, xs->datalen);
		return (1);
	}
	switch (xs->cmd->opcode) {
	case READ_COMMAND:
	case WRITE_COMMAND:
		lba = *((long *)xs->cmd) & 0x000fffff;
		nblks = xs->cmd->bytes[3];
		if (nblks == 0)
			nblks = 256;
		break;
	case READ_BIG:
	case WRITE_BIG:
		lba = *((long *)&xs->cmd->bytes[1]);
		nblks = *((short *)&xs->cmd->bytes[6]);
		break;
	default:
		panic ("idego bad SCSI command");
	}
	ide->sc_blknum = lba;
	ide->sc_blkcnt = nblks;
	ide->sc_skip = ide->sc_mskip = 0;
	dev->sc_flags &= ~IDECF_READ;
	if (xs->cmd->opcode == READ_COMMAND || xs->cmd->opcode == READ_BIG)
		dev->sc_flags |= IDECF_READ;
	dev->sc_cur = ide;
	return (idestart (dev));
}

int
idestart(dev)
	struct idec_softc *dev;
{
	long blknum, cylin, head, sector;
	int command, count;
	struct ide_softc *ide = dev->sc_cur;
	short *bf;
	int i;
	ide_regmap_p regs = dev->sc_cregs;

	dev->sc_flags |= IDECF_ACTIVE;
	blknum = ide->sc_blknum + ide->sc_skip;
	if (ide->sc_mskip == 0) {
		ide->sc_mbcount = ide->sc_bcount;
	}
	cylin = blknum / ide->sc_secpercyl;
	head = (blknum % ide->sc_secpercyl) / ide->sc_sectors;
	sector = blknum % ide->sc_sectors;
	++sector;
	if (ide->sc_mskip == 0 || dev->sc_flags & IDECF_SINGLE) {
		count = howmany(ide->sc_mbcount, DEV_BSIZE);
		command = (dev->sc_flags & IDECF_READ) ?
		    IDEC_READ : IDEC_WRITE;
		if (idecommand(ide, cylin, head, sector, count, command) != 0) {
			printf ("idestart: timeout waiting for unbusy\n");
#if 0
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			idfinish(&dev->sc_ide[0], bp);
#endif
			ide_scsidone(dev, dev->sc_stat[0]);
			return (1);
		}
	}
	dev->sc_stat[0] = 0;
	if (dev->sc_flags & IDECF_READ)
		return (0);
	if (wait_for_drq(dev) < 0) {
		printf ("idestart: timeout waiting for drq\n");
	}
#define W1	(regs->ide_data = *bf++)
	for (i = 0, bf = (short *) (ide->sc_buf + ide->sc_skip * DEV_BSIZE);
	    i < DEV_BSIZE / 2 / 16; ++i) {
		W1; W1; W1; W1; W1; W1; W1; W1;
		W1; W1; W1; W1; W1; W1; W1; W1;
	}
	return (0);
}


int
idesc_intr(dev)
	struct idec_softc *dev;
{
#if 0
	struct idec_softc *dev;
#endif
	ide_regmap_p regs;
	struct ide_softc *ide;
	short dummy;
	short *bf;
	int i;

#if 0
	if (idesccd.cd_ndevs == 0 || (dev = idesccd.cd_devs[0]) == NULL)
		return (0);
#endif
	regs = dev->sc_cregs;
	if (dev->sc_flags & IDECF_A1200) {
		if (*dev->sc_a1200 & 0x80) {
#if 0
			printf ("idesc_intr: A1200 interrupt %x\n", *dev->sc_a1200);
#endif
			dummy = regs->ide_status;	/* XXX */
			*dev->sc_a1200 = 0x7c | (*dev->sc_a1200 & 0x03);
		}
		else
			return (0);
	} else {
		if (regs->ide_intpnd >= 0)
			return (0);
		dummy = regs->ide_status;
	}
#ifdef DEBUG
	if (ide_debug)
		printf ("idesc_intr: %02x\n", dummy);
#endif
	if ((dev->sc_flags & IDECF_ACTIVE) == 0)
		return (1);
	dev->sc_flags &= ~IDECF_ACTIVE;
	if (wait_for_unbusy(dev) < 0)
		printf ("idesc_intr: timeout waiting for unbusy\n");
	ide = dev->sc_cur;
	if (dummy & IDES_ERR) {
		dev->sc_stat[0] = SCSI_CHECK;
		ide->sc_error = regs->ide_error;
printf("idesc_intr: error %02x, %02x\n", dev->sc_stat[1], dummy);
		ide_scsidone(dev, dev->sc_stat[0]);
	}
	if (dev->sc_flags & IDECF_READ) {
#define R2 (*bf++ = regs->ide_data)
		bf = (short *) (ide->sc_buf + ide->sc_skip * DEV_BSIZE);
		if (wait_for_drq(dev) != 0)
			printf ("idesc_intr: read error detected late\n");
		for (i = 0; i < DEV_BSIZE / 2 / 16; ++i) {
			R2; R2; R2; R2; R2; R2; R2; R2;
			R2; R2; R2; R2; R2; R2; R2; R2;
		}
	}
	ide->sc_skip++;
	ide->sc_mskip++;
	ide->sc_bcount -= DEV_BSIZE;
	ide->sc_mbcount -= DEV_BSIZE;
#ifdef DEBUG
	if (ide_debug)
		printf ("idesc_intr: sc_bcount %d\n", ide->sc_bcount);
#endif
	if (ide->sc_bcount == 0)
		ide_scsidone(dev, dev->sc_stat[0]);
	else
		/* Check return value here? */
		idestart (dev);
	return (1);
}	
