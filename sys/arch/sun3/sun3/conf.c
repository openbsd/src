/*	$NetBSD: conf.c,v 1.44.2.1 1995/10/29 04:22:44 gwr Exp $	*/

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

int	ttselect	__P((dev_t, int, struct proc *));

#include "cd.h"
bdev_decl(cd);
cdev_decl(cd);

#include "ccd.h"
bdev_decl(ccd);
cdev_decl(ccd);

#include "rd.h"
bdev_decl(rd);
/* no cdev for rd */

#include "sd.h"
bdev_decl(sd);
cdev_decl(sd);

#include "st.h"
bdev_decl(st);
cdev_decl(st);

/* swap device (required) */
bdev_decl(sw);
cdev_decl(sw);

#include "vnd.h"
bdev_decl(vnd);
cdev_decl(vnd);

#include "xd.h"
bdev_decl(xd);
cdev_decl(xd);

#define	NXT 0	/* XXX */
bdev_decl(xt);
cdev_decl(xt);

#include "xy.h"
bdev_decl(xy);
cdev_decl(xy);

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
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

/*
 * Devices that have only CHR nodes are declared below.
 */

cdev_decl(cn);
cdev_decl(ctty);
#define	mmread	mmrw
#define	mmwrite	mmrw
cdev_decl(mm);

#include "zs.h"
cdev_decl(zs);
cdev_decl(kd);
cdev_decl(ms);
cdev_decl(kbd);

/* XXX - Should make keyboard/mouse real children of zs. */
#if NZS > 1
#define NKD 1
#else
#define NKD 0
#endif

#include "pty.h"
#define	ptstty		ptytty
#define	ptsioctl	ptyioctl
cdev_decl(pts);
#define	ptctty		ptytty
#define	ptcioctl	ptyioctl
cdev_decl(ptc);

/* frame-buffer devices */
cdev_decl(fb);
#include "bwtwo.h"
cdev_decl(bw2);
#include "cgtwo.h"
cdev_decl(cg2);
#include "cgfour.h"
cdev_decl(cg4);

cdev_decl(log);
cdev_decl(fd);

#include "bpfilter.h"
cdev_decl(bpf);

#include "tun.h"
cdev_decl(tun);


struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_tty_init(NKD,kd),	/* 1: Sun keyboard/display */
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
	cdev_tty_init(NZS,zs),		/* 12: Zilog 8350 serial port */
	cdev_mouse_init(NKD,ms),	/* 13: Sun mouse */
	cdev_notdef(),			/* 14: cgone */
	cdev_notdef(),			/* 15: /dev/winXXX */
	cdev_log_init(1,log),		/* 16: /dev/klog */
	cdev_disk_init(NSD,sd),		/* 17: SCSI disk */
	cdev_tape_init(NST,st),		/* 18: SCSI tape */
	cdev_disk_init(NVND,vnd),	/* 19: vnode disk driver */
	cdev_tty_init(NPTY,pts),	/* 20: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 21: pseudo-tty master */
	cdev_fb_init(1,fb),		/* 22: /dev/fb indirect driver */
	cdev_fd_init(1,fd),		/* 23: file descriptor pseudo-device */
	cdev_bpftun_init(NTUN,tun),	/* 24: network tunnel */
	cdev_notdef(),			/* 25: sun pi? */
	cdev_notdef(),			/* 26: bwone */
	cdev_fb_init(NBWTWO,bw2),	/* 27: bwtwo */
	cdev_notdef(),			/* 28: Systech VPC-2200 versatec/centronics */
	cdev_mouse_init(NKD,kbd),	/* 29: Sun keyboard */
	cdev_tape_init(NXT,xt),		/* 30: Xylogics tape */
	cdev_fb_init(NCGTWO,cg2),	/* 31: cgtwo */
	cdev_notdef(),			/* 32: /dev/gpone */
	cdev_disk_init(NCCD,ccd),	/* 33: concatenated disk driver */
	cdev_notdef(),			/* 34: floating point accelerator */
	cdev_notdef(),			/* 35 */
	cdev_bpftun_init(NBPFILTER,bpf),/* 36: Berkeley packet filter */
	cdev_notdef(),			/* 37 */
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
	cdev_notdef(),			/* 51: (chut) */
	cdev_notdef(),			/* 52: RAM disk - for install tape */
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
	/* 42 */	NODEV,
	/* 43 */	10,
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
};

/*
 * Convert a character device number to a block device number.
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

/*
 * This entire table could be autoconfig()ed but that would mean that
 * the kernel's idea of the console could be out of sync with that of
 * the standalone boot.  I think it best that they both use the same
 * known algorithm unless we see a pressing need otherwise.
 */
#include <dev/cons.h>

cons_decl(kd);
#define	zscnpollc	nullcnpollc

cons_decl(zs);
dev_type_cnprobe(zscnprobe_a);
dev_type_cnprobe(zscnprobe_b);

struct	consdev constab[] = {
#if NZS > 0
	{ zscnprobe_a, zscninit, zscngetc, zscnputc, zscnpollc },
	{ zscnprobe_b, zscninit, zscngetc, zscnputc, zscnpollc },
#endif
#if NKD > 0
	cons_init(kd),
#endif
	{ 0 },	/* REQIURED! */
};
