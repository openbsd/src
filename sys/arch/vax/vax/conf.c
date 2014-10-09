/*	$OpenBSD: conf.c,v 1.70 2014/10/09 03:59:59 tedu Exp $ */
/*	$NetBSD: conf.c,v 1.44 1999/10/27 16:38:54 ragge Exp $	*/

/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 *
 *	@(#)conf.c	7.18 (Berkeley) 5/9/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/vnode.h>

#include "mt.h"
bdev_decl(mt);

#include "rd.h"

#include "ra.h"
bdev_decl(ra);
bdev_decl(rx);

#include "vnd.h"

#include "hdc.h"
bdev_decl(hd);
bdev_decl(ry);

#include "sd.h"
#include "st.h"
#include "cd.h"

#include "ksyms.h"

struct bdevsw	bdevsw[] =
{
	bdev_notdef(),			/* 0 */
	bdev_notdef(),			/* 1 */
	bdev_notdef(),			/* 2 */
	bdev_notdef(),			/* 3 */
	bdev_swap_init(1,sw),		/* 4: swap pseudo-device */
	bdev_notdef(),			/* 5 */
	bdev_notdef(),			/* 6 */
	bdev_notdef(),			/* 7 */
	bdev_notdef(),			/* 8 */
	bdev_disk_init(NRA,ra),		/* 9: MSCP disk */
	bdev_notdef(),			/* 10 */
	bdev_notdef(),			/* 11 */
	bdev_disk_init(NRX,rx),		/* 12: RX?? on MSCP */
	bdev_notdef(),			/* 13 */
	bdev_notdef(),			/* 14 */
	bdev_tape_init(NMT,mt),		/* 15: MSCP tape */
	bdev_notdef(),			/* 16: was: KDB50/RA?? */
	bdev_notdef(),			/* 17: was: concatenated disk driver */
	bdev_disk_init(NVND,vnd),	/* 18: vnode disk driver */
	bdev_disk_init(NHD,hd),		/* 19: VS3100 ST506 disk */
	bdev_disk_init(NSD,sd),		/* 20: SCSI disk */
	bdev_tape_init(NST,st),		/* 21: SCSI tape */
	bdev_disk_init(NCD,cd),		/* 22: SCSI CD-ROM */
	bdev_disk_init(NRD,rd),		/* 23: ram disk driver */
	bdev_disk_init(NRY,ry),		/* 24: VS3100 floppy */
	bdev_notdef(),			/* 25 was: RAIDframe disk driver */
};
int	nblkdev = nitems(bdevsw);

/*
 * Console routines for VAX console.
 */
#include <dev/cons.h>

cons_decl(dz);
cons_decl(gen);
cons_decl(qsc);
cons_decl(ws);

#include "dz.h"
#include "qsc.h"
#include "wsdisplay.h"
#include "wskbd.h"

struct	consdev constab[]={
#if VAX650 || VAX630 || VAX660 || VAX670 || VAX680
#define NGEN	1
	cons_init(gen), /* Generic console type; mtpr/mfpr */
#else
#define NGEN	0
#endif
#if VAX410 || VAX43 || VAX46 || VAX48 || VAX49 || VAX53 || VAX60
#if NDZ > 0
	cons_init(dz),	/* DZ11-like serial console on VAXstations */
#endif
#endif
#ifdef VXT
#if NQSC > 0
	cons_init(qsc),	/* SC26C94 serial console on VXT2000 */
#endif
#endif
#if NWSDISPLAY > 0 || NWSKBD > 0
	cons_init(ws),	/* any other frame buffer console */
#endif

#ifdef notyet
/* We may not always use builtin console, sometimes RD */
	{ hdcnprobe, hdcninit, hdcngetc, hdcnputc },
#endif
	{ 0 }
};

#define mmread	mmrw
#define mmwrite mmrw
cdev_decl(mm);
#include "bio.h"
#include "pty.h"

cdev_decl(mt);
cdev_decl(ra);
cdev_decl(gencn);
cdev_decl(rx);
cdev_decl(hd);
cdev_decl(ry);

cdev_decl(qsc);

cdev_decl(dz);

#include "dhu.h"
cdev_decl(dhu);

#include "dl.h"
cdev_decl(dl);

#include "bpfilter.h"

#include "tun.h" 
#include "ch.h"
#include "uk.h"

#include "wsdisplay.h"
#include "wskbd.h"
#include "wsmouse.h"

#include "pf.h"

#include "systrace.h"

#include "vscsi.h"
#include "pppx.h"
#include "audio.h"
#include "fuse.h"

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_tty_init(NDZ,dz),		/* 1: DZ11 */
	cdev_ctty_init(1,ctty),		/* 2: controlling terminal */
	cdev_mm_init(1,mm),		/* 3: /dev/{null,mem,kmem,...} */
	cdev_notdef(),			/* 4 */
	cdev_notdef(),			/* 5 */
	cdev_notdef(),			/* 6 */
	cdev_notdef(),			/* 7 was /dev/drum */
	cdev_notdef(),			/* 8 */
	cdev_disk_init(NRA,ra),		/* 9: MSCP disk interface */
	cdev_notdef(),			/* 10 */
	cdev_notdef(),			/* 11 */
	cdev_notdef(),			/* 12 */
	cdev_notdef(),			/* 13 */
	cdev_notdef(),			/* 14 */
	cdev_notdef(),			/* 15 */
	cdev_notdef(),			/* 16 */
	cdev_notdef(),			/* 17 */
	cdev_notdef(),			/* 18 */
	cdev_notdef(),			/* 19 */
	cdev_tty_init(NPTY,pts),	/* 20: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 21: pseudo-tty master */
	cdev_notdef(),			/* 22 */
	cdev_notdef(),			/* 23 */
	cdev_notdef(),			/* 24 */
	cdev_tty_init(NGEN,gencn),	/* 25: Generic console (mtpr...) */
	cdev_notdef(),			/* 26 */
	cdev_notdef(),			/* 27 */
	cdev_notdef(),			/* 28 was LKM */
	cdev_notdef(),			/* 29 */
	cdev_disk_init(NRX,rx),		/* 30: RX?? on MSCP */
	cdev_notdef(),			/* 31 */
	cdev_notdef(),			/* 32: RL01/02 on unibus */
	cdev_log_init(1,log),		/* 33: /dev/klog */
	cdev_tty_init(NDHU,dhu),	/* 34: DHU-11 */
	cdev_notdef(),			/* 35 */
	cdev_notdef(),			/* 36 */
	cdev_notdef(),			/* 37 */
	cdev_tape_init(NMT,mt),		/* 38: MSCP tape */
	cdev_notdef(),			/* 39 */
	cdev_notdef(),			/* 40 */
	cdev_notdef(),			/* 41 */
	cdev_pf_init(NPF,pf),		/* 42: packet filter */
	cdev_notdef(),			/* 43 */
	cdev_notdef(),			/* 44  was Datakit */
	cdev_notdef(),			/* 45  was Datakit */
	cdev_notdef(),			/* 46  was Datakit */
	cdev_notdef(),			/* 47 */
	cdev_tty_init(NQSC,qsc),	/* 48: SC26C94 on VXT2000 */
	cdev_systrace_init(NSYSTRACE,systrace),	/* 49: system call tracing */
	cdev_ksyms_init(NKSYMS,ksyms),	/* 50: Kernel symbols device */
	cdev_notdef(),			/* 51 */
	cdev_notdef(),			/* 52: was: KDB50/RA?? */
	cdev_fd_init(1,filedesc),	/* 53: file descriptor pseudo-device */
	cdev_notdef(),			/* 54: was: concatenated disk driver */
	cdev_disk_init(NVND,vnd),	/* 55: vnode disk driver */
	cdev_bpf_init(NBPFILTER,bpf),	/* 56: berkeley packet filter */
	cdev_tun_init(NTUN,tun),	/* 57: tunnel filter */
	cdev_disk_init(NHD,hd),		/* 58: HDC9224/RD?? */
	cdev_disk_init(NSD,sd),		/* 59: SCSI disk */
	cdev_tape_init(NST,st),		/* 60: SCSI tape */
	cdev_disk_init(NCD,cd),		/* 61: SCSI CD-ROM */
	cdev_disk_init(NRD,rd),		/* 62: memory disk driver */
	cdev_ch_init(NCH,ch),		/* 63: SCSI autochanger */
	cdev_notdef(),			/* 64 */
	cdev_uk_init(NUK,uk),		/* 65: SCSI unknown */
	cdev_tty_init(NDL,dl),		/* 66: DL11 */
	cdev_random_init(1,random),	/* 67: random data source */
	cdev_wsdisplay_init(NWSDISPLAY, wsdisplay), /* 68: frame buffers */
	cdev_mouse_init(NWSKBD, wskbd),	/* 69: keyboards */
	cdev_mouse_init(NWSMOUSE, wsmouse), /* 70: mice */
	cdev_disk_init(NRY,ry),		/* 71: VS floppy */
	cdev_bio_init(NBIO,bio),	/* 72: ioctl tunnel */
	cdev_notdef(),			/* 73 was: RAIDframe disk driver */
	cdev_notdef(),			/* 74 */
	cdev_ptm_init(NPTY,ptm),	/* 75: pseudo-tty ptm device */
	cdev_notdef(),			/* 76 */
	cdev_notdef(),			/* 77 */
	cdev_vscsi_init(NVSCSI,vscsi),	/* 78: vscsi */
	cdev_disk_init(1,diskmap),	/* 79: disk mapper */
	cdev_pppx_init(NPPPX,pppx),	/* 80: pppx */
	cdev_audio_init(NAUDIO,audio),	/* 81: /dev/audio */
	cdev_fuse_init(NFUSE,fuse),	/* 82: fuse */
};
int	nchrdev = nitems(cdevsw);

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

int	chrtoblktbl[] = {
	NODEV,	/* 0 */
	NODEV,	/* 1 */
	NODEV,	/* 2 */
	NODEV,	/* 3 */
	NODEV,	/* 4 */
	NODEV,	/* 5 */
	NODEV,	/* 6 */
	NODEV,	/* 7 */
	NODEV,	/* 8 */
	9,	/* 9 ra */
	NODEV,	/* 10 */
	NODEV,	/* 11 */
	NODEV,	/* 12 */
	NODEV,	/* 13 */
	NODEV,	/* 14 */
	NODEV,	/* 15 */
	NODEV,	/* 16 */
	NODEV,	/* 17 */
	NODEV,	/* 18 */
	NODEV,	/* 19 */
	NODEV,	/* 20 */
	NODEV,	/* 21 */
	NODEV,	/* 22 */
	NODEV,	/* 23 */
	NODEV,	/* 24 */
	NODEV,	/* 25 */
	NODEV,	/* 26 */
	NODEV,	/* 27 */
	NODEV,	/* 28 */
	NODEV,	/* 29 */
	12,	/* 30 rx */
	NODEV,	/* 31 */
	NODEV,	/* 32 */
	NODEV,	/* 33 */
	NODEV,	/* 34 */
	NODEV,	/* 35 */
	NODEV,	/* 36 */
	NODEV,	/* 37 */
	15,	/* 38 mt */
	NODEV,	/* 39 */
	NODEV,	/* 40 */
	NODEV,	/* 41 */
	NODEV,	/* 42 */
	NODEV,	/* 43 */
	NODEV,	/* 44 */
	NODEV,	/* 45 */
	NODEV,	/* 46 */
	NODEV,	/* 47 */
	NODEV,	/* 48 */
	NODEV,	/* 49 */
	NODEV,	/* 50 */
	NODEV,	/* 51 */
	NODEV,	/* 52 */
	NODEV,	/* 53 */
	NODEV,	/* 54 */
	18,	/* 55 vnd */
	NODEV,	/* 56 */
	NODEV,	/* 57 */
	19,	/* 58 hd */
	20,	/* 59 sd */
	21,	/* 60 st */
	22,	/* 61 cd */
	23,	/* 62 rd */
	NODEV,	/* 63 ch */
	NODEV,	/* 64 */
	NODEV,	/* 65 uk */
	NODEV,	/* 66 */
	NODEV,	/* 67 */
	NODEV,	/* 68 */
	NODEV,	/* 69 */
	NODEV,	/* 70 */
	24,	/* 71 ry */
	NODEV,	/* 72 */
	25,	/* 73 raid */
	NODEV,	/* 74 */
};
int nchrtoblktbl = nitems(chrtoblktbl);

/*
 * Returns true if dev is /dev/mem or /dev/kmem.
 */
int
iskmemdev(dev)
	dev_t dev;
{
	return (major(dev) == 3 && minor(dev) < 2);
}

/*
 * Returns true if dev is /dev/zero.
 * ?? Shall I use 12 as /dev/zero?
 */
int
iszerodev(dev)
	dev_t dev;
{

	return (major(dev) == 3 && minor(dev) == 12);
}

int getmajor(void *);	/* XXX used by dz_ibus and wscons, die die die */

int
getmajor(void *ptr)
{
	int i;

	for (i = 0; i < nchrdev; i++)
		if (cdevsw[i].d_open == ptr)
			return i;
	
	return (-1);
}

dev_t
getnulldev()
{
	return makedev(3, 2);
}
