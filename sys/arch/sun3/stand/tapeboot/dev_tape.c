/*	$NetBSD: dev_tape.c,v 1.2 1995/10/17 22:58:20 gwr Exp $	*/

/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
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
 * This module implements a "raw device" interface suitable for
 * use by the stand-alone I/O library UFS file-system code, and
 * possibly for direct access (i.e. boot from tape).
 *
 * The implementation is deceptively simple because it uses the
 * drivers provided by the Sun PROM monitor.  Note that only the
 * PROM driver used to load the boot program is available here.
 */

#include <sys/types.h>
#include <machine/mon.h>
#include <machine/saio.h>

#include "stand.h"

#include "dvma.h"
#include "promdev.h"
/* #include "dev_tape.h" XXX - needs stdarg.h */

extern int debug;

struct saioreq tape_ioreq;

/*
 * This is a special version of devopen() for tape boot.
 * In this version, the file name is a numeric string of
 * one digit, which is passed to the device open so it
 * can open the appropriate tape segment.
 */
int
devopen(f, fname, file)
	struct open_file *f;
	const char *fname;		/* normally "1" */
	char **file;
{
	struct devsw *dp;
	int error;

	*file = (char*)fname;
	dp = &devsw[0];
	f->f_dev = dp;

	/* The following will call tape_open() */
	return (dp->dv_open(f, fname));
}

int
tape_open(f, fname)
	struct open_file *f;
	char *fname;		/* partition number, i.e. "1" */
{
	struct bootparam *bp;
	struct saioreq *si;
	int	error, part;

#ifdef DEBUG
	printf("tape_open: part=%s\n", fname);
#endif

	/*
	 * Set the tape segment number to the one indicated
	 * by the single digit fname passed in above.
	 */
	if ((fname[0] < '0') && (fname[0] > '9')) {
		return ENOENT;
	}
	part = fname[0] - '0';

	/*
	 * Setup our part of the saioreq.
	 * (determines what gets opened)
	 */
	si = &tape_ioreq;
	bzero((caddr_t)si, sizeof(*si));

	bp = *romp->bootParam;
	si->si_boottab = bp->bootDevice;
	si->si_ctlr = bp->ctlrNum;
	si->si_unit = bp->unitNum;
	si->si_boff = part; 	/* default = bp->partNum + 1; */

	error = prom_iopen(si);

#ifdef DEBUG
	printf("tape_open: prom_iopen returned 0x%x\n", error);
#endif

	if (!error)
		f->f_devdata = si;

	return (error);
}

int
tape_close(f)
	struct open_file *f;
{
	struct saioreq *si;

#ifdef DEBUG
	printf("tape_close: calling prom_iclose\n");
#endif

	si = f->f_devdata;
	prom_iclose(si);
	f->f_devdata = NULL;
	return 0;
}

int
tape_strategy(devdata, flag, dblk, size, buf, rsize)
	void	*devdata;
	int	flag;
	daddr_t	dblk;
	u_int	size;
	char	*buf;
	u_int	*rsize;
{
	struct saioreq *si;
	struct boottab *ops;
	char *dmabuf;
	int	si_flag, xcnt;

	si = devdata;
	ops = si->si_boottab;

#ifdef DEBUG
	if (debug > 1)
		printf("tape_strategy: size=%d dblk=%d\n", size, dblk);
#endif

	dmabuf = dvma_mapin(buf, size);
	
	si->si_bn = dblk;
	si->si_ma = dmabuf;
	si->si_cc =	size;

	si_flag = (flag == F_READ) ? SAIO_F_READ : SAIO_F_WRITE;
	xcnt = (*ops->b_strategy)(si, si_flag);
	dvma_mapout(dmabuf, size);

#ifdef DEBUG
	if (debug > 1)
		printf("tape_strategy: xcnt = %x\n", xcnt);
#endif

	/* At end of tape, xcnt == 0 (not an error) */
	if (xcnt < 0)
		return (EIO);

	*rsize = xcnt;
	return (0);
}

int
tape_ioctl()
{
	return EIO;
}

