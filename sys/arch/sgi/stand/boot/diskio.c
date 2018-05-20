/*	$OpenBSD: diskio.c,v 1.12 2018/03/02 15:36:39 visa Exp $ */

/*
 * Copyright (c) 2016 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
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

#include <sys/param.h>
#include <lib/libkern/libkern.h>
#include <stand.h>

#include <sys/disklabel.h>
#include <mips64/arcbios.h>

char	*strstr(char *, const char *);  /* strstr.c */

struct	dio_softc {
	int	sc_fd;			/* PROM file ID */
	int	sc_part;		/* Disk partition number. */
	struct	disklabel sc_label;	/* Disk label for this disk. */
};

int
diostrategy(void *devdata, int rw, daddr32_t bn, size_t reqcnt, void *addr,
    size_t *cnt)
{
	struct dio_softc *sc = (struct dio_softc *)devdata;
	struct partition *pp = &sc->sc_label.d_partitions[sc->sc_part];
	uint64_t blkoffset;
	arc_quad_t offset;
	long result;

	blkoffset =
	    (DL_SECTOBLK(&sc->sc_label, DL_GETPOFFSET(pp)) + bn) * DEV_BSIZE;
	offset.hi = blkoffset >> 32;
	offset.lo = blkoffset;

	if (Bios_Seek(sc->sc_fd, &offset, 0) < 0 ||
	    Bios_Read(sc->sc_fd, addr, reqcnt, &result) < 0)
		return EIO;

	*cnt = result;
	return 0;
}

int
dioopen(struct open_file *f, ...)
{
	char *ctlr;
	int partition;
	struct dio_softc *sc;
	struct disklabel *lp;
	struct sgilabel *sl;
	long fd;
	/* XXX getdisklabel() assumes DEV_BSIZE bytes available */
	char buf[DEV_BSIZE + LABELOFFSET];
	arc_quad_t offset;
	daddr_t native_offset;
	long result;
	va_list ap;
	char rawctlr[1 + MAXPATHLEN];
	char *partptr;

	va_start(ap, f);
	ctlr = va_arg(ap, char *);
	partition = va_arg(ap, int);
	va_end(ap);

	if (partition >= MAXPARTITIONS)
		return ENXIO;

	/*
	 * If booting from disk, `ctlr` is something like
	 *   whatever()partition(0)
	 * or
	 *   dksc(whatever,0)
	 * where 0 is the volume header #0 partition, which is the
	 * OpenBSD area, where the OpenBSD disklabel can be found.
	 *
	 * However, the OpenBSD `a' partition, where the kernel is to be
	 * found, may not start at the same offset.
	 *
	 * In order to be able to correctly load any file from the OpenBSD
	 * partitions, we need to access the volume header partition table
	 * and the OpenBSD label.
	 *
	 * Therefore, make sure we replace `partition(*)' with `partition(10)'
	 * before reaching ARCBios, in order to access the raw disk.
	 *
	 * We could use partition #8 and use the value of SystemPartition in
	 * the environment to avoid doing this, but this would prevent us
	 * from being able to boot from a different disk than the one
	 * pointed to by SystemPartition.
	 */
	
	strlcpy(rawctlr, ctlr, sizeof rawctlr);
	partptr = strstr(rawctlr, "partition(");
	if (partptr != NULL) {
		strlcpy(partptr, "partition(10)",
		    sizeof rawctlr - (partptr - rawctlr));
	} else {
		if ((partptr = strstr(rawctlr, "dksc(")) != NULL) {
			partptr = strstr(partptr, ",0)");
			if (partptr != NULL && partptr[3] == '\0')
				strlcpy(partptr, ",10)",
				    sizeof rawctlr - (partptr - rawctlr));
		}
	}

	sl = NULL;	/* no volume header found yet */
	if (partptr != NULL) {
		if (Bios_Open(rawctlr, 0, &fd) < 0)
			return ENXIO;

		/*
		 * Read the volume header.
		 */
		offset.hi = offset.lo = 0;
		if (Bios_Seek(fd, &offset, 0) < 0 ||
		    Bios_Read(fd, buf, DEV_BSIZE, &result) < 0 ||
		    result != DEV_BSIZE)
			return EIO;

		sl = (struct sgilabel *)buf;
		if (sl->magic != SGILABEL_MAGIC) {
#ifdef DEBUG
			printf("Invalid volume header magic %x\n", sl->magic);
#endif
			Bios_Close(fd);
			sl = NULL;
		}
	}

	if (sl == NULL) {
		if (Bios_Open(ctlr, 0, &fd) < 0)
			return ENXIO;
	}

	sc = alloc(sizeof(struct dio_softc));
	bzero(sc, sizeof(struct dio_softc));
	f->f_devdata = (void *)sc;
	lp = &sc->sc_label;

	sc->sc_fd = fd;
	sc->sc_part = partition;

	if (sl != NULL) {
		native_offset = sl->partitions[0].first;
	} else {
		/*
		 * We could not read the volume header, or there isn't any.
		 * Stick to the device we were given, and assume the
		 * OpenBSD disklabel can be found at the beginning.
		 */
		native_offset = 0;
	}

	/*
	 * Read the native OpenBSD label.
	 */
#ifdef DEBUG
	printf("OpenBSD label @%lld\n", native_offset + LABELSECTOR);
#endif
	offset.hi = ((native_offset + LABELSECTOR) * DEV_BSIZE) >> 32;
	offset.lo = (native_offset + LABELSECTOR) * DEV_BSIZE;

	if (Bios_Seek(fd, &offset, 0) < 0 ||
	    Bios_Read(fd, buf, DEV_BSIZE, &result) < 0 ||
	    result != DEV_BSIZE)
		return EIO;
	
	if (getdisklabel(buf + LABELOFFSET, lp) == NULL) {
#ifdef DEBUG
		printf("Found native disklabel, "
		    "partition %c starts at %lld\n",
		    'a' + partition,
		    DL_GETPOFFSET(&lp->d_partitions[partition]));
#endif
	} else {
		/*
		 * Assume the OpenBSD partition spans the whole device.
		 */
		lp->d_secsize = DEV_BSIZE;
		lp->d_secpercyl = 1;
		lp->d_npartitions = MAXPARTITIONS;
		DL_SETPOFFSET(&lp->d_partitions[partition], native_offset);
		DL_SETPSIZE(&lp->d_partitions[partition], -1ULL);
#ifdef DEBUG
		printf("No native disklabel found, "
		    "assuming partition %c starts at %lld\n",
		    'a' + partition, native_offset);
#endif
	}

	return 0;
}

int
dioclose(struct open_file *f)
{
	Bios_Close(((struct dio_softc *)f->f_devdata)->sc_fd);
	free(f->f_devdata, sizeof(struct dio_softc));
	f->f_devdata = NULL;
	return (0);
}
