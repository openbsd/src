/*	$NetBSD: conf.c,v 1.2 1996/10/16 17:26:19 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include "ofdisk.h"
bdev_decl(ofd);
bdev_decl(sw);

struct bdevsw bdevsw[] = {
	bdev_notdef(),                  /* 0 */
	bdev_swap_init(1,sw),		/* 1: swap pseudo device */
	bdev_notdef(),                  /* 2 SCSI tape */
	bdev_notdef(),                  /* 3 SCSI CD-ROM */
	bdev_disk_init(NOFDISK,ofd),	/* 4: Openfirmware disk */
};
int nblkdev = sizeof bdevsw / sizeof bdevsw[0];

cdev_decl(cn);
cdev_decl(ctty);
#define mmread	mmrw
#define	mmwrite	mmrw
cdev_decl(mm);
#include "pty.h"
#define	ptstty		ptytty
#define	ptsioctl	ptyioctl
cdev_decl(pts);
#define	ptctty		ptytty
#define	ptcioctl	ptyioctl
cdev_decl(ptc);
cdev_decl(log);
cdev_decl(sw);
#include "ofcons.h"
cdev_decl(ofc);
cdev_decl(ofd);
#include "ofrtc.h"
cdev_decl(ofrtc);

#include <sd.h>
#include <st.h>
#include <cd.h>
#include <vnd.h>
cdev_decl(st);  
cdev_decl(sd);
cdev_decl(cd);
cdev_decl(vnd);
cdev_decl(ccd);

dev_decl(filedesc,open);

#include "bpfilter.h"
cdev_decl(bpf); 

#include "tun.h"
cdev_decl(tun);
cdev_decl(random); 

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

#define	cdev_rtc_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), \
	dev_init(c,n,read), dev_init(c,n,write), \
	(dev_type_ioctl((*))) enodev, (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev }

struct cdevsw cdevsw[] = {
        cdev_cn_init(1,cn),             /* 0: virtual console */
        cdev_ctty_init(1,ctty),         /* 1: controlling terminal */
        cdev_mm_init(1,mm),             /* 2: /dev/{null,mem,kmem,...} */
        cdev_swap_init(1,sw),           /* 3: /dev/drum (swap pseudo-device) */
        cdev_tty_init(NPTY,pts),        /* 4: pseudo-tty slave */
        cdev_ptc_init(NPTY,ptc),        /* 5: pseudo-tty master */
        cdev_log_init(1,log),           /* 6: /dev/klog */
	cdev_tty_init(NOFCONS,ofc),	/* 7: Openfirmware console */
        cdev_disk_init(NSD,sd),         /* 8: SCSI disk */
        cdev_disk_init(NCD,cd),         /* 9: SCSI CD-ROM */
        cdev_notdef(),                  /* 10 */
        cdev_notdef(),                  /* 11 */
        cdev_notdef(),                  /* 12 */
	cdev_disk_init(NOFDISK,ofd),	/* 13: Openfirmware disk */
        cdev_notdef(),                  /* 14 */
        cdev_notdef(),                  /* 15 */
        cdev_notdef(),                  /* 16 */
	cdev_rtc_init(NOFRTC,ofrtc),	/* 17: Openfirmware RTC */
        cdev_notdef(),                  /* 18 */
        cdev_disk_init(NVND,vnd),       /* 19: vnode disk */
        cdev_tape_init(NST,st),         /* 20: SCSI tape */
        cdev_fd_init(1,filedesc),       /* 21: file descriptor pseudo-dev */
        cdev_bpftun_init(NBPFILTER,bpf),/* 22: berkeley packet filter */
        cdev_bpftun_init(NTUN,tun),     /* 23: network tunnel */
        cdev_lkm_init(NLKM,lkm),        /* 24: loadable module driver */
        cdev_notdef(),                  /* 25 */ 
        cdev_notdef(),                  /* 26 */
        cdev_notdef(),                  /* 27 */
        cdev_notdef(),                  /* 28 */
        cdev_notdef(),                  /* 29 */
        cdev_notdef(),                  /* 30 */
        cdev_notdef(),                  /* 31 */
        cdev_notdef(),                  /* 32 */
        cdev_lkm_dummy(),               /* 33 */
        cdev_lkm_dummy(),               /* 34 */
        cdev_lkm_dummy(),               /* 35 */
        cdev_lkm_dummy(),               /* 36 */
        cdev_lkm_dummy(),               /* 37 */
        cdev_lkm_dummy(),               /* 38 */
        cdev_gen_ipf(NIPF,ipl),         /* 39: IP filter */
        cdev_random_init(1,random),     /* 40: random data source */
};
int nchrdev = sizeof cdevsw / sizeof cdevsw[0];

int mem_no = 2;				/* major number of /dev/mem */

/*
 * Swapdev is a fake device implemented in sw.c.
 * It is used only internally to get to swstrategy.
 */
dev_t swapdev = makedev(1, 0);

/*
 * Check whether dev is /dev/mem or /dev/kmem.
 */
int
iskmemdev(dev)
	dev_t dev;
{
	return major(dev) == mem_no && minor(dev) < 2;
}

/*
 * Check whether dev is /dev/zero.
 */
int
iszerodev(dev)
	dev_t dev;
{
	return major(dev) == mem_no && minor(dev) == 12;
}

static int chrtoblktbl[] = {
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
	/*  9 */	NODEV,
	/* 10 */	NODEV,
	/* 11 */	NODEV,
	/* 12 */	NODEV,
	/* 13 */	4,
	/* 14 */	NODEV,
	/* 15 */	NODEV,
	/* 16 */	NODEV,
	/* 10 */	NODEV,
	/* 10 */	NODEV,
	/* 10 */	NODEV,
	/* 10 */	NODEV,
	/* 10 */	NODEV,
};

/*
 * Return accompanying block dev for a char dev.
 */
int
chrtoblk(dev)
	dev_t dev;
{
	int major;
	
	if ((major = major(dev)) >= nchrdev)
		return NODEV;
	if ((major = chrtoblktbl[major]) == NODEV)
		return NODEV;
	return makedev(major, minor(dev));
}

/*
 * Convert a character device number to a block device number.
 */
dev_t
blktochr(dev)
	dev_t dev;
{
	int blkmaj = major(dev);
	int i;

	if (blkmaj >= nblkdev)
		return (NODEV);
	for (i = 0; i < sizeof(chrtoblktbl)/sizeof(chrtoblktbl[0]); i++)
		if (blkmaj == chrtoblktbl[i])
			return (makedev(i, minor(dev)));
	return (NODEV);
}

#include <dev/cons.h>

cons_decl(ofc);

struct consdev constab[] = {
#if NOFCONS > 0
	cons_init(ofc),
#endif
	{ 0 },
};
