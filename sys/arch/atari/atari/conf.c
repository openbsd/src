/*	$NetBSD: conf.c,v 1.11 1995/11/30 00:57:33 jtc Exp $	*/

/*
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

#ifdef BANKEDDEVPAGER
#include <sys/bankeddev.h>
#endif

int	ttselect	__P((dev_t, int, struct proc *));

#ifndef LKM
#define	lkmenodev	enodev
#else
int	lkmenodev();
#endif

#define	bdev_rd_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,strategy), \
	dev_init(c,n,ioctl), (dev_type_dump((*))) enxio, dev_size_init(c,n), 0 }

#include "vnd.h"
bdev_decl(vnd);
#include "ramd.h"
bdev_decl(rd);
#include "fd.h"
#define	fdopen	Fdopen	/* conflicts with fdopen() in kern_descrip.c */
bdev_decl(fd);
#undef	fdopen
bdev_decl(sw);
#include "sd.h"
bdev_decl(sd);
#include "st.h"
bdev_decl(st);
#include "cd.h"
bdev_decl(cd);
#include "ccd.h"
bdev_decl(ccd);

struct bdevsw	bdevsw[] =
{
	bdev_disk_init(NVND,vnd),	/* 0: vnode disk driver */
	bdev_rd_init(NRAMD,rd),		/* 1: ram disk - for install disk */
#define	fdopen	Fdopen	/* conflicts with fdopen() in kern_descrip.c */
	bdev_disk_init(NFD,fd),		/* 2: floppy disk */
#undef	fdopen
	bdev_swap_init(1,sw),		/* 3: swap pseudo-device */
	bdev_disk_init(NSD,sd),		/* 4: SCSI disk */
	bdev_tape_init(NST,st),		/* 5: SCSI tape */
	bdev_disk_init(NCD,cd),		/* 6: SCSI CD-ROM */
	bdev_lkm_dummy(),		/* 7 */
	bdev_lkm_dummy(),		/* 8 */
	bdev_lkm_dummy(),		/* 9 */
	bdev_lkm_dummy(),		/* 10 */
	bdev_lkm_dummy(),		/* 11 */
	bdev_lkm_dummy(),		/* 12 */
	bdev_disk_init(NCCD,ccd),	/* 13: concatenated disk driver */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

/* open, close, ioctl, select, mmap -- XXX should be a map device */
#define	cdev_grf_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) nullop, \
	(dev_type_write((*))) nullop, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, (dev_type_tty((*))) nullop, \
	dev_init(c,n,select), dev_init(c,n,mmap) }

/* open, close, ioctl, select, mmap -- XXX should be a map device */
#define	cdev_view_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) nullop, \
	(dev_type_write((*))) nullop, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, (dev_type_tty((*))) nullop, \
	dev_init(c,n,select), dev_init(c,n,mmap) }

cdev_decl(cn);
cdev_decl(ctty);
#define	mmread	mmrw
#define	mmwrite	mmrw
cdev_decl(mm);
cdev_decl(sw);
#include "pty.h"
#define	ptstty		ptytty
#define	ptsioctl	ptyioctl
cdev_decl(pts);
#define	ptctty		ptytty
#define	ptcioctl	ptyioctl
cdev_decl(ptc);
cdev_decl(log);
#include "zs.h"
cdev_decl(zs);
cdev_decl(sd);
cdev_decl(cd);
cdev_decl(st);
#include "grf.h"
cdev_decl(grf);
#include "ite.h"
cdev_decl(ite);
#include "view.h"
cdev_decl(view);
#include "kbd.h"
cdev_decl(kbd);
#include "mouse.h"
cdev_decl(ms);
#define	fdopen	Fdopen	/* conflicts with fdopen() in kern_descrip.c */
cdev_decl(fd);
#undef	fdopen
cdev_decl(vnd);
cdev_decl(ccd);
dev_decl(fd,open);
#include "bpfilter.h"
cdev_decl(bpf);
#include "tun.h"
cdev_decl(tun);
#ifdef LKM
#define NLKM 1
#else
#define NLKM 0
#endif
cdev_decl(lkm);

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_ctty_init(1,ctty),		/* 1: controlling terminal */
	cdev_mm_init(1,mm),		/* 2: /dev/{null,mem,kmem,...} */
	cdev_swap_init(1,sw),		/* 3: /dev/drum (swap pseudo-device) */
	cdev_tty_init(NPTY,pts),	/* 4: pseudo-tty slave	*/
	cdev_ptc_init(NPTY,ptc),	/* 5: pseudo-tty master */
	cdev_log_init(1,log),		/* 6: /dev/klog */
	cdev_tty_init(NZS,zs),		/* 7: 8530 SCC */
	cdev_disk_init(NSD,sd),		/* 8: SCSI disk */
	cdev_disk_init(NCD,cd),		/* 9: SCSI CD-ROM */
	cdev_tape_init(NST,st),		/* 10: SCSI tape */
	cdev_grf_init(NGRF,grf),	/* 11: frame buffer */
	cdev_tty_init(NITE,ite),	/* 12: console terminal emulator */
	cdev_view_init(NVIEW,view),	/* 13: /dev/view00 /dev/view01, ... */
	cdev_mouse_init(NKBD,kbd),	/* 14: /dev/kbd	*/
	cdev_mouse_init(NMOUSE,ms),	/* 15: /dev/mouse0 /dev/mouse1 */
#define	fdopen	Fdopen	/* conflicts with fdopen() in kern_descrip.c */
	cdev_disk_init(NFD,fd),		/* 16: floppy disk */
#undef	fdopen
	cdev_disk_init(NVND,vnd),	/* 17: vnode disk driver */
	cdev_fd_init(1,fd),		/* 18: file descriptor pseudo-device */
	cdev_bpftun_init(NBPFILTER,bpf),/* 19: Berkeley packet filter */
	cdev_lkm_init(NLKM,lkm),	/* 20: loadable module driver */
	cdev_lkm_dummy(),		/* 21 */
	cdev_lkm_dummy(),		/* 22 */
	cdev_lkm_dummy(),		/* 23 */
	cdev_lkm_dummy(),		/* 24 */
	cdev_lkm_dummy(),		/* 25 */
	cdev_lkm_dummy(),		/* 26 */
	cdev_disk_init(NCCD,ccd),	/* 27: concatenated disk driver */
	cdev_bpftun_init(NTUN,tun),	/* 28: network tunnel */
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
iskmemdev(dev)
	dev_t dev;
{

	return (major(dev) == mem_no && minor(dev) < 2);
}

/*
 * Returns true if dev is /dev/zero.
 */
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
	/*  7 */	NODEV,
	/*  8 */	4,
	/*  9 */	6,
	/* 10 */	5,
	/* 11 */	NODEV,
	/* 12 */	NODEV,
	/* 13 */	NODEV,
	/* 14 */	NODEV,
	/* 15 */	NODEV,
	/* 16 */	2,
	/* 17 */	0,
	/* 18 */	NODEV,
	/* 19 */	NODEV,
	/* 20 */	NODEV,
	/* 21 */	NODEV,
	/* 22 */	NODEV,
	/* 23 */	NODEV,
	/* 24 */	NODEV,
	/* 25 */	NODEV,
	/* 26 */	NODEV,
	/* 27 */	13,
	/* 28 */	NODEV,
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
		return(NODEV);
	blkmaj = chrtoblktab[major(dev)];
	if (blkmaj == NODEV)
		return(NODEV);
	return (makedev(blkmaj, minor(dev)));
}

/*
 * This entire table could be autoconfig()ed but that would mean that
 * the kernel's idea of the console would be out of sync with that of
 * the standalone boot.  I think it best that they both use the same
 * known algorithm unless we see a pressing need otherwise.
 */
cons_decl(ser);
#define	itecnpollc	nullcnpollc
cons_decl(ite);

struct	consdev constab[] = {
#if NSER > 0
	cons_init(ser),
#endif
#if NITE > 0
	cons_init(ite),
#endif
	{ 0 },
};
