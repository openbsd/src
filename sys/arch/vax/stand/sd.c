/*	$OpenBSD: sd.c,v 1.3 1998/05/13 07:30:27 niklas Exp $	*/
/*	$NetBSD: sd.c,v 1.1 1996/08/02 11:22:36 ragge Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory and the Systems
 * Programming Group of the University of Utah Computer Science Department.
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
 * from: Utah $Hdr: sd.c 1.9 92/12/21$
 *
 *	@(#)sd.c	8.1 (Berkeley) 6/10/93
 */

/*
 * SCSI CCS disk driver
 */

#include <sys/param.h>
#include <sys/disklabel.h>
#include "stand.h"
#include "samachdep.h"

#define SC_DEBUG	1	/* bertram */
#define SD_DEBUG	1	/* bertram */

/*----------------------------------------------------------------------*/
int
scsialive(int ctlr)
{
	return 1;		/* controller always alive! */
}

/* call functions in scsi_hi.c */
#include "so.h"

int
scsi_tt_read(ctlr, slave, buf, len, blk, nblk)
	int ctlr, slave;
	u_char *buf;
	u_int len;
	daddr_t blk;
	u_int nblk;
{
#ifdef SC_DEBUG
printf("scsi_tt_read: ctlr %d, slave %d, len %d, blk %d, nblk %d\n",
	ctlr, slave, len, blk, nblk );
#endif
	if (sc_rdwt(DISK_READ, blk, buf, nblk, 1<<slave, 0) == 0)
		return 0;
	return -2;
}

int
scsi_tt_write(ctlr, slave, buf, len, blk, nblk)
	int ctlr, slave;
	u_char *buf;
	u_int len;
	daddr_t blk;
	u_int nblk;
{
#ifdef SC_DEBUG
printf("scsi_tt_write: ctlr %d, slave %d, len %d, blk %d, nblk %d\n",
	ctlr, slave, len, blk, nblk );
#endif
#if 0
	if (sc_rdwt(DISK_WRITE, blk, buf, nblk, 1<<slave, 0) == 0)
		return 0;
#endif
	return -2;
}

/*----------------------------------------------------------------------*/

struct	sd_softc {
	int	sc_ctlr;
	int	sc_unit;
	int	sc_part;
	char	sc_retry;
	char	sc_alive;
	struct	disklabel sc_label;
} sd_softc[NSCSI][NSD];

#ifdef SD_DEBUG
int debug = SD_DEBUG;
#endif

#define	SDRETRY		2

sdinit(ctlr, unit)
	int ctlr, unit;
{
	register struct sd_softc *ss = &sd_softc[ctlr][unit];

	/* HP version does test_unit_ready
 	 * followed by read_capacity to get blocksize
	 */
	ss->sc_alive = 1;
	return (1);
}

sdreset(ctlr, unit)
	int ctlr, unit;
{
}

char io_buf[DEV_BSIZE];

sdgetinfo(ss)
	register struct sd_softc *ss;
{
	register struct disklabel *lp;
	char *msg, *getdisklabel();
	int sdstrategy(), i, err;

	lp = &sd_softc[ss->sc_ctlr][ss->sc_unit].sc_label;
	bzero((caddr_t)lp, sizeof *lp);
	lp->d_secsize = DEV_BSIZE;
	lp->d_secpercyl = 1;
	lp->d_npartitions = MAXPARTITIONS;
	lp->d_partitions[ss->sc_part].p_offset = 0;
	lp->d_partitions[ss->sc_part].p_size = 0x7fffffff;

	if (err = sdstrategy(ss, F_READ,
		       LABELSECTOR, DEV_BSIZE, io_buf, &i) < 0) {
	    printf("sdgetinfo: sdstrategy error %d\n", err);
	    return 0;
	}
	
	msg = getdisklabel(io_buf, lp);
	if (msg) {
		printf("sd(%d,%d,%d): %s\n",
		       ss->sc_ctlr, ss->sc_unit, ss->sc_part, msg);
		return 0;
	}
	return(1);
}

sdopen(f, ctlr, unit, part)
	struct open_file *f;
	int ctlr, unit, part;
{
	register struct sd_softc *ss;
	register struct disklabel *lp;

#ifdef SD_DEBUG
	if (debug)
	printf("sdopen: ctlr=%d unit=%d part=%d\n",
	    ctlr, unit, part);
#endif
	
	if (ctlr >= NSCSI || !scsialive(ctlr))
		return (EADAPT);
	if (unit >= NSD)
		return (ECTLR);
	ss = &sd_softc[ctlr][unit];	/* XXX alloc()? keep pointers? */
	ss->sc_part = part;
	ss->sc_unit = unit;
	ss->sc_ctlr = ctlr;
	if (!ss->sc_alive) {
		if (!sdinit(ctlr, unit))
			return (ENXIO);
		if (!sdgetinfo(ss))
			return (ERDLAB);
	}
	lp = &sd_softc[ctlr][unit].sc_label;
	if (part >= lp->d_npartitions || lp->d_partitions[part].p_size == 0)
		return (EPART);

	f->f_devdata = (void *)ss;
	return (0);
}

int
sdstrategy(ss, func, dblk, size, buf, rsize)
	register struct sd_softc *ss;
	int func;
	daddr_t dblk;		/* block number */
	u_int size;		/* request size in bytes */
	char *buf;
	u_int *rsize;		/* out: bytes transferred */
{
	register int ctlr = ss->sc_ctlr;
	register int unit = ss->sc_unit;
 	register int part = ss->sc_part;
	register struct partition *pp = &ss->sc_label.d_partitions[part];
	u_int nblk = size >> DEV_BSHIFT;
	u_int blk = dblk + pp->p_offset;
	char stat;

	if (size == 0)
		return(0);

	ss->sc_retry = 0;

#ifdef SD_DEBUG
	if (debug)
	    printf("sdstrategy(%d,%d): size=%d blk=%d nblk=%d\n",
		ctlr, unit, size, blk, nblk);
#endif

retry:
	if (func == F_READ)
		stat = scsi_tt_read(ctlr, unit, buf, size, blk, nblk);
	else
		stat = scsi_tt_write(ctlr, unit, buf, size, blk, nblk);
	if (stat) {
		printf("sd(%d,%d,%d): block=%x, error=0x%x\n",
		       ctlr, unit, ss->sc_part, blk, stat);
		if (++ss->sc_retry > SDRETRY)
			return(EIO);
		goto retry;
	}
	*rsize = size;
	
	return(0);
}
