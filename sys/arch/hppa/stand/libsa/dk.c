/*	$OpenBSD: dk.c,v 1.3 1998/09/29 07:20:45 mickey Exp $	*/

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
	struct disklabel *lp;
	struct hppa_dev *dp;
	struct pz_device *pzd;
	const char *st;
	int i;

#ifdef	DEBUG
	printf("dkopen(%p)\n", f);
#endif

	if (!(pzd = pdc_findev(-1, PCL_RANDOM)))
		return ENXIO;
#ifdef PDCDEBUG
	else if (debug)
		PZDEV_PRINT(pzd);
#endif

	if (f->f_devdata == 0) 
		f->f_devdata = alloc(sizeof *dp);
	dp = f->f_devdata;

	bzero(dp, sizeof *dp);

	dp->bootdev = bootdev;
	dp->pz_dev = pzd;
	lp = dp->label;
	
	if ((st = dk_disklabel(dp, lp)) != NULL) {
#ifdef DEBUG
		if (debug)
			printf ("%s\n", st);
#endif
		return ERDLAB;
	}

	i = B_PARTITION(dp->bootdev);
	if ((unsigned int)i >= lp->d_npartitions ||
	    lp->d_partitions[i].p_size == 0) {
		return (EPART);
	}

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
