/*	$NetBSD: disk.c,v 1.3 1995/11/23 02:39:40 cgd Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory and Ralph Campbell.
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
 *	@(#)rz.c	8.1 (Berkeley) 6/10/93
 */

#include <lib/libsa/stand.h>

#include <sys/param.h>
#include <sys/disklabel.h>

#include <machine/prom.h>

#include "disk.h"

struct	disk_softc {
	int	sc_fd;			/* PROM channel number */
	int	sc_ctlr;		/* controller number */
	int	sc_unit;		/* disk unit number */
	int	sc_part;		/* disk partition number */
	struct	disklabel sc_label;	/* disk label for this disk */
};

int
diskstrategy(devdata, rw, bn, reqcnt, addrvoid, cnt)
	void *devdata;
	int rw;
	daddr_t bn;
	size_t reqcnt;
	void *addrvoid;
	size_t *cnt;	/* out: number of bytes transfered */
{
	char *addr = addrvoid;
	struct disk_softc *sc;
	struct partition *pp;
	prom_return_t ret;
	int s;

	if ((reqcnt & 0xffffff) != reqcnt ||
	    reqcnt == 0)
		asm("call_pal 0");
	    
	twiddle();

	/* Partial-block transfers not handled. */
	if (reqcnt & (DEV_BSIZE - 1)) {
		*cnt = 0;
		return (EINVAL);
	}

	sc = (struct disk_softc *)devdata;
	pp = &sc->sc_label.d_partitions[sc->sc_part];

	ret.bits = prom_read(sc->sc_fd, reqcnt, addr, bn + pp->p_offset);
	if (ret.u.status)
		return (EIO);
	*cnt = ret.u.retval;
	return (0);
}

int
diskopen(f, ctlr, unit, part)
	struct open_file *f;
	int ctlr, unit, part;
{
	struct disklabel *lp;
	prom_return_t ret;
	size_t cnt;
	int devlen, i;
	char *msg, buf[DEV_BSIZE], devname[32];
	static struct disk_softc *sc;

if (sc != NULL) {
	f->f_devdata = (void *)sc;
	return 0;
}

	if (unit >= 8 || part >= 8)
		return (ENXIO);
	/* 
	 * XXX
	 * We don't know what device names look like yet,
	 * so we can't change them.
	 */
	ret.bits = prom_getenv(PROM_E_BOOTED_DEV, devname, sizeof(devname));
	devlen = ret.u.retval;

	ret.bits = prom_open(devname, devlen);
	if (ret.u.status == 2)
		return (ENXIO);
	if (ret.u.status == 3)
		return (EIO);

	sc = alloc(sizeof(struct disk_softc));
	bzero(sc, sizeof(struct disk_softc));
	f->f_devdata = (void *)sc;

	sc->sc_fd = ret.u.retval;
	sc->sc_ctlr = ctlr;
	sc->sc_unit = unit;
	sc->sc_part = part;

	/* Try to read disk label and partition table information. */
	lp = &sc->sc_label;
	lp->d_secsize = DEV_BSIZE;
	lp->d_secpercyl = 1;
	lp->d_npartitions = MAXPARTITIONS;
	lp->d_partitions[part].p_offset = 0;
	lp->d_partitions[part].p_size = 0x7fffffff;
	i = diskstrategy(sc, F_READ,
	    (daddr_t)LABELSECTOR, DEV_BSIZE, buf, &cnt);
	if (i || cnt != DEV_BSIZE) {
		printf("disk%d: error reading disk label\n", unit);
		goto bad;
	} else {
		msg = getdisklabel(buf, lp);
		if (msg) {
			printf("disk%d: %s\n", unit, msg);
			goto bad;
		}
	}

	if (part >= lp->d_npartitions || lp->d_partitions[part].p_size == 0) {
bad:		free(sc, sizeof(struct disk_softc));
		return (ENXIO);
	}
	return (0);
}

diskclose(f)
	struct open_file *f;
{
	struct disk_softc *sc;

	sc = f->f_devdata;
	(void)prom_close(sc->sc_fd);

	free(sc, sizeof(struct disk_softc));
	f->f_devdata = NULL;
	return (0);
}
