/*	$OpenBSD: conf.c,v 1.33 2002/12/05 02:49:55 kjc Exp $	*/
/*	$NetBSD: conf.c,v 1.51 1996/11/04 16:16:09 gwr Exp $	*/

/*-
 * Copyright (c) 1994 Adam Glass, Gordon W. Ross
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)conf.c	8.2 (Berkeley) 11/14/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/vnode.h>

#include <machine/conf.h>

#include "bpfilter.h"
#include "bwtwo.h"
#include "ccd.h"
#include "cd.h"
#include "cgtwo.h"
#include "cgfour.h"
#include "ch.h"
#include "kbd.h"
#include "ksyms.h"
#include "ms.h"
#include "pty.h"
#include "rd.h"
#include "sd.h"
#include "ses.h"
#include "ss.h"
#include "st.h"
#include "tun.h"
#include "uk.h"
#include "vnd.h"
#include "xd.h"
#include "xy.h"
#include "zstty.h"
#ifdef XFS
#include <xfs/nxfs.h>
cdev_decl(xfs_dev);
#endif

struct bdevsw	bdevsw[] =
{
	bdev_notdef(),			/* 0 */
	bdev_notdef(),			/* 1: tapemaster tape */
	bdev_notdef(),			/* 2 */
	bdev_disk_init(NXY,xy),		/* 3: SMD disk on Xylogics 450/451 */
	bdev_swap_init(1,sw),		/* 4: swap pseudo-device */
	bdev_disk_init(NVND,vnd),	/* 5: vnode disk driver */
	bdev_notdef(),			/* 6 */
	bdev_disk_init(NSD,sd),		/* 7: SCSI disk */
	bdev_tape_init(NXT,xt),		/* 8: Xylogics tape */
	bdev_disk_init(NCCD,ccd),	/* 9: concatenated disk driver */
	bdev_disk_init(NXD,xd),		/* 10: SMD disk on Xylogics 7053 */
	bdev_tape_init(NST,st),		/* 11: SCSI tape */
	bdev_notdef(),			/* 12: Sun ns? */
	bdev_disk_init(NRD,rd),		/* 13: RAM disk - for install tape */
	bdev_notdef(),			/* 14: Sun ft? */
	bdev_notdef(),			/* 15: Sun hd? */
	bdev_notdef(),			/* 16: Sun fd? */
	bdev_notdef(),			/* 17: Sun vd_unused */
	bdev_disk_init(NCD,cd),		/* 18: SCSI CD-ROM */
	bdev_notdef(),			/* 19: Sun vd_unused */
	bdev_notdef(),			/* 20: Sun vd_unused */
	bdev_notdef(),			/* 21: Sun vd_unused */
	bdev_notdef(),			/* 22: Sun IPI disks... */
	bdev_notdef(),			/* 23: Sun IPI disks... */
	bdev_lkm_dummy(),		/* 24 */
	bdev_lkm_dummy(),		/* 25 */
	bdev_lkm_dummy(),		/* 26 */
	bdev_lkm_dummy(),		/* 27 */
	bdev_lkm_dummy(),		/* 28 */
	bdev_lkm_dummy(),		/* 29 */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

#include "pf.h"

#include "systrace.h"

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_tty_init(NKBD,kd),		/* 1: Sun keyboard/display */
	cdev_ctty_init(1,ctty),		/* 2: controlling terminal */
	cdev_mm_init(1,mm),		/* 3: /dev/{null,mem,kmem,...} */
	cdev_notdef(),			/* 4: was PROM console */
	cdev_notdef(),			/* 5: tapemaster tape */
	cdev_notdef(),			/* 6: systech/versatec */
	cdev_swap_init(1,sw),		/* 7: /dev/drum {swap pseudo-device) */
	cdev_notdef(),			/* 8: Archive QIC-11 tape */
	cdev_disk_init(NXY,xy),		/* 9: SMD disk on Xylogics 450/451 */
	cdev_notdef(),			/* 10: systech multi-terminal board */
	cdev_notdef(),			/* 11: DES encryption chip */
	cdev_tty_init(NZSTTY,zs),	/* 12: Zilog 8350 serial port */
	cdev_mouse_init(NMS,ms),	/* 13: Sun mouse */
	cdev_notdef(),			/* 14: cgone */
	cdev_notdef(),			/* 15: /dev/winXXX */
	cdev_log_init(1,log),		/* 16: /dev/klog */
	cdev_disk_init(NSD,sd),		/* 17: SCSI disk */
	cdev_tape_init(NST,st),		/* 18: SCSI tape */
	cdev_disk_init(NVND,vnd),	/* 19: vnode disk driver */
	cdev_tty_init(NPTY,pts),	/* 20: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 21: pseudo-tty master */
	cdev_fb_init(1,fb),		/* 22: /dev/fb indirect driver */
	cdev_fd_init(1,filedesc),	/* 23: file descriptor pseudo-device */
	cdev_bpftun_init(NTUN,tun),	/* 24: network tunnel */
	cdev_notdef(),			/* 25: sun pi? */
	cdev_notdef(),			/* 26: bwone */
	cdev_fb_init(NBWTWO,bw2),	/* 27: bwtwo */
	cdev_notdef(),			/* 28: Systech VPC-2200 versatec/centronics */
	cdev_mouse_init(NKBD,kbd),	/* 29: Sun keyboard */
	cdev_tape_init(NXT,xt),		/* 30: Xylogics tape */
	cdev_fb_init(NCGTWO,cg2),	/* 31: cgtwo */
	cdev_notdef(),			/* 32: /dev/gpone */
	cdev_disk_init(NCCD,ccd),	/* 33: concatenated disk driver */
	cdev_notdef(),			/* 34: floating point accelerator */
	cdev_notdef(),			/* 35 */
	cdev_bpftun_init(NBPFILTER,bpf),/* 36: Berkeley packet filter */
	cdev_ksyms_init(NKSYMS,ksyms),	/* 37: Kernel symbols device */
	cdev_notdef(),			/* 38 */
	cdev_fb_init(NCGFOUR,cg4),	/* 39: cgfour */
	cdev_notdef(),			/* 40: (sni) */
	cdev_notdef(),			/* 41: (sun dump) */
	cdev_disk_init(NXD,xd),		/* 42: SMD disk on Xylogics 7053 */
	cdev_notdef(),			/* 43: (sun hrc) */
	cdev_notdef(),			/* 44: (mcp) */
	cdev_notdef(),			/* 45: (sun ifd) */
	cdev_notdef(),			/* 46: (dcp) */
	cdev_notdef(),			/* 47: (dna) */
	cdev_notdef(),			/* 48: (tbi) */
	cdev_notdef(),			/* 49: (chat) */
	cdev_notdef(),			/* 50: (chut) */
#ifdef XFS
	cdev_xfs_init(NXFS,xfs_dev),	/* 51: xfs communication device */
#else
	cdev_notdef(),			/* 51: (chut) */
#endif
	cdev_disk_init(NRD,rd),		/* 52: RAM disk - for install tape */
	cdev_notdef(),			/* 53: (hd - N/A) */
	cdev_notdef(),			/* 54: (fd - N/A) */
	cdev_notdef(),			/* 55: cgthree */
	cdev_notdef(),			/* 56: (pp) */
	cdev_notdef(),			/* 57: (vd) Loadable Module control */
	cdev_disk_init(NCD,cd),		/* 58: SCSI CD-ROM */
	cdev_notdef(),			/* 59: (vd) Loadable Module stub */
	cdev_notdef(),			/* 60:  ||     ||      ||    ||  */
	cdev_notdef(),			/* 61:  ||     ||      ||    ||  */
	cdev_notdef(),			/* 62: (taac) */
	cdev_notdef(),			/* 63: (tcp/tli) */
	cdev_notdef(),			/* 64: cgeight */
	cdev_notdef(),			/* 65: old IPI */
	cdev_notdef(),			/* 66: (mcp) parallel printer */
	cdev_notdef(),			/* 67: cgsix */
	cdev_notdef(),			/* 68: cgnine */
	cdev_notdef(),			/* 69: /dev/audio */
	cdev_notdef(),			/* 70: open prom */
	cdev_notdef(),			/* 71: (sg?) */
	cdev_random_init(1,random),	/* 72: randomness source */
	cdev_uk_init(NUK,uk),		/* 73: unknown SCSI */
	cdev_ss_init(NSS,ss),           /* 74: SCSI scanner */
	cdev_pf_init(NPF,pf),		/* 75: packet filter */
	cdev_lkm_init(NLKM,lkm),	/* 76: loadable module driver */
	cdev_lkm_dummy(),		/* 77 */
	cdev_lkm_dummy(),		/* 78 */
	cdev_lkm_dummy(),		/* 79 */
	cdev_lkm_dummy(),		/* 80 */
	cdev_lkm_dummy(),		/* 81 */
	cdev_lkm_dummy(),		/* 82 */
	cdev_ch_init(NCH,ch),		/* 83: SCSI autochanger */
	cdev_ses_init(NSES,ses),	/* 84: SCSI SES or SAF-TE device */
	cdev_notdef(),			/* 85: ALTQ (deprecated) */
	cdev_systrace_init(NSYSTRACE,systrace),	/* 86: system call tracing */
};
int	nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

int	mem_no = 3;	/* major device number of memory special file */

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

static int chrtoblktbl[] = {
        /* XXXX This needs to be dynamic for LKMs. */
        /*VCHR*/        /*VBLK*/
	/*  0 */	NODEV,
	/*  1 */	NODEV,
	/*  2 */	NODEV,
	/*  3 */	NODEV,
	/*  4 */	NODEV,
	/*  5 */	1,
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
	/* 19 */	5,
	/* 20 */	NODEV,
	/* 21 */	NODEV,
	/* 22 */	NODEV,
	/* 23 */	NODEV,
	/* 24 */	NODEV,
	/* 25 */	NODEV,
	/* 26 */	NODEV,
	/* 27 */	NODEV,
	/* 28 */	NODEV,
	/* 29 */	NODEV,
	/* 30 */	8,
	/* 31 */	NODEV,
	/* 32 */	NODEV,
	/* 33 */	9,
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
	/* 52 */	13,
	/* 53 */	NODEV,
	/* 54 */	NODEV,
	/* 55 */	NODEV,
	/* 56 */	NODEV,
	/* 57 */	NODEV,
	/* 58 */	18,
};

/*
 * Convert a character device number to a block device number.
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
