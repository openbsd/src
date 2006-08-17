/*	$OpenBSD: sd.c,v 1.6 2006/08/17 06:31:10 miod Exp $	*/
/*	$NetBSD: sd.c,v 1.9 1996/12/21 21:34:41 thorpej Exp $	*/

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
 * from: Utah $Hdr: sd.c 1.9 92/12/21$
 *
 *	@(#)sd.c	8.1 (Berkeley) 6/10/93
 */

/*
 * SCSI CCS disk driver
 */

#include <sys/param.h>
#include <sys/disklabel.h>

#include <lib/libsa/stand.h>

#include "samachdep.h"
#include "scsireg.h"

struct	disklabel sdlabel;

struct	sdminilabel {
	u_short	npart;
	u_long	offset[MAXPARTITIONS];
};

struct	sd_softc {
	int	sc_ctlr;
	int	sc_unit;
	int	sc_part;
	char	sc_retry;
	char	sc_alive;
	short	sc_blkshift;
	struct	sdminilabel sc_pinfo;
} sd_softc[NSCSI][NSD];

#define	SDRETRY		2

int	sdclose(struct open_file *);
int	sdgetinfo(struct sd_softc *);
int	sdinit(int, int);
int	sdopen(struct open_file *, int, int, int);
void	sdreset(int, int);
int	sdstrategy(struct sd_softc *, int, daddr_t, size_t, void *, size_t *);

int
sdinit(int ctlr, int unit)
{
	struct sd_softc *ss = &sd_softc[ctlr][unit];
	u_char stat;
	int capbuf[2];

	stat = scsi_test_unit_rdy(ctlr, unit);
	if (stat) {
		/* drive may be doing RTZ - wait a bit */
		if (stat == STS_CHECKCOND) {
			DELAY(1000000);
			stat = scsi_test_unit_rdy(ctlr, unit);
		}
		if (stat) {
			printf("sd(%d,%d,0,0): init failed (stat=%x)\n",
			       ctlr, unit, stat);
			return (0);
		}
	}
	/*
	 * try to get the drive block size.
	 */
	capbuf[0] = 0;
	capbuf[1] = 0;
	stat = scsi_read_capacity(ctlr, unit,
				  (u_char *)capbuf, sizeof(capbuf));
	if (stat == 0) {
		if (capbuf[1] > DEV_BSIZE)
			for (; capbuf[1] > DEV_BSIZE; capbuf[1] >>= 1)
				++ss->sc_blkshift;
	}
	ss->sc_alive = 1;
	return (1);
}

void
sdreset(int ctlr, int unit)
{
}

char io_buf[MAXBSIZE];

int
sdgetinfo(struct sd_softc *ss)
{
	struct sdminilabel *pi = &ss->sc_pinfo;
	struct disklabel *lp = &sdlabel;
	char *msg;
	int err, savepart;
	size_t i;

	bzero((caddr_t)lp, sizeof *lp);
	lp->d_secsize = (DEV_BSIZE << ss->sc_blkshift);

	/* Disklabel is always from RAW_PART. */
	savepart = ss->sc_part;
	ss->sc_part = RAW_PART;
	err = sdstrategy(ss, F_READ, LABELSECTOR,
	    lp->d_secsize ? lp->d_secsize : DEV_BSIZE, io_buf, &i);
	ss->sc_part = savepart;

	if (err) {
		printf("sdgetinfo: sdstrategy error %d\n", err);
		return(0);
	}

	msg = getdisklabel(io_buf, lp);
	if (msg) {
		printf("sd(%d,%d,%d): WARNING: %s, ",
		       ss->sc_ctlr, ss->sc_unit, ss->sc_part, msg);
		printf("defining `c' partition as entire disk\n");
		pi->npart = 3;
#ifdef CD9660_DUMMYLABEL
		pi->offset[0] = 0;
		lp->d_partitions[0].p_fstype = FS_ISO9660; /* just for kicks */
#else
		pi->offset[0] = -1;
#endif
		pi->offset[1] = -1;
		pi->offset[2] = 0;
	} else {
		pi->npart = lp->d_npartitions;
		for (i = 0; i < pi->npart; i++)
			pi->offset[i] = lp->d_partitions[i].p_size == 0 ?
				-1 : lp->d_partitions[i].p_offset;
	}
	return(1);
}

int
sdopen(struct open_file *f, int ctlr, int unit, int part)
{
	struct sd_softc *ss;

#ifdef SD_DEBUG
	if (debug)
	printf("sdopen: ctlr=%d unit=%d part=%d\n",
	    ctlr, unit, part);
#endif

	if (ctlr >= NSCSI || scsialive(ctlr) == 0)
		return (EADAPT);
	if (unit >= NSD)
		return (ECTLR);
	ss = &sd_softc[ctlr][unit];
	ss->sc_part = part;
	ss->sc_unit = unit;
	ss->sc_ctlr = ctlr;
	if (ss->sc_alive == 0) {
		if (sdinit(ctlr, unit) == 0)
			return (ENXIO);
		if (sdgetinfo(ss) == 0)
			return (ERDLAB);
	}
	if (part != RAW_PART &&     /* always allow RAW_PART to be opened */
	    (part >= ss->sc_pinfo.npart || ss->sc_pinfo.offset[part] == -1))
		return (EPART);
	f->f_devdata = (void *)ss;
	return (0);
}

int
sdclose(struct open_file *f)
{
	struct sd_softc *ss = f->f_devdata;

	/*
	 * Mark the disk `not alive' so that the disklabel
	 * will be re-loaded at next open.
	 */
	bzero(ss, sizeof(sd_softc));
	f->f_devdata = NULL;

	return (0);
}

int
sdstrategy(struct sd_softc *ss, int func, daddr_t dblk, size_t size,
    void *v_buf, size_t *rsize)
{
	char *buf = v_buf;
	int ctlr = ss->sc_ctlr;
	int unit = ss->sc_unit;
	u_int nblk = size >> ss->sc_blkshift;
	daddr_t blk;
	char stat;

	if (size == 0)
		return(0);

	/*
	 * Don't do partition translation on the `raw partition'.
	 */
	blk = (dblk + ((ss->sc_part == RAW_PART) ? 0 :
	    ss->sc_pinfo.offset[ss->sc_part])) >> ss->sc_blkshift;

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
