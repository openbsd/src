/*	$OpenBSD: cfl.c,v 1.6 2006/01/20 23:27:26 miod Exp $	*/
/*	$NetBSD: cfl.c,v 1.2 1998/04/13 12:10:26 ragge Exp $	*/
/*-
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)crl.c	7.5 (Berkeley) 5/9/91
 */

/*
 * Console floppy driver for 11/780.
 *	XXX - Does not work. (Not completed)
 *	Included here if someone wants to finish it.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>

#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/scb.h>

#include <vax/vax/gencons.h>

#define	CFL_TRACKS	77
#define	CFL_SECTORS	26
#define	CFL_BYTESPERSEC	128
#define	CFL_MAXSEC	(CFL_TRACKS * CFL_SECTORS)

#define	FLOP_READSECT	0x900
#define	FLOP_WRITSECT	0x901
#define	FLOP_DATA	0x100
#define	FLOP_COMPLETE	0x200

struct {
	short	cfl_state;		/* open and busy flags */
	short	cfl_active;		/* driver state flag */
	struct	buf *cfl_buf;		/* buffer we're using */
	unsigned char *cfl_xaddr;		/* transfer address */
	short	cfl_errcnt;
} cfltab;

#define	IDLE	0
#define	OPEN	1
#define	BUSY	2

#define	CFL_IDLE	0
#define	CFL_START	1
#define	CFL_SECTOR	2
#define	CFL_DATA	3
#define	CFL_TRACK	4
#define	CFL_NEXT	5
#define	CFL_FINISH	6
#define	CFL_GETIN	7

static	void cflstart(void);

int	cflopen(dev_t, int, struct proc *);
int	cflclose(dev_t, int, struct proc *);
int	cflrw(dev_t, struct uio *, int);

/*ARGSUSED*/
int
cflopen(dev, flag, p)
	dev_t dev;
	int flag;
	struct proc *p;
{
	if (vax_cputype != VAX_780)
		return (ENXIO);
	if (cfltab.cfl_state != IDLE)
		return (EALREADY);
	cfltab.cfl_state = OPEN;
	cfltab.cfl_buf = geteblk(512);
	return (0);
}

/*ARGSUSED*/
int
cflclose(dev, flag, p)
	dev_t dev;
	int flag;
	struct proc *p;
{

	brelse(cfltab.cfl_buf);
	cfltab.cfl_state = IDLE;
	return 0;
}

/*ARGSUSED*/
int
cflrw(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct buf *bp;
	register int i;
	register int s;
	int error;

	if (uio->uio_resid == 0) 
		return (0);
	s = spl4();
	while (cfltab.cfl_state == BUSY)
		tsleep((caddr_t)&cfltab, PRIBIO, "cflrw", 0);
	cfltab.cfl_state = BUSY;
	splx(s);

	bp = cfltab.cfl_buf;
	error = 0;
	while ((i = imin(CFL_BYTESPERSEC, uio->uio_resid)) > 0) {
		bp->b_blkno = uio->uio_offset>>7;
		if (bp->b_blkno >= CFL_MAXSEC ||
		    (uio->uio_offset & 0x7F) != 0) {
			error = EIO;
			break;
		}
		if (uio->uio_rw == UIO_WRITE) {
			error = uiomove(bp->b_data, i, uio);
			if (error)
				break;
		}
		bp->b_flags = uio->uio_rw == UIO_WRITE ? B_WRITE : B_READ;
		s = spl4(); 
		cflstart();
		while ((bp->b_flags & B_DONE) == 0)
			tsleep((caddr_t)bp, PRIBIO, "cflrw", 0);	
		splx(s);
		if (bp->b_flags & B_ERROR) {
			error = EIO;
			break;
		}
		if (uio->uio_rw == UIO_READ) {
			error = uiomove(bp->b_data, i, uio);
			if (error)
				break;
		}
	}
	cfltab.cfl_state = OPEN;
	wakeup((caddr_t)&cfltab);
	return (error);
}

void
cflstart()
{
	register struct buf *bp;

	bp = cfltab.cfl_buf;
	cfltab.cfl_errcnt = 0;
	cfltab.cfl_xaddr = (unsigned char *) bp->b_data;
	cfltab.cfl_active = CFL_START;
	bp->b_resid = 0;

	if ((mfpr(PR_TXCS) & GC_RDY) == 0)
		/* not ready to receive order */
		return;

	cfltab.cfl_active = CFL_SECTOR;
	mtpr(bp->b_flags & B_READ ? FLOP_READSECT : FLOP_WRITSECT, PR_TXDB);

#ifdef lint
	cflintr();
#endif
}

void cfltint(int);

void
cfltint(arg)
	int arg;
{
	register struct buf *bp = cfltab.cfl_buf;

	switch (cfltab.cfl_active) {
	case CFL_START:/* do a read */
		mtpr(bp->b_flags & B_READ ? FLOP_READSECT : FLOP_WRITSECT,
		    PR_TXDB);
		cfltab.cfl_active = CFL_SECTOR;
		break;

	case CFL_SECTOR:/* send sector */
		mtpr(FLOP_DATA | (int)bp->b_blkno % (CFL_SECTORS + 1), PR_TXDB);
		cfltab.cfl_active = CFL_TRACK;
		break;

	case CFL_TRACK:
		mtpr(FLOP_DATA | (int)bp->b_blkno / CFL_SECTORS, PR_TXDB);
		cfltab.cfl_active = CFL_NEXT;
		break;

	case CFL_NEXT:
		mtpr(FLOP_DATA | *cfltab.cfl_xaddr++, PR_TXDB);
		if (--bp->b_bcount == 0)
			cfltab.cfl_active = CFL_FINISH;
		break;

	}
}

void cflrint(int);

void
cflrint(ch)
	int ch;
{
	struct buf *bp = cfltab.cfl_buf;

	switch (cfltab.cfl_active) {
	case CFL_NEXT:
		if ((bp->b_flags & B_READ) == B_READ)
			cfltab.cfl_active = CFL_GETIN;
		else {
			cfltab.cfl_active = CFL_IDLE;
			bp->b_flags |= B_DONE;
			wakeup(bp);
		}
		break;

	case CFL_GETIN:
		*cfltab.cfl_xaddr++ = ch & 0377;
		if (--bp->b_bcount==0) {
			cfltab.cfl_active = CFL_IDLE;
			bp->b_flags |= B_DONE;
			wakeup(bp);
		}
		break;
	}
}
