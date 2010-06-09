/*	$OpenBSD: conf.c,v 1.45 2010/06/09 15:25:32 jsing Exp $	*/
/*	$NetBSD: conf.c,v 1.41 1997/02/11 07:35:49 scottr Exp $	*/

/*
 * Copyright (c) 1990 The Regents of the University of California.
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
 */
/*-
 * Derived a long time ago from
 *      @(#)conf.c	7.9 (Berkeley) 5/28/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <dev/cons.h>

#include "st.h"
#include "sd.h"
#include "cd.h"
#include "ch.h"
#include "vnd.h"
#include "ccd.h"
#include "rd.h"

struct bdevsw	bdevsw[] =
{
	bdev_notdef(),         		/* 0 */
	bdev_notdef(),			/* 1 */
	bdev_notdef(),         		/* 2 */
	bdev_swap_init(1,sw),		/* 3: swap pseudo-device */
	bdev_disk_init(NSD,sd),		/* 4: SCSI disk */
	bdev_tape_init(NST,st),		/* 5: SCSI tape */
	bdev_disk_init(NCD,cd),		/* 6: SCSI CD-ROM */
	bdev_notdef(),        	 	/* 7 */
	bdev_disk_init(NVND,vnd),	/* 8: vnode disk driver */
	bdev_disk_init(NCCD,ccd),	/* 9: concatenated disk driver */
	bdev_notdef(),        	 	/* 10 */
	bdev_notdef(),        	 	/* 11 */
	bdev_notdef(),        	 	/* 12 */
	bdev_disk_init(NRD,rd),	 	/* 13: RAM disk -- for install */
	bdev_lkm_dummy(),		/* 14 */
	bdev_lkm_dummy(),		/* 15 */
	bdev_lkm_dummy(),		/* 16 */
	bdev_lkm_dummy(),		/* 17 */
	bdev_lkm_dummy(),		/* 18 */
	bdev_lkm_dummy(),		/* 19 */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

#define mmread	mmrw
#define mmwrite	mmrw
cdev_decl(mm);
#include "bio.h"
#include "pty.h"
#include "ss.h"
#include "uk.h"
cdev_decl(fd);
#include "zsc.h"
cdev_decl(zsc);
#include "zstty.h"
cdev_decl(zs);
#include "bpfilter.h"
#include "tun.h"
#include "asc.h"
cdev_decl(asc);
#include "ksyms.h"
#ifdef NNPFS
#include <nnpfs/nnnpfs.h>
cdev_decl(nnpfs_dev);
#endif
#include "wsdisplay.h"
#include "wskbd.h"
#include "wsmouse.h"
#include "wsmux.h"

#include "pf.h"

#include "systrace.h"

#include "vscsi.h"

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_ctty_init(1,ctty),		/* 1: controlling terminal */
	cdev_mm_init(1,mm),		/* 2: /dev/{null,mem,kmem,...} */
	cdev_notdef(),			/* 3 was /dev/drum */
	cdev_tty_init(NPTY,pts),	/* 4: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 5: pseudo-tty master */
	cdev_log_init(1,log),		/* 6: /dev/klog */
	cdev_notdef(),			/* 7 */
	cdev_notdef(),			/* 8 */
	cdev_notdef(),			/* 9 */
	cdev_notdef(),			/* 10 was GRF */
	cdev_notdef(),			/* 11 was ITE */
	cdev_tty_init(NZSTTY,zs),	/* 12: 2 mac serial ports -- BG*/
	cdev_disk_init(NSD,sd),		/* 13: SCSI disk */
	cdev_tape_init(NST,st),		/* 14: SCSI tape */
	cdev_disk_init(NCD,cd),		/* 15: SCSI CD-ROM */
	cdev_notdef(),			/* 16 */
	cdev_ch_init(NCH,ch),		/* 17: SCSI autochanger */
        cdev_disk_init(NRD,rd),         /* 18: ramdisk device */
	cdev_disk_init(NVND,vnd),	/* 19: vnode disk driver */
	cdev_disk_init(NCCD,ccd),	/* 20: concatenated disk driver */
	cdev_fd_init(1,filedesc),	/* 21: file descriptor pseudo-device */
	cdev_bpf_init(NBPFILTER,bpf),	/* 22: Berkeley packet filter */
	cdev_notdef(),			/* 23 was ADB */
	cdev_tun_init(NTUN,tun),	/* 24: network tunnel */
	cdev_lkm_init(NLKM,lkm),	/* 25: loadable module driver */
	cdev_lkm_dummy(),		/* 26 */
	cdev_lkm_dummy(),		/* 27 */
	cdev_lkm_dummy(),		/* 28 */
	cdev_lkm_dummy(),		/* 29 */
	cdev_lkm_dummy(),		/* 30 */
	cdev_lkm_dummy(),		/* 31 */
	cdev_random_init(1,random),	/* 32: random data source */
	cdev_ss_init(NSS,ss),           /* 33: SCSI scanner */
	cdev_uk_init(NUK,uk),		/* 34: SCSI unknown */
	cdev_pf_init(NPF,pf),		/* 35: packet filter */
	cdev_audio_init(NASC,asc),      /* 36: ASC audio device */
	cdev_ksyms_init(NKSYMS,ksyms),	/* 37: Kernel symbols device */
	cdev_wsdisplay_init(NWSDISPLAY, wsdisplay), /* 38: displays */
	cdev_mouse_init(NWSKBD, wskbd),	/* 39: keyboards */
	cdev_mouse_init(NWSMOUSE, wsmouse), /* 40: mice */
	cdev_mouse_init(NWSMUX, wsmux),	/* 41: ws multiplexor */
	cdev_notdef(),			/* 42 */
	cdev_notdef(),			/* 43 */
	cdev_notdef(),			/* 44 */
	cdev_notdef(),			/* 45 */
	cdev_notdef(),			/* 46 */
	cdev_notdef(),			/* 47 */
	cdev_notdef(),			/* 48 */
	cdev_bio_init(NBIO,bio),	/* 49: ioctl tunnel */
	cdev_systrace_init(NSYSTRACE,systrace),	/* 50 system call tracing */
#ifdef NNPFS
	cdev_nnpfs_init(NNNPFS,nnpfs_dev),	/* 51: nnpfs communication device */
#else
	cdev_notdef(),			/* 51 */
#endif
	cdev_ptm_init(NPTY,ptm),	/* 52: pseudo-tty ptm device */
	cdev_vscsi_init(NVSCSI,vscsi),	/* 53: vscsi */
	cdev_disk_init(1,diskmap),	/* 54: disk mapper */
};
int	nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

int	mem_no = 2; 	/* major device number of memory special file */

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
	dev_t	dev;
{

	return (major(dev) == mem_no && minor(dev) < 2);
}

/*
 * Returns true if dev is /dev/zero.
 */
int
iszerodev(dev)
	dev_t	dev;
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
	/*  8 */	NODEV,
	/*  9 */	NODEV,
	/* 10 */	NODEV,
	/* 11 */	NODEV,
	/* 12 */	NODEV,
	/* 13 */	4,		/* sd */
	/* 14 */	5,		/* st */
	/* 15 */	6,		/* cd */
	/* 16 */	NODEV,
	/* 17 */	NODEV,
	/* 18 */	13,		/* rd */
	/* 19 */	8,		/* vnd */
	/* 20 */	9,		/* ccd */
};
int nchrtoblktbl = sizeof(chrtoblktbl) / sizeof(chrtoblktbl[0]);

cons_decl(ws);
#define zscnpollc	nullcnpollc
cons_decl(zs);

struct	consdev constab[] = {
#if NWSDISPLAY > 0
	cons_init(ws),
#endif
#if NZSTTY > 0
	cons_init(zs),
#endif
	{ 0 },
};
