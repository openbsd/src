/*	$NetBSD: rz.c,v 1.6 1995/06/28 10:22:35 jonathan Exp $	*/

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

#include <stdarg.h>

#include <stand.h>
#include <sys/param.h>
#include <sys/disklabel.h>

struct	rz_softc {
	int	sc_fd;			/* PROM file id */
	int	sc_ctlr;		/* controller number */
	int	sc_unit;		/* disk unit number */
	int	sc_part;		/* disk partition number */
	struct	disklabel sc_label;	/* disk label for this disk */
};

int
rzstrategy(devdata, rw, bn, reqcnt, addr, cnt)
	void *devdata;
	int rw;
	daddr_t bn;
	u_int reqcnt;
	char *addr;
	u_int *cnt;	/* out: number of bytes transfered */
{
	register struct rz_softc *sc = (struct rz_softc *)devdata;
	register int part = sc->sc_part;
	register struct partition *pp = &sc->sc_label.d_partitions[part];
	register int s;
	long offset;

	offset = bn * DEV_BSIZE;

#ifdef DEBUG
/*XXX*/printf("rz:%x %d\n", offset, reqcnt);
#endif

	/*
	 * Partial-block transfers not handled.
	 */
	if (reqcnt & (DEV_BSIZE - 1)) {
		*cnt = 0;
		return (EINVAL);
	}

	offset += pp->p_offset * DEV_BSIZE;

	if (Bios_Seek(sc->sc_fd, &offset, 0) != 0)
		return (EIO);
	s = Bios_Read(sc->sc_fd, addr, reqcnt, &reqcnt);
	if (s != 0)
		return (EIO);

	*cnt = reqcnt;
	return (0);
}

int
rzopen(struct open_file *f, ...)
{
	int ctlr, unit, part;

	struct rz_softc *sc;
	struct disklabel *lp;
	int i, fd;
	char *msg;
	char buf[DEV_BSIZE];
	int cnt;
	static char device[] = "scsi()disk(0)rdisk()";
	va_list ap;

	va_start(ap, f);

	ctlr = va_arg(ap, int);
	unit = va_arg(ap, int);
	part = va_arg(ap, int);
	if (unit >= 8 || part >= 8)
		return (ENXIO);
#if 0
	device[5] = '0' + unit;
#endif
	/* NOTE: only support reads for now */

	i = Bios_Open(device, 0, &fd);
	if (i != 0) {
		printf("boot init failed error code %d\n", i);
		return (ENXIO);
	}

	sc = alloc(sizeof(struct rz_softc));
	bzero(sc, sizeof(struct rz_softc));
	f->f_devdata = (void *)sc;

	sc->sc_fd = fd;
	sc->sc_ctlr = ctlr;
	sc->sc_unit = unit;
	sc->sc_part = part;

	/* try to read disk label and partition table information */
	lp = &sc->sc_label;
	lp->d_secsize = DEV_BSIZE;
	lp->d_secpercyl = 1;
	lp->d_npartitions = MAXPARTITIONS;
	lp->d_partitions[part].p_offset = 0;
	lp->d_partitions[part].p_size = 0x7fffffff;
	i = rzstrategy(sc, F_READ, (daddr_t)LABELSECTOR, DEV_BSIZE, buf, &cnt);
	if (i || cnt != DEV_BSIZE) {
		printf("rz%d: error reading disk label\n", unit);
		goto bad;
	} else {
		msg = getdisklabel(buf, lp);
		if (msg) {
			printf("rz%d: %s\n", unit, msg);
			goto bad;
		}
	}

	if (part >= lp->d_npartitions || lp->d_partitions[part].p_size == 0) {
	bad:
#ifndef SMALL
		free(sc, sizeof(struct rz_softc));
#endif
		return (ENXIO);
	}
	return (0);
}

#ifndef SMALL
rzclose(f)
	struct open_file *f;
{
	free(f->f_devdata, sizeof(struct rz_softc));
	f->f_devdata = (void *)0;
	return (0);
}
#endif
