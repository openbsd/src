/*	$OpenBSD: conf.c,v 1.10 1998/08/06 15:04:06 pefo Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
 * Copyright (c) 1997 RTMX Inc, North Carolina
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
 *	This product includes software developed under OpenBSD for RTMX Inc,
 *	North Carolina, USA, by Per Fogelstrom, Opsycon AB, Sweden.
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
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include "sd.h"
bdev_decl(sd);
bdev_decl(sw);
#include "cd.h"
bdev_decl(cd);

#include "ofdisk.h"
bdev_decl(ofd);

#include "rd.h"
bdev_decl(rd);

#include "vnd.h"
bdev_decl(vnd);
#include "ccd.h"
bdev_decl(ccd);

struct bdevsw bdevsw[] = {
	bdev_notdef(),			/* 0 */
	bdev_swap_init(1,sw),		/* 1 swap pseudo device */
	bdev_disk_init(NSD,sd),		/* 2 SCSI Disk */
	bdev_disk_init(NCD,cd),		/* 3 SCSI CD-ROM */
	bdev_disk_init(NOFDISK,ofd),	/* 4 Openfirmware disk */
	bdev_notdef(),                  /* 5 unknown*/
	bdev_notdef(),                  /* 6 unknown*/
	bdev_notdef(),                  /* 7 unknown*/
	bdev_lkm_dummy(),		/* 8 */
	bdev_lkm_dummy(),		/* 9 */
	bdev_lkm_dummy(),		/* 10 */
	bdev_lkm_dummy(),		/* 11 */
	bdev_lkm_dummy(),		/* 12 */
	bdev_lkm_dummy(),		/* 13 */
	bdev_disk_init(NVND,vnd),	/* 14 vnode disk driver*/
	bdev_notdef(),                  /* 15 unknown*/
	bdev_disk_init(NCCD,ccd),	/* 16 concatenated disk driver*/
	bdev_disk_init(NRD,rd),		/* 17 ram disk driver*/
	bdev_notdef(),                  /* 18 unknown*/
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
#include "com.h"
cdev_decl(com);

#include "ofcons.h"
cdev_decl(ofc);
cdev_decl(ofd);

#include "ofrtc.h"
cdev_decl(ofrtc);

#include <sd.h>
#include <st.h>
#include <cd.h>
cdev_decl(st);  
cdev_decl(sd);
cdev_decl(cd);
cdev_decl(vnd);
cdev_decl(ccd);
cdev_decl(rd);  

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

#ifdef IPFILTER
#define NIPF 1
#else   
#define NIPF 0
#endif  


struct cdevsw cdevsw[] = {
        cdev_cn_init(1,cn),             /* 0: virtual console */
        cdev_ctty_init(1,ctty),         /* 1: controlling terminal */
        cdev_mm_init(1,mm),             /* 2: /dev/{null,mem,kmem,...} */
        cdev_swap_init(1,sw),           /* 3: /dev/drum (swap pseudo-device) */
        cdev_tty_init(NPTY,pts),        /* 4: pseudo-tty slave */
        cdev_ptc_init(NPTY,ptc),        /* 5: pseudo-tty master */
        cdev_log_init(1,log),           /* 6: /dev/klog */
	cdev_tty_init(NCOM,com),	/* 7: Serial ports */
        cdev_disk_init(NSD,sd),         /* 8: SCSI disk */
        cdev_disk_init(NCD,cd),         /* 9: SCSI CD-ROM */
        cdev_notdef(),                  /* 10: SCSI changer */
        cdev_notdef(),                  /* 11 */
        cdev_notdef(),                  /* 12 */
	cdev_disk_init(NOFDISK,ofd),	/* 13 Openfirmware disk */
        cdev_tty_init(NOFCONS,ofc),	/* 14 Openfirmware console */
        cdev_notdef(),                  /* 15 */
        cdev_notdef(),                  /* 16 */
	cdev_disk_init(NRD,rd),		/* 17 ram disk driver*/
	cdev_disk_init(NCCD,ccd),	/* 18 concatenated disk driver */
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
	/* If adding devs, don't forget to expand 'chrtoblktbl' below! */
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
	/*  8 */	2,
	/*  9 */	NODEV,
	/* 10 */	NODEV,
	/* 11 */	NODEV,
	/* 12 */	NODEV,
	/* 13 */	4,
	/* 14 */	NODEV,
	/* 15 */	NODEV,
	/* 16 */	NODEV,
	/* 17 */	17,
};

/*
 * Return accompanying block dev for a char dev.
 */
int
chrtoblk(dev)
	dev_t dev;
{
	int blkmaj;
	
	if (major(dev) >= nchrdev ||
	    major(dev) > sizeof(chrtoblktbl)/sizeof(chrtoblktbl[0]))
		return (NODEV);
	blkmaj = chrtoblktbl[major(dev)];
	if (blkmaj == NODEV)
		return (NODEV);
	return (makedev(blkmaj, minor(dev)));
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

cons_decl(com);
cons_decl(ofc);

struct consdev constab[] = {
#if NOFCONS > 0
	cons_init(ofc),
#endif
#if NCOM > 0
	cons_init(com),
#endif
	{ 0 },
};
