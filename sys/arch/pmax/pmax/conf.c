/*	$NetBSD: conf.c,v 1.23 1996/09/07 12:40:38 mycroft Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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

#include "vnd.h"
bdev_decl(sw);
#include "rz.h"
bdev_decl(rz);
#include "tz.h"
bdev_decl(tz);
#include "sd.h"
#include "st.h"
#include "ss.h"
#include "uk.h"
#include "ccd.h"

struct bdevsw	bdevsw[] =
{
	bdev_notdef(),			/* 0: SCSI disk */
	bdev_notdef(),			/* 1: vax ht */
	bdev_disk_init(NVND,vnd),	/* 2: vnode disk driver */
	bdev_notdef(),			/* 3: vax rk*/
	bdev_swap_init(1,sw),		/* 4: swap pseudo-device*/
	bdev_notdef(),			/* 5: vax tm */
	bdev_notdef(),			/* 6: vax ts */
	bdev_notdef(),			/* 7: vax mt */
	bdev_notdef(),			/* 8: vax tu */
	bdev_notdef(),			/* 9: ?? */
	bdev_notdef(),			/* 10: ut */
	bdev_notdef(),			/* 11: 11/730 idc */
	bdev_notdef(),			/* 12: rx */
	bdev_notdef(),			/* 13: uu */
	bdev_notdef(),			/* 14: rl */
	bdev_notdef(),			/* 15: tmscp */
	bdev_notdef(),			/* 16: cs */
	bdev_notdef(),			/* 17: md */
	bdev_tape_init(NST,st),		/* 18: st */
	bdev_disk_init(NSD,sd),		/* 19: sd */
	bdev_tape_init(NTZ, tz),	/* 20: tz */
	bdev_disk_init(NRZ,rz),		/* 21: ?? SCSI disk */ /*XXX*/
	bdev_disk_init(NRZ,rz),		/* 22: ?? old SCSI disk */ /*XXX*/
	bdev_notdef(),			/* 23: mscp */
	bdev_disk_init(NCCD,ccd),	/* 24: concatenated disk driver */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

/*
 * Swapdev is a fake block device implemented  in sw.c and only used 
 * internally to get to swstrategy.  It cannot be provided to the
 * users, because the swstrategy routine munches the b_dev and b_blkno
 * entries before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines.   User access (e.g., for libkvm
 * and ps) is provided through the /dev/drum character (raw) device.
 */
dev_t	swapdev = makedev(4, 0);


cdev_decl(sw);
#define	mmread	mmrw
#define	mmwrite	mmrw
dev_type_read(mmrw);
cdev_decl(mm);
#include "pty.h"
#include "bpfilter.h"
#include "dtop.h"
cdev_decl(dtop);
#include "dc_ioasic.h"
#include "dc_ds.h"
cdev_decl(dc);
#include "scc.h"
cdev_decl(scc);
cdev_decl(rz);
cdev_decl(tz);
#include "rasterconsole.h"
cdev_decl(rcons);
#include "fb.h"
cdev_decl(fb);
#include "pm.h"
cdev_decl(pm);
#include "cfb.h"
cdev_decl(cfb);
#include "xcfb.h"
cdev_decl(xcfb);
#include "mfb.h"
cdev_decl(mfb);
dev_decl(filedesc,open);

#ifdef IPFILTER
#define NIPF 1
#else
#define NIPF 0
#endif

#if (NDC_DS > 0) || (NDC_IOASIC > 0)
# define NDC 1
#else
# define NDC 0
#endif

/* a framebuffer with an attached mouse: */
/* open, close, ioctl, poll, mmap */

/* dev_init(c,n,select) in cdev_fbm_init(c,n) should be dev_init(c,n,poll) */
/* see also dev/fb_userreq.c TTTTT */

#define	cdev_fbm_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, dev_init(c,n,select), \
	dev_init(c,n,mmap) }


struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_swap_init(1,sw),		/* 1: /dev/drum (swap pseudo-device) */
	cdev_ctty_init(1,ctty),		/* 2: controlling terminal */
	cdev_mm_init(1,mm),		/* 3: /dev/{null,mem,kmem,...} */
        cdev_tty_init(NPTY,pts),        /* 4: pseudo-tty slave */
        cdev_ptc_init(NPTY,ptc),        /* 5: pseudo-tty master */
	cdev_log_init(1,log),		/* 6: /dev/klog */
	cdev_fd_init(1,filedesc),	/* 7: file descriptor pseudo-dev */
	cdev_notdef(),			/* 8: old 2100/3100 frame buffer */
	cdev_notdef(),			/* 9: old slot for SCSI disk */
	cdev_tape_init(NTZ,tz),		/* 10: SCSI tape */
	cdev_disk_init(NVND,vnd),	/* 11: vnode disk driver */
	cdev_bpftun_init(NBPFILTER,bpf),/* 12: Berkeley packet filter */
	cdev_notdef(),			/* 13: color frame buffer */
	cdev_notdef(),			/* 14: maxine color frame buffer */
	cdev_tty_init(NDTOP,dtop),	/* 15: desktop bus interface */
	cdev_tty_init(NDC,dc),		/* 16: dc7085 serial interface */
	cdev_tty_init(NSCC,scc),	/* 17: scc 82530 serial interface */
	cdev_notdef(),			/* 18: mono frame buffer */
        cdev_notdef(),		        /* 19: mt */
	cdev_tty_init(NPTY,pts),	/* 20: pty slave  */
        cdev_ptc_init(NPTY,ptc),        /* 21: pty master */
	cdev_notdef(),			/* 22: dmf */
	cdev_notdef(),			/* 23: vax 730 idc */
	cdev_notdef(),			/* 24: dn-11 */

		/* 25-28 CSRG reserved to local sites, DEC sez: */
	cdev_notdef(),		/* 25: gpib */
	cdev_notdef(),		/* 26: lpa */
	cdev_notdef(),		/* 27: psi */
	cdev_notdef(),		/* 28: ib */
	cdev_notdef(),		/* 29: ad */
	cdev_notdef(),		/* 30: rx */
	cdev_notdef(),		/* 31: ik */
	cdev_notdef(),		/* 32: rl-11 */
	cdev_notdef(),		/* 33: dhu/dhv */
	cdev_notdef(),		/* 34: Vax Able dmz, mips dc  */
	cdev_notdef(),		/* 35: qv */
	cdev_notdef(),		/* 36: tmscp */
	cdev_notdef(),		/* 37: vs */
	cdev_notdef(),		/* 38: vax cn console */
	cdev_notdef(),		/* 39: lta */
	cdev_notdef(),		/* 40: crl (Venus, aka 8600 aka 11/790 console RL02) */
	cdev_notdef(),		/* 41: cs */
	cdev_notdef(),		/* 42: qd, Qdss, vcb02 */
	cdev_mm_init(1,mm),	/* 43: errlog (VMS-lookalike puke) */
	cdev_notdef(),		/* 44: dmb */
	cdev_notdef(),		/* 45:  vax ss, mips scc */
	cdev_tape_init(NST,st),	/* 46: st */
	cdev_disk_init(NSD,sd),	/* 47: sd */
	cdev_notdef(),		/* 48: Ultrix /dev/trace */
	cdev_notdef(),		/* 49: sm (sysV shm?) */
	cdev_notdef(),		/* 50 sg */
	cdev_notdef(),		/* 51: sh tty */
	cdev_notdef(),		/* 52: its */
	cdev_notdef(),		/* 53: nodev */
	cdev_notdef(),		/* 54: nodev */
	cdev_tape_init(NTZ,tz),	/* 55: ultrix-compatible scsi tape (tz) */
	cdev_disk_init(NRZ,rz), /* 56: rz scsi, Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 57: nodev */
	cdev_notdef(),		/* 58: fc */
	cdev_notdef(),		/* 59: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 60: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 61: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 62: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 63: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 64: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 65: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 66: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 67: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 68: ld */
	cdev_notdef(),		/* 69: /dev/audit */
	cdev_notdef(),		/* 70: Mogul (nee' CMU) packetfilter */
	cdev_notdef(),		/* 71: xcons, mips Ultrix /dev/xcons virtual console nightmare */
	cdev_notdef(),		/* 72: xa */
	cdev_notdef(),		/* 73: utx */
	cdev_notdef(),		/* 74: sp */
	cdev_notdef(),		/* 75: pr Ultrix PrestoServe NVRAM pseudo-device control device */
	cdev_notdef(),		/* 76: ultrix disk shadowing */
	cdev_notdef(),		/* 77: ek */
	cdev_notdef(),		/* 78: msdup ? */
	cdev_notdef(),		/* 79: so-called multimedia audio A */
	cdev_notdef(),		/* 80: so-called multimedia audio B */
	cdev_notdef(),		/* 81: so-called multimedia video in */
	cdev_notdef(),		/* 82: so-called multimedia video out */
	cdev_notdef(),		/* 83: fd */
	cdev_notdef(),		/* 84: DTi */
	cdev_tty_init(NRASTERCONSOLE,rcons), /* 85: rcons pseudo-dev */
	cdev_fbm_init(NFB,fb),	/* 86: frame buffer pseudo-device */
	cdev_disk_init(NCCD,ccd),	/* 87: concatenated disk driver */
	cdev_random_init(1,random),     /* 88: random data source */
	cdev_uk_init(NUK,uk),           /* 89: unknown SCSI */
	cdev_ss_init(NSS,ss),           /* 90: SCSI scanner */
	cdev_gen_ipf(NIPF,ipl),		/* 91: ip filtering */
};
int	nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

int	mem_no = 2; 	/* major device number of memory special file */

/*
 * Routine that identifies /dev/mem and /dev/kmem.
 */
int
iskmemdev(dev)
	dev_t dev;
{

#ifdef COMPAT_BSD44
	return (major(dev) == 2 && minor(dev) < 2);
#else
	return (major(dev) == 3 && minor(dev) < 2);
#endif
}

/*
 * Returns true if dev is /dev/zero.
 */
int
iszerodev(dev)
	dev_t dev;
{

#ifdef COMPAT_BSD44
	return (major(dev) == 2 && minor(dev) == 12);
#else
	return (major(dev) == 3 && minor(dev) == 12);
#endif
}

static int chrtoblktbl[] =  {
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
	/* 11 */	2,
	/* 12 */	NODEV,
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
	/* 47 */	19,		/* sd */
	/* 48 */	NODEV,
	/* 49 */	NODEV,
	/* 50 */	NODEV,
	/* 51 */	NODEV,
	/* 52 */	NODEV,
	/* 53 */	NODEV,
	/* 54 */	NODEV,
	/* 55 */	NODEV,
	/* 56 */	21,		/* 19 */ /* XXX rz, remapped to sd */
	/* 57 */	NODEV,
	/* 58 */	NODEV,
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
	/* 87 */	24,
};

/*
 * Routine to convert from character to block device number.
 */
dev_t
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
