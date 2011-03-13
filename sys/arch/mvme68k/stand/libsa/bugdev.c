/*	$OpenBSD: bugdev.c,v 1.7 2011/03/13 00:13:53 deraadt Exp $ */

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

#include <sys/param.h>
#include <sys/disklabel.h>
#include <machine/prom.h>

#include "stand.h"
#include "libsa.h"

void cputobsdlabel(struct disklabel *lp, struct mvmedisklabel *clp);

int errno;

struct bugsc_softc {
	int	fd;	/* Prom file descriptor */
	int	poff;	/* Partition offset */
	int	psize;	/* Partition size */
	short	ctrl;
	short	dev;
} bugsc_softc[1];

int
devopen(struct open_file *f, const char *fname, char **file)
{
	register struct bugsc_softc *pp = &bugsc_softc[0];
	int	error, i, dn = 0, pn = 0;
	char	*dev, *cp;
	static	char iobuf[MAXBSIZE];
	struct disklabel sdlabel;

	dev = bugargs.arg_start;

	/*
	 * Extract partition # from boot device string.
	 */
	for (cp = dev; *cp; cp++) /* void */;
	while (*cp != '/' && cp > dev) {
		if (*cp == ':')
			pn = *(cp+1) - 'a';
		--cp;
	}

	pp->fd = bugscopen(f);

	if (pp->fd < 0) {
		printf("Can't open device `%s'\n", dev);
		return (ENXIO);
	}
	error = bugscstrategy(pp, F_READ, LABELSECTOR, DEV_BSIZE, iobuf, &i);
	if (error)
		return (error);
	if (i != DEV_BSIZE)
		return (EINVAL);

	cputobsdlabel(&sdlabel, (struct mvmedisklabel *)iobuf);
	pp->poff = sdlabel.d_partitions[pn].p_offset;
	pp->psize = sdlabel.d_partitions[pn].p_size;

	f->f_dev = devsw;
	f->f_devdata = (void *)pp;
	*file = (char *)fname;
	return (0);
}

/* silly block scale factor */
#define BUG_BLOCK_SIZE 256
#define BUG_SCALE (512/BUG_BLOCK_SIZE)

int
bugscstrategy(void *devdata, int func, daddr32_t dblk, size_t size, void *buf,
    size_t *rsize)
{
	struct mvmeprom_dskio dio;
	register struct bugsc_softc *pp = (struct bugsc_softc *)devdata;
	daddr32_t	blk = dblk + pp->poff;

	twiddle();

	dio.ctrl_lun = pp->ctrl;
	dio.dev_lun = pp->dev;
	dio.status = 0;
	dio.pbuffer = buf;
	dio.blk_num = blk * BUG_SCALE;
	dio.blk_cnt = size / BUG_BLOCK_SIZE; /* assumed size in bytes */
	dio.flag = 0;
	dio.addr_mod = 0;
#ifdef DEBUG
	printf("bugscstrategy: size=%d blk=%d buf=%x\n", size, blk, buf);
	printf("ctrl %d dev %d\n", dio.ctrl_lun, dio.dev_lun);
#endif
	mvmeprom_diskrd(&dio);

	*rsize = dio.blk_cnt * BUG_BLOCK_SIZE;
#ifdef DEBUG
printf("rsize %d status %x\n", *rsize, dio.status);
#endif

	if (dio.status)
		return (EIO);
	return (0);
}

int
bugscopen(struct open_file *f)
{
#ifdef DEBUG
	printf("bugscopen:\n");
#endif

	f->f_devdata = (void *)bugsc_softc;
	bugsc_softc[0].ctrl = (short)bugargs.ctrl_lun;
	bugsc_softc[0].dev =  (short)bugargs.dev_lun;
#ifdef DEBUG
	printf("using mvmebug ctrl %d dev %d\n",
	    bugsc_softc[0].ctrl, bugsc_softc[0].dev);
#endif
	return (0);
}

int
bugscclose(struct open_file *f)
{
	return (EIO);
}

int
bugscioctl(struct open_file *f, u_long cmd, void *data)
{
	return (EIO);
}

void
cputobsdlabel(struct disklabel *lp, struct mvmedisklabel *clp)
{
	int i;

	lp->d_magic = clp->magic1;
	lp->d_type = clp->type;
	lp->d_subtype = clp->subtype;
	bcopy(clp->vid_vd, lp->d_typename, 16);
	bcopy(clp->packname, lp->d_packname, 16);
	lp->d_secsize = clp->cfg_psm;
	lp->d_nsectors = clp->cfg_spt;
	lp->d_ncylinders = clp->cfg_trk; /* trk is really num of cyl! */
	lp->d_ntracks = clp->cfg_hds;

	lp->d_secpercyl = clp->secpercyl;
	lp->d_secperunit = clp->secperunit;
	lp->d_secpercyl = clp->secpercyl;
	lp->d_secperunit = clp->secperunit;
	lp->d_acylinders = clp->acylinders;
	lp->d_flags = clp->flags;
	for (i = 0; i < NDDATA; i++)
		lp->d_drivedata[i] = clp->drivedata[i];
	for (i = 0; i < NSPARE; i++)
		lp->d_spare[i] = clp->spare[i];
	lp->d_magic2 = clp->magic2;
	lp->d_checksum = clp->checksum;
	lp->d_npartitions = clp->partitions;
	lp->d_bbsize = clp->bbsize;
	lp->d_sbsize = clp->sbsize;
	bcopy(clp->vid_4, &(lp->d_partitions[0]),sizeof (struct partition) * 4);
	bcopy(clp->cfg_4, &(lp->d_partitions[4]), sizeof (struct partition) *
	    ((MAXPARTITIONS < 16) ? (MAXPARTITIONS - 4) : 12));
}
