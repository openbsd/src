/*	$OpenBSD: dev_tape.c,v 1.4 2011/03/13 00:13:53 deraadt Exp $	*/
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
 */

#include <sys/types.h>
#include <machine/prom.h>

#include "stand.h"
#include "libsa.h"


extern int debug;

struct mvmeprom_dskio tape_ioreq;

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

	*file = (char *)fname;
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
	int	error, part;
	struct mvmeprom_dskio *ti;

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
	ti = &tape_ioreq;
	bzero((caddr_t)ti, sizeof(*ti));

	ti->ctrl_lun = bugargs.ctrl_lun;
	ti->dev_lun = bugargs.dev_lun;
	ti->status = 0;
	ti->pbuffer = NULL;
	ti->blk_num = part;
	ti->blk_cnt = 0;
	ti->flag = 0;
	ti->addr_mod = 0;

	f->f_devdata = ti;

	return (error);
}

int
tape_close(f)
	struct open_file *f;
{
	struct mvmeprom_dskio *ti;


	ti = f->f_devdata;
	f->f_devdata = NULL;
	return 0;
}

#define MVMEPROM_SCALE (512/MVMEPROM_BLOCK_SIZE)

int
tape_strategy(devdata, flag, dblk, size, buf, rsize)
	void	*devdata;
	int	flag;
	daddr32_t	dblk;
	u_int	size;
	char	*buf;
	u_int	*rsize;
{
	struct mvmeprom_dskio *ti;
	int ret;

	ti = devdata;

	if (flag != F_READ)
		return(EROFS);

	ti->status = 0;
	ti->pbuffer = buf;
	/* don't change block #, set in open */
	ti->blk_cnt = size / (512 / MVMEPROM_SCALE);

	ret = mvmeprom_diskrd(ti);

	if (ret != 0)
		return (EIO);

	*rsize = (ti->blk_cnt / MVMEPROM_SCALE) * 512;
	ti->flag |= IGNORE_FILENUM; /* ignore next time */

	return (0);
}

int
tape_ioctl()
{
	return EIO;
}

