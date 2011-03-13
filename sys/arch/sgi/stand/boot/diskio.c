/*	$OpenBSD: diskio.c,v 1.6 2011/03/13 00:13:53 deraadt Exp $ */

/*
 * Copyright (c) 2000 Opsycon AB  (www.opsycon.se)
 * Copyright (c) 2000 Rtmx, Inc   (www.rtmx.com)
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
 *	This product includes software developed for Rtmx, Inc by
 *	Opsycon Open System Consulting AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stand.h>
#include <sys/param.h>
#include <sys/disklabel.h>
#include <mips64/arcbios.h>

struct	dio_softc {
	int	sc_fd;			/* PROM file ID */
	int	sc_part;		/* Disk partition number. */
	struct	disklabel sc_label;	/* Disk label for this disk. */
};

int
diostrategy(void *devdata, int rw, daddr32_t bn, u_int reqcnt, void *addr,
    size_t *cnt)
{
	struct dio_softc *sc = (struct dio_softc *)devdata;
	struct partition *pp = &sc->sc_label.d_partitions[sc->sc_part];
	arc_quad_t offset;
	long result;

	offset.hi = 0;
	offset.lo = (pp->p_offset + bn) * DEV_BSIZE;

	if ((Bios_Seek(sc->sc_fd, &offset, 0) < 0) ||
	    (Bios_Read(sc->sc_fd, addr, reqcnt, &result) < 0))
		return (EIO);

	*cnt = result;
	return (0);
}

int
dioopen(struct open_file *f, ...)
{
	char *ctlr;
	int partition;

	struct dio_softc *sc;
	struct disklabel *lp;
	long fd;
	daddr32_t labelsector;
	va_list ap;

	va_start(ap, f);
	ctlr = va_arg(ap, char *);
	partition = va_arg(ap, int);
	va_end(ap);

	if (partition >= MAXPARTITIONS)
		return (ENXIO);

	if (Bios_Open(ctlr, 0, &fd) < 0)
		return (ENXIO);

	sc = alloc(sizeof(struct dio_softc));
	bzero(sc, sizeof(struct dio_softc));
	f->f_devdata = (void *)sc;

	sc->sc_fd = fd;
	sc->sc_part = partition;

	lp = &sc->sc_label;
	lp->d_secsize = DEV_BSIZE;
	lp->d_secpercyl = 1;
	lp->d_npartitions = MAXPARTITIONS;
	lp->d_partitions[partition].p_offset = 0;
	lp->d_partitions[0].p_size = 0x7fff0000;

	labelsector = LABELSECTOR;

#if 0
	/* Try to read disk label and partition table information. */
	i = diostrategy(sc, F_READ, (daddr32_t)labelsector, DEV_BSIZE, buf, &cnt);

	if (i == 0 && cnt == DEV_BSIZE)
		msg = getdisklabel(buf, lp);
	else
		msg = "rd err";

	if (msg) {
		printf("%s: %s\n", ctlr, msg);
		return (ENXIO);
	}
#endif

	return (0);
}

int
dioclose(struct open_file *f)
{
	Bios_Close(((struct dio_softc *)f->f_devdata)->sc_fd);
	free(f->f_devdata, sizeof(struct dio_softc));
	f->f_devdata = NULL;
	return (0);
}
