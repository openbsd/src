/*	$OpenBSD: conf.c,v 1.52 2007/05/28 22:26:03 todd Exp $ */
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

#include "hp.h" /* 0 */
bdev_decl(hp);

#include "ht.h"
bdev_decl(ht);

#include "rk.h"
bdev_decl(rk);

#include "te.h"
bdev_decl(tm);

#include "mt.h"
bdev_decl(mt);

#include "ts.h"
bdev_decl(ts);

#include "mu.h"
bdev_decl(mu);

#if defined(VAX750)
#define NCTU	1
#else
#define NCTU	0
#endif
bdev_decl(ctu);
#include "rd.h"

#include "ra.h"
bdev_decl(ra);
bdev_decl(rx);

#include "up.h"
bdev_decl(up);

#include "tj.h"
bdev_decl(ut);

#include "rb.h"
bdev_decl(idc);

#include "uu.h"
bdev_decl(uu);

#if 0
#include "rl.h"
bdev_decl(rl);
#endif

#include "ccd.h"

#include "raid.h"

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
	bdev_disk_init(NHP,hp),		/* 0: RP0?/RM0? */
	bdev_tape_init(NHT,ht),		/* 1: TU77 w/ TM03 */
	bdev_disk_init(NUP,up),		/* 2: SC-21/SC-31 */
	bdev_disk_init(NRK,rk),		/* 3: RK06/07 */
	bdev_swap_init(1,sw),		/* 4: swap pseudo-device */
	bdev_tape_init(NTE,tm),		/* 5: TM11/TE10 */
	bdev_tape_init(NTS,ts),		/* 6: TS11 */
	bdev_tape_init(NMU,mu),		/* 7: TU78 */
	bdev_tape_init(NCTU,ctu),	/* 8: Console TU58 on 730/750 */
	bdev_disk_init(NRA,ra),		/* 9: MSCP disk */
	bdev_tape_init(NTJ,ut),		/* 10: TU45 */
	bdev_disk_init(NRB,idc),	/* 11: IDC (RB730) */
	bdev_disk_init(NRX,rx),		/* 12: RX?? on MSCP */
	bdev_disk_init(NUU,uu),		/* 13: TU58 on DL11 */
	bdev_notdef(),			/* 14: RL01/02 */
	bdev_tape_init(NMT,mt),		/* 15: MSCP tape */
	bdev_notdef(),			/* 16: was: KDB50/RA?? */
	bdev_disk_init(NCCD,ccd),	/* 17: concatenated disk driver */
	bdev_disk_init(NVND,vnd),	/* 18: vnode disk driver */
	bdev_disk_init(NHD,hd),		/* 19: VS3100 ST506 disk */
	bdev_disk_init(NSD,sd),		/* 20: SCSI disk */
	bdev_tape_init(NST,st),		/* 21: SCSI tape */
	bdev_disk_init(NCD,cd),		/* 22: SCSI CD-ROM */
	bdev_disk_init(NRD,rd),		/* 23: ram disk driver */
	bdev_disk_init(NRY,ry),		/* 24: VS3100 floppy */
	bdev_disk_init(NRAID,raid),	/* 25: RAIDframe disk driver */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

/*
 * Console routines for VAX console.
 */
#include <dev/cons.h>

cons_decl(dz);
cons_decl(gen);
cons_decl(qd);
cons_decl(qsc);
cons_decl(ws);

#include "dz.h"
#include "qd.h"
#include "qsc.h"
#include "qv.h"
#include "wsdisplay.h"
#include "wskbd.h"

struct	consdev constab[]={
#if VAX8600 || VAX8200 || VAX780 || VAX750 || VAX650 || VAX630 || VAX660 || \
    VAX670 || VAX680
#define NGEN	1
	cons_init(gen), /* Generic console type; mtpr/mfpr */
#else
#define NGEN	0
#endif
#if VAX410 || VAX43 || VAX46 || VAX48 || VAX49 || VAX53
#if NDZ > 0
	cons_init(dz),	/* DZ11-like serial console on VAXstations */
#endif
#endif
#ifdef VXT
#if NQSC > 0
	cons_init(qsc),	/* SC26C94 serial console on VXT2000 */
#endif
#endif
#if VAX650 || VAX630
#if NQV
	cons_init(qv),	/* QVSS/QDSS bit-mapped console driver */
#endif
#if NQD
	cons_init(qd),
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

/* Special for console storage */
#define dev_type_rw(n)	int n(dev_t, int, int, struct proc *)

/* plotters - open, close, write, ioctl, poll*/
#define cdev_plotter_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, dev_init(c,n,poll), (dev_type_mmap((*))) enodev }

/* console mass storage - open, close, read/write */
#define cdev_cnstore_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), (dev_type_ioctl((*))) enodev, \
	(dev_type_stop((*))) enodev, 0, (dev_type_poll((*))) enodev, \
	(dev_type_mmap((*))) enodev }

#define cdev_lp_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	dev_init(c,n,write), (dev_type_ioctl((*))) enodev, \
	(dev_type_stop((*))) enodev, 0, seltrue, (dev_type_mmap((*))) enodev }

/* graphic display adapters */
#define cdev_graph_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_init(c,n,stop), \
	0, dev_init(c,n,poll), (dev_type_mmap((*))) enodev }

/* Ingres */
#define cdev_ingres_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) nullop, \
	(dev_type_write((*))) nullop, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) nullop, 0, (dev_type_poll((*))) nullop, \
	(dev_type_mmap((*))) enodev }

#define mmread	mmrw
#define mmwrite mmrw
cdev_decl(mm);
#include "bio.h"
#include "pty.h"

cdev_decl(hp);
cdev_decl(rk);
cdev_decl(tm);
cdev_decl(mt);
cdev_decl(ts);
cdev_decl(mu);
cdev_decl(ra);
cdev_decl(up);
cdev_decl(ut);
cdev_decl(idc);
cdev_decl(fd);
cdev_decl(gencn);
cdev_decl(rx);
#if 0
cdev_decl(rl);
#endif
cdev_decl(hd);
cdev_decl(ry);

#include "ct.h"
cdev_decl(ct);
#include "dh.h"
cdev_decl(dh);
#include "dmf.h"
cdev_decl(dmf);

#include "np.h"
cdev_decl(np);

#if VAX8600
#define NCRL 1
#else
#define NCRL 0
#endif
#define crlread crlrw
#define crlwrite crlrw
cdev_decl(crl);

#if VAX8200
#define NCRX 1
#else
#define NCRX 0
#endif
#define crxread crxrw
#define crxwrite crxrw
cdev_decl(crx);

#if VAX780
#define NCFL 1
#else
#define NCFL 0
#endif
#define cflread cflrw
#define cflwrite cflrw
cdev_decl(cfl);

cdev_decl(qsc);

cdev_decl(dz);

#include "vp.h"
cdev_decl(vp);

#include "lp.h"
cdev_decl(lp);

#include "va.h"
cdev_decl(va);

#include "lpa.h"
cdev_decl(lpa);

#include "dn.h"
cdev_decl(dn);

#include "ik.h"
cdev_decl(ik);

#include "ps.h"
cdev_decl(ps);

#include "ad.h"
cdev_decl(ad);

#include "dhu.h"
cdev_decl(dhu);

#include "dmz.h"
cdev_decl(dmz);

cdev_decl(qv);
cdev_decl(qd);

#include "dl.h"
cdev_decl(dl);

#if defined(INGRES)
#define NII 1
#else
#define NII 0
#endif
cdev_decl(ii);

#include "bpfilter.h"

#include "tun.h" 
#include "ch.h"
#include "ss.h"
#include "uk.h"

#ifdef XFS
#include <xfs/nxfs.h>
cdev_decl(xfs_dev);
#endif

#include "wsdisplay.h"
#include "wskbd.h"
#include "wsmouse.h"

#include "pf.h"

#include "systrace.h"

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_tty_init(NDZ,dz),		/* 1: DZ11 */
	cdev_ctty_init(1,ctty),		/* 2: controlling terminal */
	cdev_mm_init(1,mm),		/* 3: /dev/{null,mem,kmem,...} */
	cdev_disk_init(NHP,hp),		/* 4: Massbuss disk */
	cdev_notdef(),			/* 5 */
	cdev_plotter_init(NVP,vp),	/* 6: Versatec plotter */
	cdev_swap_init(1,sw),		/* 7 */
	cdev_cnstore_init(NCFL,cfl),	/* 8: 11/780 console floppy */
	cdev_disk_init(NRA,ra),		/* 9: MSCP disk interface */
	cdev_plotter_init(NVA,va),	/* 10: Benson-Varian plotter */
	cdev_disk_init(NRK,rk),		/* 11: RK06/07 */
	cdev_tty_init(NDH,dh),		/* 12: DH-11/DM-11 */
	cdev_disk_init(NUP,up),		/* 13: SC-21/SC-31 */
	cdev_tape_init(NTE,tm),		/* 14: TM11/TE10 */
	cdev_lp_init(NLP,lp),		/* 15: LP-11 line printer */
	cdev_tape_init(NTS,ts),		/* 16: TS11 */
	cdev_tape_init(NTJ,ut),		/* 17: TU45 */
	cdev_lp_init(NCT,ct),		/* 18: phototypesetter interface */
	cdev_tape_init(NMU,mu),		/* 19: TU78 */
	cdev_tty_init(NPTY,pts),	/* 20: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 21: pseudo-tty master */
	cdev_tty_init(NDMF,dmf),	/* 22: DMF32 */
	cdev_disk_init(NRB,idc),	/* 23: IDC (RB730) */
	cdev_lp_init(NDN,dn),		/* 24: DN-11 autocall unit */
	cdev_tty_init(NGEN,gencn),	/* 25: Generic console (mtpr...) */
	cdev_audio_init(NLPA,lpa),	/* 26 ??? */
	cdev_graph_init(NPS,ps),	/* 27: E/S graphics device */
	cdev_lkm_init(NLKM,lkm),	/* 28: loadable module driver */
	cdev_ch_init(NAD,ad),		/* 29: DT A/D converter */
	cdev_disk_init(NRX,rx),		/* 30: RX?? on MSCP */
	cdev_graph_init(NIK,ik),	/* 31: Ikonas frame buffer */
	cdev_notdef(),			/* 32: RL01/02 on unibus */
	cdev_log_init(1,log),		/* 33: /dev/klog */
	cdev_tty_init(NDHU,dhu),	/* 34: DHU-11 */
	cdev_cnstore_init(NCRL,crl),	/* 35: Console RL02 on 8600 */
	cdev_notdef(),			/* 36 */
	cdev_tty_init(NDMZ,dmz),	/* 37: DMZ32 */
	cdev_tape_init(NMT,mt),		/* 38: MSCP tape */
	cdev_audio_init(NNP,np),	/* 39: NP Intelligent Board */
	cdev_graph_init(NQV,qv),	/* 40: QVSS graphic display */
	cdev_graph_init(NQD,qd),	/* 41: QDSS graphic display */
	cdev_pf_init(NPF,pf),		/* 42: packet filter */
	cdev_ingres_init(NII,ii),	/* 43: Ingres device */
	cdev_notdef(),			/* 44  was Datakit */
	cdev_notdef(),			/* 45  was Datakit */
	cdev_notdef(),			/* 46  was Datakit */
	cdev_notdef(),			/* 47 */
	cdev_tty_init(NQSC,qsc),	/* 48: SC26C94 on VXT2000 */
	cdev_systrace_init(NSYSTRACE,systrace),	/* 49: system call tracing */
	cdev_ksyms_init(NKSYMS,ksyms),	/* 50: Kernel symbols device */
	cdev_cnstore_init(NCRX,crx),	/* 51: Console RX50 at 8200 */
	cdev_notdef(),			/* 52: was: KDB50/RA?? */
	cdev_fd_init(1,filedesc),	/* 53: file descriptor pseudo-device */
	cdev_disk_init(NCCD,ccd),	/* 54: concatenated disk driver */
	cdev_disk_init(NVND,vnd),	/* 55: vnode disk driver */
	cdev_bpftun_init(NBPFILTER,bpf),/* 56: berkeley packet filter */
	cdev_bpftun_init(NTUN,tun),	/* 57: tunnel filter */
	cdev_disk_init(NHD,hd),		/* 58: HDC9224/RD?? */
	cdev_disk_init(NSD,sd),		/* 59: SCSI disk */
	cdev_tape_init(NST,st),		/* 60: SCSI tape */
	cdev_disk_init(NCD,cd),		/* 61: SCSI CD-ROM */
	cdev_disk_init(NRD,rd),		/* 62: memory disk driver */
	cdev_ch_init(NCH,ch),		/* 63: SCSI autochanger */
	cdev_scanner_init(NSS,ss),	/* 64: SCSI scanner */
	cdev_uk_init(NUK,uk),		/* 65: SCSI unknown */
	cdev_tty_init(NDL,dl),		/* 66: DL11 */
	cdev_random_init(1,random),	/* 67: random data source */
	cdev_wsdisplay_init(NWSDISPLAY, wsdisplay), /* 68: frame buffers */
	cdev_mouse_init(NWSKBD, wskbd),	/* 69: keyboards */
	cdev_mouse_init(NWSMOUSE, wsmouse), /* 70: mice */
	cdev_disk_init(NRY,ry),		/* 71: VS floppy */
	cdev_bio_init(NBIO,bio),	/* 72: ioctl tunnel */
	cdev_disk_init(NRAID,raid),	/* 73: RAIDframe disk driver */
#ifdef XFS
	cdev_xfs_init(NXFS,xfs_dev),	/* 74: xfs communication device */
#else
	cdev_notdef(),			/* 74 */
#endif
	cdev_ptm_init(NPTY,ptm),	/* 75: pseudo-tty ptm device */
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

int	chrtoblktbl[] = {
	NODEV,	/* 0 */
	NODEV,	/* 1 */
	NODEV,	/* 2 */
	NODEV,	/* 3 */
	0,	/* 4 */
	1,	/* 5 */
	NODEV,	/* 6 */
	NODEV,	/* 7 */
	NODEV,	/* 8 */
	9,	/* 9 */
	NODEV,	/* 10 */
	3,	/* 11 */
	NODEV,	/* 12 */
	2,	/* 13 */
	5,	/* 14 */
	NODEV,	/* 15 */
	6,	/* 16 */
	10,	/* 17 */
	NODEV,	/* 18 */
	7,	/* 19 */
	NODEV,	/* 20 */
	NODEV,	/* 21 */
	NODEV,	/* 22 */
	11,	/* 23 */
	NODEV,	/* 24 */
	NODEV,	/* 25 */
	NODEV,	/* 26 */
	NODEV,	/* 27 */
	NODEV,	/* 28 */
	NODEV,	/* 29 */
	12,	/* 30 */
	NODEV,	/* 31 */
	14,	/* 32 */
	NODEV,	/* 33 */
	NODEV,	/* 34 */
	NODEV,	/* 35 */
	NODEV,	/* 36 */
	NODEV,	/* 37 */
	15,	/* 38 */
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
	16,	/* 52 */
	NODEV,	/* 53 */
	17,	/* 54 */
	18,	/* 55 */
	NODEV,	/* 56 */
	NODEV,	/* 57 */
	19,	/* 58 */
	20,	/* 59 */
	21,	/* 60 */
	22,	/* 61 */
	23,	/* 62 */
	NODEV,	/* 63 */
	NODEV,	/* 64 */
	NODEV,	/* 65 */
	NODEV,	/* 66 */
	NODEV,	/* 67 */
	NODEV,	/* 68 */
	NODEV,	/* 69 */
	NODEV,	/* 70 */
	NODEV,	/* 71 */
	NODEV,	/* 72 */
	25,	/* 73 */
	NODEV,	/* 74 */
};
int nchrtoblktbl = sizeof(chrtoblktbl) / sizeof(chrtoblktbl[0]);

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
