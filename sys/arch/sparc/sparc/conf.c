/*	$OpenBSD: conf.c,v 1.4 1996/02/21 12:53:53 mickey Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)conf.c	8.3 (Berkeley) 11/14/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/vnode.h>
#include <sys/tty.h>
#include <sys/conf.h>

int	ttselect	__P((dev_t, int, struct proc *));

#ifdef LKM
int	lkmenodev();
#else
#define	lkmenodev	enodev
#endif

bdev_decl(sw);
#include "sd.h"
bdev_decl(sd);
#include "xd.h"
bdev_decl(xd);
#include "xy.h"
bdev_decl(xy);
#include "st.h"
bdev_decl(st);
#include "cd.h"
bdev_decl(cd);
#include "vnd.h"
bdev_decl(vnd);
#define fdopen  Fdopen	/* conflicts with fdopen() in kern_descrip.c */
#include "fd.h"
bdev_decl(fd);
#undef  fdopen
#include "ccd.h"
bdev_decl(ccd);

struct bdevsw	bdevsw[] =
{
	bdev_notdef(),			/* 0 */
	bdev_notdef(),			/* 1 */
	bdev_notdef(),			/* 2 */
	bdev_disk_init(NXY,xy),		/* 3: XY SMD disk */
	bdev_swap_init(1,sw),		/* 4 */
	bdev_notdef(),			/* 5 */
	bdev_notdef(),			/* 6 */
	bdev_disk_init(NSD,sd),		/* 7: SCSI disk */
	bdev_disk_init(NVND,vnd),	/* 8: vnode disk driver */
	bdev_disk_init(NCCD,ccd),	/* 9: concatenated disk driver */
	bdev_disk_init(NXD,xd),		/* 10: XD SMD disk */
	bdev_tape_init(NST,st),		/* 11: SCSI tape */
	bdev_notdef(),			/* 12 */
	bdev_notdef(),			/* 13 */
	bdev_notdef(),			/* 14 */
	bdev_notdef(),			/* 15 */
#define fdopen  Fdopen	/* conflicts with fdopen() in kern_descrip.c */
	bdev_disk_init(NFD,fd),		/* 16: floppy disk */
#undef  fdopen
	bdev_notdef(),			/* 17 */
	bdev_disk_init(NCD,cd),		/* 18: SCSI CD-ROM */
	bdev_lkm_dummy(),		/* 19 */
	bdev_lkm_dummy(),		/* 20 */
	bdev_lkm_dummy(),		/* 21 */
	bdev_lkm_dummy(),		/* 22 */
	bdev_lkm_dummy(),		/* 23 */
	bdev_lkm_dummy(),		/* 24 */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

/* open, close, read, write, ioctl, select */
#define	cdev_gen_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) nullop, \
	0, dev_init(c,n,select), (dev_type_mmap((*))) enodev }

/* open, close, ioctl */
#define	cdev_openprom_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) nullop, 0, (dev_type_select((*))) enodev, \
	(dev_type_mmap((*))) enodev }

cdev_decl(cn);
cdev_decl(ctty);
#define	mmread	mmrw
#define	mmwrite	mmrw
cdev_decl(mm);
cdev_decl(sw);
#include "zs.h"
cdev_decl(zs);
cdev_decl(ms);
cdev_decl(log);
cdev_decl(sd);
cdev_decl(st);
cdev_decl(xd);
cdev_decl(xy);
#include "ch.h"
cdev_decl(ch);
#include "pty.h"
#define	ptstty		ptytty
#define	ptsioctl	ptyioctl
cdev_decl(pts);
#define	ptctty		ptytty
#define	ptcioctl	ptyioctl
cdev_decl(ptc);
cdev_decl(fb);
dev_decl(fd,open);
#include "bwtwo.h"
cdev_decl(bwtwo);
cdev_decl(kbd);
#define	fdopen	Fdopen
cdev_decl(fd);
#undef	fdopen
#include "cgthree.h"
cdev_decl(cgthree);
#include "cgfour.h"
cdev_decl(cgfour);
cdev_decl(cd);
#include "cgsix.h"
cdev_decl(cgsix);
#include "cgeight.h"
cdev_decl(cgeight);
#include "audio.h"
cdev_decl(audio);
cdev_decl(openprom);
#include "bpfilter.h"
cdev_decl(bpf);
cdev_decl(vnd);
cdev_decl(ccd);
#include "tun.h"
cdev_decl(tun);
cdev_decl(svr4_net);
#ifdef LKM
#define NLKM 1
#else
#define NLKM 0
#endif
cdev_decl(lkm);

/* open, close, read, ioctl */
cdev_decl(ipl);
#ifdef IPFILTER
#define NIPF 1
#else
#define NIPF 0
#endif

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_notdef(),			/* 1 */
	cdev_ctty_init(1,ctty),		/* 2: controlling terminal */
	cdev_mm_init(1,mm),		/* 3: /dev/{null,mem,kmem,...} */
	cdev_notdef(),			/* 4 */
	cdev_notdef(),			/* 5 */
	cdev_notdef(),			/* 6 */
	cdev_swap_init(1,sw),		/* 7: /dev/drum (swap pseudo-device) */
	cdev_notdef(),			/* 8 */
	cdev_disk_init(NXY,xy),		/* 9: XY SMD disk */
	cdev_notdef(),			/* 10 */
	cdev_notdef(),			/* 11 */
	cdev_tty_init(NZS,zs),		/* 12: zs serial */
	cdev_gen_init(1,ms),		/* 13: /dev/mouse */
	cdev_notdef(),			/* 14 */
	cdev_notdef(),			/* 15: sun /dev/winNNN */
	cdev_log_init(1,log),		/* 16: /dev/klog */
	cdev_disk_init(NSD,sd),		/* 17: SCSI disk */
	cdev_tape_init(NST,st),		/* 18: SCSI tape */
	cdev_ch_init(NCH,ch),		/* 19: SCSI autochanger */
	cdev_tty_init(NPTY,pts),	/* 20: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 21: pseudo-tty master */
	cdev_fb_init(1,fb),		/* 22: /dev/fb indirect driver */
	cdev_disk_init(NCCD,ccd),	/* 23: concatenated disk driver */
	cdev_fd_init(1,fd),		/* 24: file descriptor pseudo-device */
	cdev_notdef(),			/* 25 */
	cdev_notdef(),			/* 26 */
	cdev_fb_init(NBWTWO,bwtwo),	/* 27: /dev/bwtwo */
	cdev_notdef(),			/* 28 */
	cdev_gen_init(1,kbd),		/* 29: /dev/kbd */
	cdev_notdef(),			/* 30 */
	cdev_notdef(),			/* 31: should be /dev/cgtwo */
	cdev_notdef(),			/* 32: should be /dev/gpone */
	cdev_notdef(),			/* 33 */
	cdev_notdef(),			/* 34 */
	cdev_notdef(),			/* 35 */
	cdev_notdef(),			/* 36 */
	cdev_notdef(),			/* 37 */
	cdev_notdef(),			/* 38 */
	cdev_fb_init(NCGFOUR,cgfour),	/* 39: /dev/cgfour */
	cdev_notdef(),			/* 40 */
	cdev_notdef(),			/* 41 */
	cdev_disk_init(NXD,xd),		/* 42: XD SMD disk */
#ifdef COMPAT_SVR4
	cdev_svr4_net_init(1,svr4_net),	/* 43: svr4 net pseudo-device */
#else
	cdev_notdef(),			/* 43 */
#endif
	cdev_notdef(),			/* 44 */
	cdev_notdef(),			/* 45 */
	cdev_notdef(),			/* 46 */
	cdev_notdef(),			/* 47 */
	cdev_notdef(),			/* 48 */
	cdev_notdef(),			/* 49 */
	cdev_notdef(),			/* 50 */
	cdev_notdef(),			/* 51 */
	cdev_notdef(),			/* 52 */
	cdev_notdef(),			/* 53 */
#define fdopen  Fdopen	/* conflicts with fdopen() in kern_descrip.c */
	cdev_disk_init(NFD,fd),		/* 54: floppy disk */
#undef  fdopen
	cdev_fb_init(NCGTHREE,cgthree),	/* 55: /dev/cgthree */
	cdev_notdef(),			/* 56 */
	cdev_notdef(),			/* 57 */
	cdev_disk_init(NCD,cd),		/* 58 SCSI CD-ROM */
	cdev_gen_ipf(NIPF,ipl),		/* 59 */
	cdev_notdef(),			/* 60 */
	cdev_notdef(),			/* 61 */
	cdev_notdef(),			/* 62 */
	cdev_notdef(),			/* 63 */
	cdev_fb_init(NCGEIGHT,cgeight),	/* 64: /dev/cgeight */
	cdev_notdef(),			/* 65 */
	cdev_notdef(),			/* 66 */
	cdev_fb_init(NCGSIX,cgsix),	/* 67: /dev/cgsix */
	cdev_notdef(),			/* 68 */
	cdev_gen_init(NAUDIO,audio),	/* 69: /dev/audio */
	cdev_openprom_init(1,openprom),	/* 70: /dev/openprom */
	cdev_notdef(),			/* 71 */
	cdev_notdef(),			/* 72 */
	cdev_notdef(),			/* 73 */
	cdev_notdef(),			/* 74 */
	cdev_notdef(),			/* 75 */
	cdev_notdef(),			/* 76 */
	cdev_notdef(),			/* 77 */
	cdev_notdef(),			/* 78 */
	cdev_notdef(),			/* 79 */
	cdev_notdef(),			/* 80 */
	cdev_notdef(),			/* 81 */
	cdev_notdef(),			/* 82 */
	cdev_notdef(),			/* 83 */
	cdev_notdef(),			/* 84 */
	cdev_notdef(),			/* 85 */
	cdev_notdef(),			/* 86 */
	cdev_notdef(),			/* 87 */
	cdev_notdef(),			/* 88 */
	cdev_notdef(),			/* 89 */
	cdev_notdef(),			/* 90 */
	cdev_notdef(),			/* 91 */
	cdev_notdef(),			/* 92 */
	cdev_notdef(),			/* 93 */
	cdev_notdef(),			/* 94 */
	cdev_notdef(),			/* 95 */
	cdev_notdef(),			/* 96 */
	cdev_notdef(),			/* 97 */
	cdev_notdef(),			/* 98 */
	cdev_notdef(),			/* 99 */
	cdev_notdef(),			/* 100 */
	cdev_notdef(),			/* 101 */
	cdev_notdef(),			/* 102 */
	cdev_notdef(),			/* 103 */
	cdev_notdef(),			/* 104 */
	cdev_bpftun_init(NBPFILTER,bpf),/* 105: packet filter */
	cdev_notdef(),			/* 106 */
	cdev_notdef(),			/* 107 */
	cdev_notdef(),			/* 108 */
	cdev_notdef(),			/* 109 */
	cdev_disk_init(NVND,vnd),	/* 110: vnode disk driver */
	cdev_bpftun_init(NTUN,tun),	/* 111: network tunnel */
	cdev_lkm_init(NLKM,lkm),	/* 112: loadable module driver */
	cdev_lkm_dummy(),		/* 113 */
	cdev_lkm_dummy(),		/* 114 */
	cdev_lkm_dummy(),		/* 115 */
	cdev_lkm_dummy(),		/* 116 */
	cdev_lkm_dummy(),		/* 117 */
	cdev_lkm_dummy(),		/* 118 */
};
int	nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

int	mem_no = 3; 	/* major device number of memory special file */

/*
 * Swapdev is a fake device implemented
 * in sw.c used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */
dev_t	swapdev = makedev(4, 0);

/*
 * Routine that identifies /dev/mem and /dev/kmem.
 *
 * A minimal stub routine can always return 0.
 */
iskmemdev(dev)
	dev_t dev;
{

	return (major(dev) == mem_no && minor(dev) < 2);
}

iszerodev(dev)
	dev_t dev;
{
	return (major(dev) == mem_no && minor(dev) == 12);
}

static int chrtoblktbl[] = {
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
	/*  8 */	NODEV,
	/*  9 */	3,
	/* 10 */	NODEV,
	/* 11 */	NODEV,
	/* 12 */	NODEV,
	/* 13 */	NODEV,
	/* 14 */	NODEV,
	/* 15 */	NODEV,
	/* 16 */	NODEV,
	/* 17 */	7,
	/* 18 */	11,
	/* 19 */	NODEV,
	/* 20 */	NODEV,
	/* 21 */	NODEV,
	/* 22 */	NODEV,
	/* 23 */	9,
	/* 24 */	NODEV,
	/* 25 */	NODEV,
	/* 26 */	NODEV,
	/* 27 */	NODEV,
	/* 28 */	NODEV,
	/* 29 */	NODEV,
	/* 30 */	NODEV,
	/* 31 */	NODEV,
	/* 32 */	NODEV,
	/* 33 */	NODEV,
	/* 34 */	NODEV,
	/* 35 */	NODEV,
	/* 36 */	NODEV,
	/* 37 */	NODEV,
	/* 38 */	NODEV,
	/* 39 */	NODEV,
	/* 40 */	NODEV,
	/* 41 */	NODEV,
	/* 42 */	10,
	/* 43 */	NODEV,
	/* 44 */	NODEV,
	/* 45 */	NODEV,
	/* 46 */	NODEV,
	/* 47 */	NODEV,
	/* 48 */	NODEV,
	/* 49 */	NODEV,
	/* 50 */	NODEV,
	/* 51 */	NODEV,
	/* 52 */	NODEV,
	/* 53 */	NODEV,
	/* 54 */	16,
	/* 55 */	NODEV,
	/* 56 */	NODEV,
	/* 57 */	NODEV,
	/* 58 */	18,
	/* 59 */	NODEV,
	/* 60 */	NODEV,
	/* 61 */	NODEV,
	/* 62 */	NODEV,
	/* 63 */	NODEV,
	/* 64 */	NODEV,
	/* 65 */	NODEV,
	/* 66 */	NODEV,
	/* 67 */	NODEV,
	/* 68 */	NODEV,
	/* 69 */	NODEV,
	/* 70 */	NODEV,
	/* 71 */	NODEV,
	/* 72 */	NODEV,
	/* 73 */	NODEV,
	/* 74 */	NODEV,
	/* 75 */	NODEV,
	/* 76 */	NODEV,
	/* 77 */	NODEV,
	/* 78 */	NODEV,
	/* 79 */	NODEV,
	/* 80 */	NODEV,
	/* 81 */	NODEV,
	/* 82 */	NODEV,
	/* 83 */	NODEV,
	/* 84 */	NODEV,
	/* 85 */	NODEV,
	/* 86 */	NODEV,
	/* 87 */	NODEV,
	/* 88 */	NODEV,
	/* 89 */	NODEV,
	/* 90 */	NODEV,
	/* 91 */	NODEV,
	/* 92 */	NODEV,
	/* 93 */	NODEV,
	/* 94 */	NODEV,
	/* 95 */	NODEV,
	/* 96 */	NODEV,
	/* 97 */	NODEV,
	/* 98 */	NODEV,
	/* 99 */	NODEV,
	/*100 */	NODEV,
	/*101 */	NODEV,
	/*102 */	NODEV,
	/*103 */	NODEV,
	/*104 */	NODEV,
	/*105 */	NODEV,
	/*106 */	NODEV,
	/*107 */	NODEV,
	/*108 */	NODEV,
	/*109 */	NODEV,
	/*110 */	8,
	/*111 */	NODEV,
	/*112 */	NODEV,
	/*113 */	NODEV,
	/*114 */	NODEV,
	/*115 */	NODEV,
	/*116 */	NODEV,
	/*117 */	NODEV,
	/*118 */	NODEV,
};

/*
 * Routine to convert from character to block device number.
 */
chrtoblk(dev)
	dev_t dev;
{
	int blkmaj;

	if (major(dev) >= nchrdev)
		return (NODEV);
	blkmaj = chrtoblktbl[major(dev)];
	if (blkmaj == NODEV)
		return (NODEV);
	return (makedev(blkmaj, minor(dev)));
}
