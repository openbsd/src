/*	$OpenBSD: conf.c,v 1.33 2007/05/27 01:50:36 todd Exp $	*/

/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *      @(#)conf.c	7.9 (Berkeley) 5/28/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include <machine/conf.h>

#include "pty.h"
#include "bpfilter.h"
#include "tun.h"
#include "vnd.h"
#include "ccd.h"
#include "rd.h"
#include "cd.h"
#include "ch.h"
#include "sd.h"
#include "ss.h"
#include "st.h"
#include "uk.h"

#ifdef XFS
#include <xfs/nxfs.h>
cdev_decl(xfs_dev);
#endif
#include "ksyms.h"

#include "sram.h"
#include "nvram.h"
#include "vmel.h"
#include "vmes.h"
#include "dart.h"
#include "cl.h"
#include "vx.h"

#ifdef notyet
#include "xd.h"
bdev_decl(xd);
cdev_decl(xd);
#endif /* notyet */

#ifdef notyet
#include "flash.h"
cdev_decl(flash);
#endif /* notyet */

/* open, close, write, ioctl */
#define	cdev_lp_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev }

/* open, close, ioctl, mmap, ioctl */
#define	cdev_mdev_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_poll((*))) enodev, \
	dev_init(c,n,mmap) }

#if notyet
#include "lp.h"
cdev_decl(lp);
#include "lptwo.h"
cdev_decl(lptwo);
#endif /* notyet */

#include "pf.h"

#include "systrace.h"

struct bdevsw	bdevsw[] =
{
	bdev_notdef(),			/* 0 */
	bdev_notdef(),			/* 1 */
	bdev_notdef(),			/* 2 */
	bdev_swap_init(1,sw),		/* 3: swap pseudo-device */
	bdev_disk_init(NSD,sd),		/* 4: SCSI disk */
	bdev_tape_init(NST,st),		/* 5: SCSI tape */
	bdev_disk_init(NCD,cd),		/* 6: SCSI CD-ROM */
	bdev_disk_init(NRD,rd),		/* 7: ramdisk */
	bdev_disk_init(NVND,vnd),	/* 8: vnode disk driver */
	bdev_disk_init(NCCD,ccd),	/* 9: concatenated disk driver */
#if notyet
	bdev_disk_init(NXD,xd),		/* 10: XD disk */
#endif /* notyet */
	bdev_notdef(),			/* 11 */
	bdev_notdef(),			/* 12 */
	bdev_lkm_dummy(),		/* 13 */
	bdev_lkm_dummy(),		/* 14 */
	bdev_lkm_dummy(),		/* 15 */
	bdev_lkm_dummy(),		/* 16 */
	bdev_lkm_dummy(),		/* 17 */
	bdev_lkm_dummy(),		/* 18 */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_ctty_init(1,ctty),		/* 1: controlling terminal */
	cdev_mm_init(1,mm),		/* 2: /dev/{null,mem,kmem,...} */
	cdev_swap_init(1,sw),		/* 3: /dev/drum (swap pseudo-device) */
	cdev_tty_init(NPTY,pts),	/* 4: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 5: pseudo-tty master */
	cdev_log_init(1,log),		/* 6: /dev/klog */
	cdev_mdev_init(NSRAM,sram),	/* 7: /dev/sramX */
	cdev_disk_init(NSD,sd),		/* 8: SCSI disk */
	cdev_disk_init(NCD,cd),		/* 9: SCSI CD-ROM */
	cdev_mdev_init(NNVRAM,nvram),	/* 10: /dev/nvramX */
#ifdef notyet
	cdev_mdev_init(NFLASH,flash),	/* 11: /dev/flashX */
#else
	cdev_notdef(),			/* 11: */
#endif /* notyet */
	cdev_tty_init(NDART,dart),	/* 12: MVME188 serial (tty[a-b]) */
	cdev_tty_init(NCL,cl),		/* 13: CL-CD1400 serial (tty0[0-3]) */
	cdev_notdef(),			/* 14 */
	cdev_tty_init(NVX,vx),		/* 15: MVME332XT serial/lpt ttyv[0-7][a-i] */
	cdev_notdef(),			/* 16 */
	cdev_disk_init(NCCD,ccd),	/* 17: concatenated disk */
	cdev_disk_init(NRD,rd),		/* 18: ramdisk disk */
	cdev_disk_init(NVND,vnd),	/* 19: vnode disk */
	cdev_tape_init(NST,st),		/* 20: SCSI tape */
	cdev_fd_init(1,filedesc),	/* 21: file descriptor pseudo-dev */
	cdev_bpftun_init(NBPFILTER,bpf),/* 22: berkeley packet filter */
	cdev_bpftun_init(NTUN,tun),	/* 23: network tunnel */
	cdev_lkm_init(NLKM,lkm),	/* 24: loadable module driver */
	cdev_notdef(),			/* 25 */
#ifdef notyet
	cdev_disk_init(NXD,xd),		/* 26: XD disk */
#else
	cdev_notdef(),			/* 26: XD disk */
#endif /* notyet */
	cdev_notdef(),			/* 27 */
#ifdef notyet
	cdev_lp_init(NLP,lp),		/* 28: lp */
	cdev_lp_init(NLPTWO,lptwo),	/* 29: lptwo */
#else
	cdev_notdef(),			/* 28: lp */
	cdev_notdef(),			/* 29: lptwo */
#endif /* notyet */
	cdev_notdef(),			/* 30 */
	cdev_mdev_init(NVMEL,vmel),	/* 31: /dev/vmelX */
	cdev_mdev_init(NVMES,vmes),	/* 32: /dev/vmesX */
	cdev_lkm_dummy(),		/* 33 */
	cdev_lkm_dummy(),		/* 34 */
	cdev_lkm_dummy(),		/* 35 */
	cdev_lkm_dummy(),		/* 36 */
	cdev_lkm_dummy(),		/* 37 */
	cdev_lkm_dummy(),		/* 38 */
	cdev_pf_init(NPF,pf),		/* 39: packet filter */
	cdev_random_init(1,random),	/* 40: random data source */
	cdev_uk_init(NUK,uk),		/* 41 */
	cdev_ss_init(NSS,ss),		/* 42 */
	cdev_ksyms_init(NKSYMS,ksyms),	/* 43: Kernel symbols device */
	cdev_ch_init(NCH,ch),		/* 44: SCSI autochanger */
	cdev_notdef(),			/* 45 */
	cdev_notdef(),			/* 46 */
	cdev_notdef(),			/* 47 */
	cdev_notdef(),			/* 48 */
	cdev_notdef(),			/* 49 */
	cdev_systrace_init(NSYSTRACE,systrace),	/* 50 system call tracing */
#ifdef XFS
	cdev_xfs_init(NXFS,xfs_dev),	/* 51: xfs communication device */
#else
	cdev_notdef(),			/* 51 */
#endif
	cdev_ptm_init(NPTY,ptm),	/* 52: pseudo-tty ptm device */
};
int	nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

int	mem_no = 2;	/* major device number of memory special file */

/*
 * Swapdev is a fake device implemented
 * in sw.c used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */
dev_t	swapdev = makedev(3, 0);

/*
 * Returns true if dev is /dev/mem or /dev/kmem.
 */
int
iskmemdev(dev)
	dev_t dev;
{

	return (major(dev) == mem_no && minor(dev) < 2);
}

/*
 * Returns true if dev is /dev/zero.
 */
int
iszerodev(dev)
	dev_t dev;
{

	return (major(dev) == mem_no && minor(dev) == 12);
}

dev_t
getnulldev()
{
	return makedev(mem_no, 2);
}

int chrtoblktbl[] = {
	/* XXXX This needs to be dynamic for LKMs. */
	/*VCHR*/	/*VBLK*/
	/*  0 */	NODEV,
	/*  1 */	NODEV,
	/*  2 */	NODEV,
	/*  3 */	NODEV,
	/*  4 */	NODEV,
	/*  5 */	NODEV,
	/*  6 */	NODEV,
	/*  7 */	NODEV,
	/*  8 */	4,	/* SCSI disk */
	/*  9 */	6,	/* SCSI CD-ROM */
	/* 10 */	NODEV,
	/* 11 */	NODEV,
	/* 12 */	NODEV,
	/* 13 */	NODEV,
	/* 14 */	NODEV,
	/* 15 */	NODEV,
	/* 16 */	NODEV,
	/* 17 */	NODEV,
	/* 18 */	7,	/* ram disk */
	/* 19 */	8,	/* vnode disk */
	/* 20 */	NODEV,
	/* 21 */	NODEV,
	/* 22 */	NODEV,
	/* 23 */	NODEV,
	/* 24 */	NODEV,
	/* 25 */	NODEV,
	/* 26 */	10,	/* XD disk */
};
int nchrtoblktbl = sizeof(chrtoblktbl) / sizeof(chrtoblktbl[0]);

/*
 * This entire table could be autoconfig()ed but that would mean that
 * the kernel's idea of the console would be out of sync with that of
 * the standalone boot.  I think it best that they both use the same
 * known algorithm unless we see a pressing need otherwise.
 */
#include <dev/cons.h>

#define clcnpollc	nullcnpollc
cons_decl(cl);
#define dartcnpollc	nullcnpollc
cons_decl(dart);

struct	consdev constab[] = {
#if NDART > 0
	cons_init(dart),
#endif
#if NCL > 0
	cons_init(cl),
#endif
	{ 0 },
};
