/*	$OpenBSD: dk.c,v 1.2 1998/07/08 21:34:34 mickey Exp $	*/

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
#include <machine/iodc.h>
#include <machine/iomod.h>

#include "dev_hppa.h"

iodcio_t btiodc;	/* boot IODC entry point */

char btbuf[BTIOSIZ] __attribute ((aligned (MINIOSIZ)));
int HP800;	

void
btinit()
{
	int err;
	static int firstime = 1;

	btiodc = (iodcio_t)(PAGE0->mem_free + IODC_MAXSIZE);

	if (firstime) {
		/*
		 * If we _rtt(), we will call btinit() again.
		 * We only want to do ctdev initialization once.
		 */
		bcopy((char *)&PAGE0->mem_boot, (char *)&ctdev,
		      sizeof(struct pz_device));
		firstime = 0;
	}

	/*
	 * Initialize "HP800" to boolean value (T=HP800 F=HP700).
	 */
	if (!HP800) {
		struct pdc_model model;
		err = (*pdc)(PDC_MODEL, PDC_MODEL_INFO, &model, 0,0,0,0,0);
		if (err < 0) {
			HP800 = 1;	/* default: HP800 */
			printf("Proc model info ret'd %d (assuming %s)\n",
			       err, HP800? "HP800": "HP700");
		}
		HP800 = (((model.hvers >> 4) & 0xfff) < 0x200);
	}
}

int
dkreset(slot, unit)
	int slot, unit;
{
	struct device_path bootdp;
	int err, srchtype;

	/*
	 * Save a copy of the previous boot device path.
	 */
	bcopy((char *)&PAGE0->mem_boot.pz_dp, (char *)&bootdp,
	      sizeof(struct device_path));

	/*
	 * Read the boot device initialization code into memory.
	 */
	err = (*pdc)(PDC_IODC, PDC_IODC_READ, pdcbuf, BT_HPA, IODC_INIT,
	             btiodc, IODC_MAXSIZE);
	if (err < 0) {
		printf("Boot module ENTRY_INIT Read ret'd %d\n", err);
		goto bad;
	}

	/*
	 * Plod over boot devices looking for one with the same unit
	 * number as that which is in `unit'.
	 */
	srchtype = IODC_INIT_FIRST;
	while (1) {
		err = (*btiodc)(BT_HPA,srchtype,BT_SPA,BT_LAYER,pdcbuf,0,0,0,0);
		if (err < 0) {
			if (err == -9) {
				BT_IODC = 0;
				return(EUNIT);
			}
			printf("Boot module ENTRY_INIT Search ret'd %d\n", err);
			goto bad;
		}

		srchtype = IODC_INIT_NEXT;	/* for next time... */

		if (pdcbuf[1] != PCL_RANDOM)	/* only want disks */
			continue;

		if (HP800) {
			if (slot != ANYSLOT && slot != BT_LAYER[0])
				continue;

			if (BT_LAYER[1] == unit) {
				BT_CLASS = pdcbuf[1];
				break;
			}
		} else {
			if (slot != NOSLOT)
				continue;

			if (BT_LAYER[0] == unit) {
				BT_CLASS = pdcbuf[1];
				break;
			}
		}
	}

	/*
	 * If this is not the "currently initialized" boot device,
	 * initialize the new boot device we just found.
	 *
	 * N.B. We do not need/want to initialize the entire module
	 * (e.g. CIO, SCSI), and doing so may blow away our console.
	 * if the user specified a boot module other than the
	 * console module, we initialize both the module and device.
	 */
	if (bcmp((char *)&PAGE0->mem_boot.pz_dp, (char *)&bootdp,
	         sizeof(struct device_path)) != 0) {
		err = (*btiodc)(BT_HPA,(!HP800||BT_HPA==CN_HPA||BT_HPA==KY_HPA)?
		                IODC_INIT_DEV: IODC_INIT_ALL,
		                BT_SPA, BT_LAYER, pdcbuf, 0,0,0,0);
		if (err < 0) {
			printf("Boot module/device IODC Init ret'd %d\n", err);
			goto bad;
		}
	}

	err = (*pdc)(PDC_IODC, PDC_IODC_READ, pdcbuf, BT_HPA, IODC_IO,
	             btiodc, IODC_MAXSIZE);
	if (err < 0) {
		printf("Boot device ENTRY_IO Read ret'd %d\n", err);
		goto bad;
	}

	BT_IODC = btiodc;
	return (0);
bad:
	BT_IODC = 0;
	return(-1);
}

const char *
dk_disklabel(dp, label)
	struct hppa_dev *dp;
	struct disklabel *label;
{
	char buf[DEV_BSIZE];
	size_t ret;

	if (dkstrategy(dp, F_READ, LABELOFFSET, DEV_BSIZE, buf, &ret) ||
	    ret != DEV_BSIZE)
		return "cannot read disklbael";

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
	const char *st;
	int i;

	if (f->f_devdata == 0) 
		f->f_devdata = alloc(sizeof *dp);
	dp = f->f_devdata;

	bzero(dp, sizeof *dp);

	{
		int adapt, ctlr, unit, part, type;
		va_list ap;

#ifdef __STDC__
		va_start(ap, f);
#else
		va_start(ap);
#endif
		adapt = va_arg(ap, int);
		ctlr = va_arg(ap, int);
		unit = va_arg(ap, int);
		part = va_arg(ap, int);
		type = va_arg(ap, int);
		va_end(ap);

		dp->bootdev = MAKEBOOTDEV(type, adapt, ctlr, unit, part);
	}
	lp = &dp->label;
	
	if ((st = dk_disklabel(dp, lp)) != NULL) {
		printf ("%s\n", st);
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
dkstrategy(devdata, rw, blk, size, buf, rsize)
	void *devdata;
	int rw;
	daddr_t blk;
	size_t size;
	void *buf;
	size_t *rsize;
{
	int ret;

	ret = iodc_rw(buf, blk, size, rw, &PAGE0->mem_boot);
	if (ret < 0) {
		printf("dk: iodc ret'd %d\n", ret);
		return (-1);
	}
	
	*rsize = ret;
	return (ret);
}

int
dkclose(f)
	struct open_file *f;
{
	return 0;
}
