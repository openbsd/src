/*	$OpenBSD: conf.c,v 1.25 2003/02/08 12:34:27 todd Exp $	*/

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
 *     @(#)conf.c	7.9 (Berkeley) 5/28/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include <machine/conf.h>

#include "ccd.h"
#include "vnd.h"
#include "rd.h"
#include "sd.h"
#include "st.h"
#include "cd.h"
#include "ch.h"
#include "ss.h"
#include "ses.h"
#include "uk.h"
#if 0
#include "fd.h"
#else
#define NFD 0
#endif
bdev_decl(fd);
cdev_decl(fd);

struct bdevsw   bdevsw[] =
{
	bdev_swap_init(1,sw),		/*  0: swap pseudo-device */
	bdev_disk_init(NCCD,ccd),	/*  1: concatenated disk driver */
	bdev_disk_init(NVND,vnd),	/*  2: vnode disk driver */
	bdev_disk_init(NRD,rd),		/*  3: RAM disk */
	bdev_disk_init(NSD,sd),		/*  4: SCSI disk */
	bdev_tape_init(NST,st),		/*  5: SCSI tape */
	bdev_disk_init(NCD,cd),		/*  6: SCSI CD-ROM */
	bdev_disk_init(NFD,fd),		/*  7: floppy drive */
					/*  8: */
	bdev_lkm_dummy(),
	bdev_lkm_dummy(),
	bdev_lkm_dummy(),
	bdev_lkm_dummy(),
	bdev_lkm_dummy(),
	bdev_lkm_dummy(),
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

/* open, close, read, write, ioctl, tty, mmap */
#define cdev_wscons_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_init(c,n,stop), \
	dev_init(c,n,tty), ttselect /* ttpoll */, dev_init(c,n,mmap) }

#include "audio.h"
#include "pty.h"
#include "wsdisplay.h"
#include "wskbd.h"
#include "wsmouse.h"
#include "wsmux.h"
#ifdef XFS
#include <xfs/nxfs.h>
cdev_decl(xfs_dev);
#endif

#include "inet.h"
#include "bpfilter.h"
#include "tun.h"

#include "ksyms.h"

#include "lpt.h"
cdev_decl(lpt);

#include "com.h"
cdev_decl(com);

#include "pf.h"

#include "systrace.h"

struct cdevsw   cdevsw[] =
{
	cdev_cn_init(1,cn),		/*  0: virtual console */
	cdev_ctty_init(1,ctty),		/*  1: controlling terminal */
	cdev_mm_init(1,mm),		/*  2: /dev/{null,mem,kmem,...} */
	cdev_swap_init(1,sw),		/*  3: /dev/drum (swap pseudo-device) */
	cdev_tty_init(NPTY,pts),	/*  4: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/*  5: pseudo-tty master */
	cdev_log_init(1,log),		/*  6: /dev/klog */
	cdev_disk_init(NCCD,ccd),	/*  7: concatenated disk */
	cdev_disk_init(NVND,vnd),	/*  8: vnode disk driver */
	cdev_disk_init(NRD,rd),		/*  9: RAM disk */
	cdev_disk_init(NSD,sd),		/* 10: SCSI disk */
	cdev_tape_init(NST,st),		/* 11: SCSI tape */
	cdev_disk_init(NCD,cd),		/* 12: SCSI cd-rom */
	cdev_ch_init(NCH,ch),		/* 13: SCSI changer */
	cdev_ss_init(NSS,ss),		/* 14: SCSI scanner */
	cdev_uk_init(NUK,uk),		/* 15: SCSI unknown */
	cdev_fd_init(1,filedesc),	/* 16: file descriptor pseudo-device */
	cdev_bpftun_init(NBPFILTER,bpf),/* 17: Berkeley packet filter */
	cdev_bpftun_init(NTUN,tun),	/* 18: network tunnel */
	cdev_lkm_init(NLKM,lkm),	/* 19: loadable module driver */
	cdev_random_init(1,random),	/* 20: random generator */
	cdev_pf_init(NPF,pf),		/* 21: packet filter */
	cdev_tty_init(1,pdc),		/* 22: PDC device */
	cdev_tty_init(NCOM,com),	/* 23: RS232 */
	cdev_disk_init(NFD,fd),		/* 24: floppy drive */
	cdev_ksyms_init(NKSYMS,ksyms),	/* 25: Kernel symbols device */
	cdev_lpt_init(NLPT,lpt),	/* 26: parallel printer */
	cdev_wsdisplay_init(NWSDISPLAY,	/* 27: workstation console */
	    wsdisplay),
	cdev_mouse_init(NWSKBD,wskbd),	/* 28: keyboards */
	cdev_mouse_init(NWSMOUSE,wsmouse), /* 29: mice */
	cdev_mouse_init(NWSMUX,wsmux),	/* 30: mux */
	cdev_notdef(),			/* 31 */

#ifdef XFS
	cdev_xfs_init(NXFS,xfs_dev),	/* 32: xfs communication device */
#else
	cdev_notdef(),			/* 32 */
#endif
	cdev_notdef(),			/* 33: ALTQ (deprecated) */
	cdev_systrace_init(NSYSTRACE,systrace),	/* 34: system call tracing */
	cdev_audio_init(NAUDIO,audio),	/* 35: /dev/audio */
	cdev_crypto_init(NCRYPTO,crypto), /* 36: /dev/crypto */
	cdev_ses_init(NSES,ses),	/* 37: SCSI SES/SAF-TE */
	cdev_lkm_dummy(),
	cdev_lkm_dummy(),
	cdev_lkm_dummy(),
	cdev_lkm_dummy(),
	cdev_lkm_dummy(),
	cdev_lkm_dummy(),
};
int nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

int mem_no = 2;		/* major device number of memory special file */

/*
 * Swapdev is a fake device implemented
 * in sw.c used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */
dev_t   swapdev = makedev(1, 0);

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
	/*  7 */	1,
	/*  8 */	2,
	/*  9 */	3,
	/* 10 */	4,
	/* 11 */	5,
	/* 12 */	6,
	/* 13 */	NODEV,
	/* 14 */	NODEV,
	/* 15 */	NODEV,
	/* 16 */	NODEV,
	/* 17 */	NODEV,
	/* 18 */	NODEV,
	/* 19 */	NODEV,
	/* 20 */	NODEV,
	/* 21 */	NODEV,
	/* 22 */	NODEV,
	/* 23 */	NODEV,
	/* 24 */	NODEV,
	/* 25 */	7,
	/* 25 */	8,
};

/*
 * Convert a character device number to a block device number.
 */
dev_t
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

/*
 * Convert a block device number to a character device number.
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

/*
 * Returns true if dev is /dev/mem or /dev/kmem.
 */
int
iskmemdev(dev)
	dev_t dev;
{
	return (major(dev) == mem_no && minor(dev) < 2);
}

#include <dev/cons.h>

cons_decl(pdc);
cons_decl(ws);
cons_decl(com);

struct  consdev constab[] = {
	cons_init(pdc),		/* XXX you'd better leave it here for pdc.c */
#if NWSDISPLAY > 0
	cons_init(ws),
#endif
#if NCOM > 0
	cons_init(com),
#endif
	{ 0 }
};

