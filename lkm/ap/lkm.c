/*	$OpenBSD: lkm.c,v 1.1 1996/03/05 11:25:34 mickey Exp $	*/
/*
 * Copyright (c) 1994 The XFree86 Project Inc.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/exec.h>
#include <sys/lkm.h>
#include <errno.h>
#include "version.h"

extern int apopen(dev_t dev, int oflags, int devtype, struct proc *p);
extern int apclose(dev_t dev, int fflags, int devtype, struct proc *p);
extern int apmmap(dev_t dev, int offset, int length);

static struct cdevsw newdev = {
    apopen, apclose, 
    (dev_type_read((*))) enodev, (dev_type_write((*))) enodev,
    (dev_type_ioctl((*))) enodev, 
    (dev_type_stop((*))) enodev,
    0, seltrue, (dev_type_mmap((*))) apmmap, 0};

MOD_DEV("ap", LM_DT_CHAR, -1, &newdev)

static int 
ap_load(struct lkm_table *lkmtp, int cmd)
{
    if (cmd == LKM_E_LOAD) {
	printf("\n Aperture driver for XFree86 version %s.%s\n",
	       ap_major_version, ap_minor_version);
    }
    return(0);
}

int
ap(struct lkm_table *lkmtp, int cmd, int ver)
{
    DISPATCH(lkmtp, cmd, ver, ap_load, lkm_nofunc, lkm_nofunc)
}
    
