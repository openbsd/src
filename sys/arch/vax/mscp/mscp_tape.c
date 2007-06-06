/*	$OpenBSD: mscp_tape.c,v 1.10 2007/06/06 17:15:13 deraadt Exp $ */
/*	$NetBSD: mscp_tape.c,v 1.16 2001/11/13 07:38:28 lukem Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
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
 * MSCP tape device driver
 */

/*
 * TODO
 *	Write status handling code.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/ioccom.h>
#include <sys/mtio.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <arch/vax/mscp/mscp.h>
#include <arch/vax/mscp/mscpreg.h>
#include <arch/vax/mscp/mscpvar.h>

/*
 * Drive status, per drive
 */
struct mt_softc {
	struct	device mt_dev;	/* Autoconf struct */
	int	mt_state;	/* open/closed state */
	int	mt_hwunit;	/* Hardware unit number */
	int	mt_inuse;	/* Locks the tape drive for others */
	int	mt_waswrite;	/* Last operation was a write op */
	int	mt_serex;	/* Got serious exception */
	int	mt_ioctlerr;	/* Error after last ioctl */
};

#define MT_OFFLINE	0
#define MT_ONLINE	1

int	mtmatch(struct device *, struct cfdata *, void *);
void	mtattach(struct device *, struct device *, void *);
void	mtdgram(struct device *, struct mscp *, struct mscp_softc *);
void	mtiodone(struct device *, struct buf *);
int	mtonline(struct device *, struct mscp *);
int	mtgotstatus(struct device *, struct mscp *);
int	mtioerror(struct device *, struct mscp *, struct buf *);
void	mtfillin(struct buf *, struct mscp *);
int	mtopen(dev_t, int, int, struct proc *);
int	mtclose(dev_t, int, int, struct proc *);
void	mtstrategy(struct buf *);
int	mtread(dev_t, struct uio *);
int	mtwrite(dev_t, struct uio *);
int	mtioctl(dev_t, int, caddr_t, int, struct proc *);
int	mtdump(dev_t, daddr64_t, caddr_t, size_t);
int	mtcmd(struct mt_softc *, int, int, int);
void	mtcmddone(struct device *, struct mscp *);
int	mt_putonline(struct mt_softc *);

struct	mscp_device mt_device = {
	mtdgram,
	mtiodone,
	mtonline,
	mtgotstatus,
	0,
	mtioerror,
	0,
	mtfillin,
	mtcmddone,
};

/* This is not good, should allow more than 4 tapes/device type */
#define mtunit(dev)	(minor(dev) & T_UNIT)
#define mtnorewind(dev) (dev & T_NOREWIND)
#define mthdensity(dev) (dev & T_1600BPI)

struct	cfattach mt_ca = {
	sizeof(struct mt_softc), (cfmatch_t)mtmatch, mtattach
};

struct cfdriver mt_cd = {
	NULL, "mt", DV_TAPE
};

/*
 * More driver definitions, for generic MSCP code.
 */

int
mtmatch(parent, cf, aux)
	struct	device *parent;
	struct	cfdata *cf;
	void	*aux;
{
	struct	drive_attach_args *da = aux;
	struct	mscp *mp = da->da_mp;

	if ((da->da_typ & MSCPBUS_TAPE) == 0)
		return 0;
	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != mp->mscp_unit)
		return 0;
	return 1;
}

/*
 * The attach routine only checks and prints drive type.
 */
void
mtattach(parent, self, aux)
	struct	device *parent, *self;
	void	*aux; 
{
	struct	mt_softc *mt = (void *)self;
	struct	drive_attach_args *da = aux;
	struct	mscp *mp = da->da_mp;
	struct	mscp_softc *mi = (void *)parent;

	mt->mt_hwunit = mp->mscp_unit;
	mi->mi_dp[mp->mscp_unit] = self;

	disk_printtype(mp->mscp_unit, mp->mscp_guse.guse_mediaid);
}

/* 
 * (Try to) put the drive online. This is done the first time the
 * drive is opened, or if it has fallen offline.
 */
int
mt_putonline(mt)
	struct mt_softc *mt;
{
	struct	mscp *mp;
	struct	mscp_softc *mi = (struct mscp_softc *)mt->mt_dev.dv_parent;
	volatile int i;

	(volatile int)mt->mt_state = MT_OFFLINE;
	mp = mscp_getcp(mi, MSCP_WAIT);
	mp->mscp_opcode = M_OP_ONLINE;
	mp->mscp_unit = mt->mt_hwunit;
	mp->mscp_cmdref = (long)&mt->mt_state;
	*mp->mscp_addr |= MSCP_OWN | MSCP_INT;

	/* Poll away */
	i = bus_space_read_2(mi->mi_iot, mi->mi_iph, 0);
	if (tsleep(&mt->mt_state, PRIBIO, "mtonline", 240 * hz))
		return MSCP_FAILED;

	if ((volatile int)mt->mt_state != MT_ONLINE)
		return MSCP_FAILED;

	return MSCP_DONE;
}
/*
 * Open a drive.
 */
/*ARGSUSED*/
int
mtopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct	proc *p;
{
	struct mt_softc *mt;
	int unit;

	/*
	 * Make sure this is a reasonable open request.
	 */
	unit = mtunit(dev);
	if (unit >= mt_cd.cd_ndevs)
		return ENXIO;
	mt = mt_cd.cd_devs[unit];
	if (mt == 0)
		return ENXIO;

	if (mt->mt_inuse)
			return EBUSY;
	mt->mt_inuse = 1;

	if (mt_putonline(mt) == MSCP_FAILED) {
		mt->mt_inuse = 0;
		return EIO;
	}

	return 0;
}

/* ARGSUSED */
int
mtclose(dev, flags, fmt, p)
	dev_t dev;
	int flags, fmt;
	struct	proc *p;
{
	int unit = mtunit(dev);
	struct mt_softc *mt = mt_cd.cd_devs[unit];

	/*
	 * If we just have finished a writing, write EOT marks.
	 */
	if ((flags & FWRITE) && mt->mt_waswrite) {
		mtcmd(mt, MTWEOF, 0, 0);
		mtcmd(mt, MTWEOF, 0, 0);
		mtcmd(mt, MTBSR, 1, 0);
	}
	if (mtnorewind(dev) == 0)
		mtcmd(mt, MTREW, 0, 1);
	if (mt->mt_serex)
		mtcmd(mt, -1, 0, 0);

	mt->mt_inuse = 0; /* Release the tape */
	return 0;
}

void
mtstrategy(bp)
	struct buf *bp;
{
	int unit;
	struct mt_softc *mt;
	int s;

	/*
	 * Make sure this is a reasonable drive to use.
	 */
	unit = mtunit(bp->b_dev);
	if (unit >= mt_cd.cd_ndevs || (mt = mt_cd.cd_devs[unit]) == NULL) {
		bp->b_error = ENXIO;
		goto bad;
	}

	mt->mt_waswrite = bp->b_flags & B_READ ? 0 : 1;
	mscp_strategy(bp, mt->mt_dev.dv_parent);
	return;

bad:
	bp->b_flags |= B_ERROR;
	s = splbio();
	biodone(bp);
	splx(s);
}

int
mtread(dev, uio)
	dev_t dev;
	struct uio *uio;
{

	return (physio(mtstrategy, NULL, dev, B_READ, minphys, uio));
}

int
mtwrite(dev, uio)
	dev_t dev;
	struct uio *uio;
{

	return (physio(mtstrategy, NULL, dev, B_WRITE, minphys, uio));
}

void
mtiodone(usc, bp)
	struct device *usc;
	struct buf *bp;
{
	int s;

	s = splbio();
	biodone(bp);
	splx(s);
}

/*
 * Fill in drive addresses in a mscp packet waiting for transfer.
 */
void
mtfillin(bp, mp)
	struct buf *bp;
	struct mscp *mp;
{
	int unit = mtunit(bp->b_dev);
	struct mt_softc *mt = mt_cd.cd_devs[unit];

	mp->mscp_unit = mt->mt_hwunit;
	if (mt->mt_serex == 2) {
		mp->mscp_modifier = M_MD_CLSEX;
		mt->mt_serex = 0;
	} else
		mp->mscp_modifier = 0;

	mp->mscp_seq.seq_bytecount = bp->b_bcount;
}

/*
 * Handle an error datagram.
 */
void
mtdgram(usc, mp, mi)
	struct device *usc;
	struct mscp *mp;
	struct mscp_softc *mi;
{
	if (mscp_decodeerror(usc == NULL?"unconf mt" : usc->dv_xname, mp, mi))
		return;
}

/*
 * A drive came on line, make sure it really _is_ on line before
 * trying to use it.
 */
int
mtonline(usc, mp)
	struct device *usc;
	struct mscp *mp;
{
	struct mt_softc *mt = (void *)usc;

	wakeup((caddr_t)&mt->mt_state);
	if ((mp->mscp_status & M_ST_MASK) == M_ST_SUCCESS) 
		mt->mt_state = MT_ONLINE;

	return (MSCP_DONE);
}

/*
 * We got some (configured) unit's status.  Return DONE.
 */
int
mtgotstatus(usc, mp)
	struct device *usc;
	struct mscp *mp;
{
	return (MSCP_DONE);
}

static char *mt_ioerrs[] = {
	"invalid command",	/* 1 M_ST_INVALCMD */
	"command aborted",	/* 2 M_ST_ABORTED */
	"unit offline",		/* 3 M_ST_OFFLINE */
	"unknown",		/* 4 M_ST_AVAILABLE */
	"unknown",		/* 5 M_ST_MFMTERR */
	"unit write protected", /* 6 M_ST_WRPROT */
	"compare error",	/* 7 M_ST_COMPERR */
	"data error",		/* 8 M_ST_DATAERR */
	"host buffer access error",	/* 9 M_ST_HOSTBUFERR */
	"controller error",	/* 10 M_ST_CTLRERR */
	"drive error",		/* 11 M_ST_DRIVEERR */
	"formatter error",	/* 12 M_ST_FORMATTERR */
	"BOT encountered",	/* 13 M_ST_BOT */
	"tape mark encountered",/* 14 M_ST_TAPEMARK */
	"unknown",		/* 15 */
	"record data truncated",/* 16 M_ST_RDTRUNC */
};

/*
 * An I/O error, may be because of a tapemark encountered.
 * Check that before failing.
 */
/*ARGSUSED*/
int
mtioerror(usc, mp, bp)
	struct device *usc;
	struct mscp *mp;
	struct buf *bp;
{
	struct mt_softc *mt = (void *)usc;
	int st = mp->mscp_status & M_ST_MASK;

	if (mp->mscp_flags & M_EF_SEREX)
		mt->mt_serex = 1;
	if (st == M_ST_TAPEMARK)
		mt->mt_serex = 2;
	else {
		if (st && st < 17)
			printf("%s: error %d (%s)\n", mt->mt_dev.dv_xname, st,
			    mt_ioerrs[st-1]);
		else
			printf("%s: error %d\n", mt->mt_dev.dv_xname, st);
		bp->b_flags |= B_ERROR;
		bp->b_error = EROFS;
	}

	return (MSCP_DONE);
}

/*
 * I/O controls.
 */
int
mtioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = mtunit(dev);
	struct mt_softc *mt = mt_cd.cd_devs[unit];
	struct	mtop *mtop;
	struct	mtget *mtget;
	int error = 0, count;

	count = mtop->mt_count;

	switch (cmd) {

	case MTIOCTOP:
		mtop = (void *)data;
		if (mtop->mt_op == MTWEOF) {
			while (mtop->mt_count-- > 0)
				if ((error = mtcmd(mt, mtop->mt_op, 0, 0)))
					break;
		} else
			error = mtcmd(mt, mtop->mt_op, mtop->mt_count, 0);

	case MTIOCGET:
		mtget = (void *)data;
		mtget->mt_type = MT_ISTMSCP;
		/* XXX we need to fill in more fields here */
		break;

	default:
		error = ENXIO;
		break;
	}
	return (error);
}

/*
 * No crash dump support...
 */
int
mtdump(dev, blkno, va, size)
	dev_t	dev;
	daddr64_t blkno;
	caddr_t va;
	size_t	size;
{
	return -1;
}

/*
 * Send a command to the tape drive. Wait until the command is
 * finished before returning.
 * This routine must only be called when there are no data transfer
 * active on this device. Can we be sure of this? Or does the ctlr
 * queue up all command packets and take them in sequential order?
 * It sure would be nice if my manual stated this... /ragge
 */
int
mtcmd(mt, cmd, count, complete)
	struct mt_softc *mt;
	int cmd, count, complete;
{
	struct mscp *mp;
	struct mscp_softc *mi = (void *)mt->mt_dev.dv_parent;
	volatile int i;

	mp = mscp_getcp(mi, MSCP_WAIT);

	mt->mt_ioctlerr = 0;
	mp->mscp_unit = mt->mt_hwunit;
	mp->mscp_cmdref = -1;
	*mp->mscp_addr |= MSCP_OWN | MSCP_INT;

	switch (cmd) {
	case MTWEOF:
		mp->mscp_opcode = M_OP_WRITM;
		break;

	case MTBSF:
		mp->mscp_modifier = M_MD_REVERSE;
	case MTFSF:
		mp->mscp_opcode = M_OP_POS;
		mp->mscp_seq.seq_buffer = count;
		break;

	case MTBSR:
		mp->mscp_modifier = M_MD_REVERSE;
	case MTFSR:
		mp->mscp_opcode = M_OP_POS;
		mp->mscp_modifier |= M_MD_OBJCOUNT;
		mp->mscp_seq.seq_bytecount = count;
		break;

	case MTREW:
		mp->mscp_opcode = M_OP_POS;
		mp->mscp_modifier = M_MD_REWIND | M_MD_CLSEX;
		if (complete)
			mp->mscp_modifier |= M_MD_IMMEDIATE;
		mt->mt_serex = 0;
		break;

	case MTOFFL:
		mp->mscp_opcode = M_OP_AVAILABLE;
		mp->mscp_modifier = M_MD_UNLOAD | M_MD_CLSEX;
		mt->mt_serex = 0;
		break;

	case MTNOP:
		mp->mscp_opcode = M_OP_GETUNITST;
		break;

	case -1: /* Clear serious exception only */
		mp->mscp_opcode = M_OP_POS;
		mp->mscp_modifier = M_MD_CLSEX;
		mt->mt_serex = 0;
		break;

	default:
		printf("Bad ioctl %x\n", cmd);
		mp->mscp_opcode = M_OP_POS;
		break;
	}

	i = bus_space_read_2(mi->mi_iot, mi->mi_iph, 0);
	tsleep(&mt->mt_inuse, PRIBIO, "mtioctl", 0);
	return mt->mt_ioctlerr;
}

/*
 * Called from bus routines whenever a non-data transfer is finished.
 */
void
mtcmddone(usc, mp)
	struct device *usc;
	struct mscp *mp;
{
	struct mt_softc *mt = (void *)usc;

	if (mp->mscp_status) {
		mt->mt_ioctlerr = EIO;
		printf("%s: bad status %x\n", mt->mt_dev.dv_xname,
		    mp->mscp_status);
	}
	wakeup(&mt->mt_inuse);
}
