/*	$OpenBSD: conf.c,v 1.28 2010/07/03 03:59:17 krw Exp $ */

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
#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include <machine/conf.h>

#if 0
#include "wd.h"
#else
#define	NWD 0
#endif
bdev_decl(wd);
#include "sd.h"
#include "cd.h"

#include "rd.h"

#include "vnd.h"
#include "ccd.h"
#include "raid.h"

struct bdevsw bdevsw[] = {
	bdev_disk_init(NWD,wd),		/* 0: ST506/ESDI/IDE disk */
	bdev_swap_init(1,sw),		/* 1 swap pseudo device */
	bdev_disk_init(NSD,sd),		/* 2 SCSI Disk */
	bdev_disk_init(NCD,cd),		/* 3 SCSI CD-ROM */
	bdev_notdef(),                  /* 4 unknown*/
	bdev_tape_init(NST,st),		/* 5 SCSI tape */
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
	bdev_disk_init(NRAID,raid),	/* 19: RAIDframe disk driver */
};
int nblkdev = sizeof bdevsw / sizeof bdevsw[0];

#include "bio.h"
#include "pty.h"

#include "bugtty.h"
cdev_decl(bugtty);

#include "st.h"
#include "uk.h"

cdev_decl(wd);

#include "bpfilter.h"

#include "tun.h"

#ifdef NNPFS
#include <nnpfs/nnnpfs.h>
cdev_decl(nnpfs_dev);
#endif

#ifdef LKM
#define NLKM 1
#else
#define NLKM 0
#endif  
 
#include "ksyms.h"

#include "pf.h"

#include "systrace.h"

#include "vscsi.h"

struct cdevsw cdevsw[] = {
        cdev_cn_init(1,cn),             /* 0: virtual console */
        cdev_ctty_init(1,ctty),         /* 1: controlling terminal */
        cdev_mm_init(1,mm),             /* 2: /dev/{null,mem,kmem,...} */
        cdev_notdef(),                  /* 3 was /dev/drum */
        cdev_tty_init(NPTY,pts),        /* 4: pseudo-tty slave */
        cdev_ptc_init(NPTY,ptc),        /* 5: pseudo-tty master */
        cdev_log_init(1,log),           /* 6: /dev/klog */
#if 0
	cdev_tty_init(NCOM,com),	/* 7: Serial ports */
#else
        cdev_notdef(),                  /* 7 */
#endif 
        cdev_disk_init(NSD,sd),         /* 8: SCSI disk */
        cdev_disk_init(NCD,cd),         /* 9: SCSI CD-ROM */
        cdev_notdef(),                  /* 10: SCSI changer */
	cdev_disk_init(NWD,wd),		/* 11: ST506/ESDI/IDE disk */
        cdev_notdef(),			/* 12 */
	cdev_notdef(),			/* 13 */
	cdev_tty_init(NBUGTTY,bugtty),  /* 14: BUGtty (ttyB) */
        cdev_notdef(),                  /* 15 */
        cdev_notdef(),                  /* 16 */
	cdev_disk_init(NRD,rd),		/* 17: ram disk driver*/
	cdev_disk_init(NCCD,ccd),	/* 18: concatenated disk driver */
        cdev_disk_init(NVND,vnd),       /* 19: vnode disk */
        cdev_tape_init(NST,st),         /* 20: SCSI tape */
        cdev_fd_init(1,filedesc),       /* 21: file descriptor pseudo-dev */
        cdev_bpf_init(NBPFILTER,bpf),	/* 22: berkeley packet filter */
        cdev_tun_init(NTUN,tun),	/* 23: network tunnel */
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
        cdev_pf_init(NPF,pf),           /* 39: packet filter */
        cdev_random_init(1,random),     /* 40: random data source */
	cdev_uk_init(NUK,uk),		/* 41: unknown SCSI */
        cdev_notdef(),                  /* 42 */
	cdev_ksyms_init(NKSYMS,ksyms),	/* 43: Kernel symbols device */
        cdev_notdef(),                  /* 44 */
        cdev_notdef(),                  /* 45 */
        cdev_notdef(),                  /* 46 */
        cdev_notdef(),                  /* 47 */
        cdev_notdef(),                  /* 48 */
        cdev_notdef(),                  /* 49 */ 
        cdev_systrace_init(NSYSTRACE,systrace),	/* 50: system call tracing */ 
#ifdef NNPFS
	cdev_nnpfs_init(NNNPFS,nnpfs_dev),	/* 51: nnpfs communication device */
#else
        cdev_notdef(),                  /* 51 */
#endif
        cdev_notdef(),                  /* 52 */ 
	cdev_bio_init(NBIO,bio),	/* 53: ioctl tunnel */
	cdev_disk_init(NRAID,raid),	/* 54: RAIDframe disk driver */
	cdev_ptm_init(NPTY,ptm),	/* 55: pseudo-tty ptm device */
	cdev_vscsi_init(NVSCSI,vscsi),	/* 56: vscsi */
	cdev_disk_init(1,diskmap),	/* 57: disk mapper */
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

dev_t
getnulldev()
{
	return makedev(mem_no, 2);
}

int chrtoblktbl[] = {
	/*VCHR*/	/*VBLK*/
	/*  0 */	NODEV,
	/*  1 */	NODEV,
	/*  2 */	NODEV,
	/*  3 */	NODEV,
	/*  4 */	NODEV,
	/*  5 */	NODEV,
	/*  6 */	NODEV,
	/*  7 */	NODEV,
	/*  8 */	2,		/* sd */
	/*  9 */	3,		/* cd */
	/* 10 */	NODEV,
	/* 11 */	0,		/* wd */
	/* 12 */	NODEV,
	/* 13 */	NODEV,
	/* 14 */	NODEV,
	/* 15 */	NODEV,
	/* 16 */	NODEV,
	/* 17 */	17,		/* rd */
	/* 18 */	16,		/* ccd */
	/* 19 */	14,		/* vnd */
	/* 20 */	5,		/* st */
	/* 21 */	NODEV,
	/* 22 */	NODEV,
	/* 23 */	NODEV,
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
	/* 42 */	NODEV,
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
	/* 54 */	19,		/* raid */
};
int nchrtoblktbl = sizeof(chrtoblktbl) / sizeof(chrtoblktbl[0]);

#include <dev/cons.h>

#define bugttycnpollc      nullcnpollc
cons_decl(bugtty);

struct consdev constab[] = {
#if NBUGTTY > 0
	cons_init(bugtty),
#endif
	{ 0 },
};
