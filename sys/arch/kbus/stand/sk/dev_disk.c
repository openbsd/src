/*	$OpenBSD: dev_disk.c,v 1.2 2000/03/03 00:54:51 todd Exp $ */

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

int
disk_open(f, devname)
	struct open_file *f;
	char *devname;		/* Device part of file name (or NULL). */
{
	struct saioreq *sip;
	int	error;

#ifdef DEBUG_PROM
	printf("disk_open: %s\n", devname);
#endif

	if ((error = prom_iopen(&sip)) != 0)
		return (error);

	f->f_devdata = sip;
	return 0;
}

int
disk_close(f)
	struct open_file *f;
{
	struct saioreq *sip;

	sip = f->f_devdata;
	prom_iclose(sip);
	f->f_devdata = NULL;
	return 0;
}

int
disk_strategy(devdata, flag, dblk, size, buf, rsize)
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

#ifdef DEBUG_PROM
	printf("disk_strategy: size=%d dblk=%d\n", size, dblk);
#else
	twiddle();
#endif

	dmabuf = dvma_mapin(buf, size);
	
	si->si_bn = dblk;
	si->si_ma = dmabuf;
	si->si_cc =	size;

	si_flag = (flag == F_READ) ? SAIO_F_READ : SAIO_F_WRITE;
	xcnt = (*ops->b_strategy)(si, si_flag);
	dvma_mapout(dmabuf, size);

#ifdef DEBUG_PROM
	printf("disk_strategy: xcnt = %x\n", xcnt);
#endif

	if (xcnt <= 0)
		return (EIO);

	*rsize = xcnt;
	return (0);
}

int
disk_ioctl()
{
	return EIO;
}

