/*	$OpenBSD: dk.c,v 1.4 1998/10/30 19:42:17 mickey Exp $	*/

/*
 * Copyright 1996 1995 by Open Software Foundation, Inc.   
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */

#include "libsa.h"

#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/reboot.h>
#include <machine/pdc.h>
#include <machine/iomod.h>

#include "dev_hppa.h"

iodcio_t dkiodc;	/* boot IODC entry point */

const char *
dk_disklabel(dp, label)
	struct hppa_dev *dp;
	struct disklabel *label;
{
	char buf[IONBPG];
	size_t ret;

	if (iodcstrategy(dp, F_READ, LABELSECTOR, IONBPG, buf, &ret))
		if (ret != DEV_BSIZE)
			return "cannot read LIF header";

	return (getdisklabel(buf, label));
}

int
#ifdef __STDC__
dkopen(struct open_file *f, ...)
#else
dkopen(f, va_alist)
	struct open_file *f;
#endif
{
	register struct disklabel *lp;
	register struct hppa_dev *dp;
	register struct pz_device *pzd;
	register const char *st;

#ifdef	DEBUG
	if (debug)
		printf("dkopen(%p)\n", f);
#endif

	if (!(pzd = pdc_findev(-1, PCL_RANDOM)))
		return ENXIO;

#ifdef	DEBUG
	if (debug)
		printf("alloc\n");
#endif
	if (!(dp = alloc(sizeof *dp))) {
#ifdef DEBUG
		printf ("dkopen: no mem\n");
#endif
		return ENODEV;
	}

	bzero(dp, sizeof *dp);

	dp->bootdev = bootdev;
	dp->pz_dev = pzd;
	lp = dp->label;
	st = NULL;
#if 0	
#ifdef DEBUG
	if (debug)
		printf ("disklabel\n");
#endif
	if ((st = dk_disklabel(dp, lp)) != NULL) {
#ifdef DEBUG
		if (debug)
			printf ("dkopen: %s\n", st);
#endif
		return ERDLAB;
	} else {
		register u_int i;

		i = B_PARTITION(dp->bootdev);
		if (i >= lp->d_npartitions || !lp->d_partitions[i].p_size) {
			return (EPART);
		}
	}
#endif
#ifdef DEBUG
	if (debug)
		printf ("dkopen() ret\n");
#endif
	f->f_devdata = dp;
	return (0);
}

int
dkclose(f)
	struct open_file *f;
{
	free (f->f_devdata, sizeof(struct hppa_dev));
	f->f_devdata = NULL;
	return 0;
}
