/*	$OpenBSD: conf.c,v 1.20 1998/04/26 22:30:57 niklas Exp $	*/
/*	$NetBSD: conf.c,v 1.42 1997/01/07 11:35:03 mrg Exp $	*/

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

#include <machine/conf.h>

#ifdef BANKEDDEVPAGER
#include <sys/bankeddev.h>
#endif

#include "vnd.h"
#include "sd.h"
#include "cd.h"
#include "st.h"
#include "fd.h"
#include "ccd.h"
#include "ss.h"
#include "wd.h"
#include "acd.h"
#include "rd.h"
#include "ch.h"

struct bdevsw	bdevsw[] =
{
	bdev_disk_init(NWD,wd),	/* 0: ST506/ESDI/IDE disk */
	bdev_notdef(),			/* 1 */
	bdev_disk_init(NFD,fd),		/* 2: floppy disk */
	bdev_swap_init(1,sw),		/* 3: swap pseudo-device */
	bdev_disk_init(NSD,sd),		/* 4: SCSI disk */
	bdev_tape_init(NST,st),		/* 5: SCSI tape */
	bdev_disk_init(NVND,vnd),	/* 6: vnode disk driver */
	bdev_disk_init(NCD,cd),		/* 7: SCSI CD-ROM */
	bdev_disk_init(NCCD,ccd),	/* 8: concatenated disk driver */
	bdev_lkm_dummy(),		/* 9 */
	bdev_lkm_dummy(),		/* 10 */
	bdev_lkm_dummy(),		/* 11 */
	bdev_lkm_dummy(),		/* 12 */
	bdev_lkm_dummy(),		/* 13 */
	bdev_lkm_dummy(),		/* 14 */
	bdev_disk_init(NACD,acd),	/* 15: ATAPI CD-ROM */
	bdev_disk_init(NRD,rd),		/* 16: ram disk driver */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

#include "pty.h"
#include "grf.h"
#include "par.h"
#include "ser.h"
#include "msc.h"
#include "ite.h"
#include "kbd.h"
#include "ms.h"
#include "view.h"
#include "mfcs.h"
dev_decl(filedesc,open);
#include "bpfilter.h"
#include "tun.h"
#include "com.h"
#include "lpt.h"
#include "uk.h"
#include "audio.h"
cdev_decl(audio);

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_ctty_init(1,ctty),		/* 1: controlling terminal */
	cdev_mm_init(1,mm),		/* 2: /dev/{null,mem,kmem,...} */
	cdev_swap_init(1,sw),		/* 3: /dev/drum (swap pseudo-device) */
	cdev_tty_init(NPTY,pts),	/* 4: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 5: pseudo-tty master */
	cdev_log_init(1,log),		/* 6: /dev/klog */
	cdev_disk_init(NCCD,ccd),	/* 7: concatenated disk driver */
	cdev_disk_init(NSD,sd),		/* 8: SCSI disk */
	cdev_disk_init(NCD,cd),		/* 9: SCSI CD-ROM */
	cdev_grf_init(NGRF,grf),	/* 10: frame buffer */
	cdev_par_init(NPAR,par),	/* 11: parallel interface */
	cdev_tty_init(NSER,ser),	/* 12: built-in single-port serial */
	cdev_tty_init(NITE,ite),	/* 13: console terminal emulator */
	cdev_mouse_init(NKBD,kbd),	/* 14: /dev/kbd */
	cdev_mouse_init(NMS,ms),	/* 15: /dev/mouse0 /dev/mouse1 */
	cdev_view_init(NVIEW,view),	/* 16: /dev/view00 /dev/view01 ... */
	cdev_tty_init(NMFCS,mfcs),	/* 17: MultiFaceCard III serial */
	cdev_disk_init(NFD,fd),		/* 18: floppy disk */
	cdev_disk_init(NVND,vnd),	/* 19: vnode disk driver */
	cdev_tape_init(NST,st),		/* 20: SCSI tape */
	cdev_fd_init(1,filedesc),	/* 21: file descriptor pseudo-dev */
	cdev_bpftun_init(NBPFILTER,bpf),/* 22: Berkeley packet filter */
	cdev_bpftun_init(NTUN,tun),	/* 23: network tunnel */
	cdev_lkm_init(NLKM,lkm),	/* 24: loadable module driver */
	cdev_scanner_init(NSS,ss),	/* 25: SCSI scanner */
	cdev_lkm_dummy(),		/* 26 */
	cdev_lkm_dummy(),		/* 27 */
	cdev_lkm_dummy(),		/* 28 */
	cdev_lkm_dummy(),		/* 29 */
	cdev_lkm_dummy(),		/* 30 */
 	cdev_tty_init(NMSC,msc),	/* 31: A2232 MSC Multiport serial */
	cdev_tty_init(NCOM,com),	/* 32: ISA serial port */
	cdev_lpt_init(NLPT,lpt),	/* 33: ISA parallel printer */
	cdev_gen_ipf(NIPF,ipl),		/* 34: IP filter log */
	cdev_random_init(1,random),	/* 35: random data source */
	cdev_uk_init(NUK,uk),		/* 36: unknown SCSI */
	cdev_disk_init(NWD,wd),		/* 37: ST506/ESDI/IDE disk */
	cdev_disk_init(NACD,acd),	/* 38: ATAPI CD-ROM */
	cdev_audio_init(NAUDIO,audio),	/* 39: cc audio interface */
	cdev_ch_init(NCH,ch),		/* 40: SCSI autochanger */
	cdev_disk_init(NRD,rd),		/* 41: RAM disk */
};
int	nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

#ifdef BANKEDDEVPAGER
extern int grfbanked_get __P((int, int, int));
extern int grfbanked_set __P((int, int));
extern int grfbanked_cur __P((int));

struct bankeddevsw bankeddevsw[sizeof (cdevsw) / sizeof (cdevsw[0])] = {
  { 0, 0, 0 },						/* 0 */
  { 0, 0, 0 },						/* 1 */
  { 0, 0, 0 },						/* 2 */
  { 0, 0, 0 },						/* 3 */
  { 0, 0, 0 },						/* 4 */
  { 0, 0, 0 },						/* 5 */
  { 0, 0, 0 },						/* 6 */
  { 0, 0, 0 },						/* 7 */
  { 0, 0, 0 },						/* 8 */
  { 0, 0, 0 },						/* 9 */
  { grfbanked_get, grfbanked_cur, grfbanked_set },	/* 10 */
  /* rest { 0, 0, 0 } */
};
#endif

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

static int chrtoblktab[] = {
	/* XXXX This needs to be dynamic for LKMs. */
	/*VCHR*/	/*VBLK*/
	/*  0 */	NODEV,
	/*  1 */	NODEV,
	/*  2 */	NODEV,
	/*  3 */	NODEV,
	/*  4 */	NODEV,
	/*  5 */	NODEV,
	/*  6 */	NODEV,
	/*  7 */	8,		/* ccd */
	/*  8 */	4,		/* sd */
	/*  9 */	7,		/* cd */
	/* 10 */	NODEV,
	/* 11 */	NODEV,
	/* 12 */	NODEV,
	/* 13 */	NODEV,
	/* 14 */	NODEV,
	/* 15 */	NODEV,
	/* 16 */	NODEV,
	/* 17 */	NODEV,
	/* 18 */	2,		/* fd */
	/* 19 */	6,		/* vnd */
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
	/* 37 */	0,		/* wd */
	/* 38 */	15,		/* acd */
	/* 39 */	NODEV,
	/* 40 */	NODEV,
	/* 41 */	16,		/* rd */
};

/*
 * Convert a character device number to a block device number.
 */
int
chrtoblk(dev)
	dev_t dev;
{
	int blkmaj;

	if (major(dev) >= nchrdev)
		return(NODEV);
	blkmaj = chrtoblktab[major(dev)];
	if (blkmaj == NODEV)
		return(NODEV);
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
	for (i = 0; i < sizeof(chrtoblktab)/sizeof(chrtoblktab[0]); i++)
		if (blkmaj == chrtoblktab[i])
			return (makedev(i, minor(dev)));
	return (NODEV);
}

/*
 * This entire table could be autoconfig()ed but that would mean that
 * the kernel's idea of the console would be out of sync with that of
 * the standalone boot.  I think it best that they both use the same
 * known algorithm unless we see a pressing need otherwise.
 */
cons_decl(com);
cons_decl(ite);
cons_decl(ser);

struct	consdev constab[] = {
#if NSER > 0
	cons_init(ser),
#endif
#if NITE > 0
	cons_init(ite),
#endif
#ifdef notyet
#if NCOM > 0
	cons_init(com),
#endif
#endif
	{ 0 },
};
