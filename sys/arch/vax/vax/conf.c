/*	$NetBSD: conf.c,v 1.15 1996/01/28 11:46:54 ragge Exp $	*/

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
 *	@(#)conf.c	7.18 (Berkeley) 5/9/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/vnode.h>

int	ttselect	__P((dev_t, int, struct proc *));

#ifndef LKM
#define lkmenodev       enodev
#else
int     lkmenodev();
#endif

#include "hp.h" /* 0 */
bdev_decl(hp);

#include "tu.h"
bdev_decl(ht);

#include "rk.h"
bdev_decl(rk);

bdev_decl(sw);

#include "te.h"
bdev_decl(tm);

#include "tmscp.h"
bdev_decl(tmscp);

#include "ts.h"
bdev_decl(ts);

#include "mu.h"
bdev_decl(mt);

#if defined(VAX750)
#define	NCTU	1
#else
#define	NCTU	0
#endif
bdev_decl(ctu);

#include "uda.h"
bdev_decl(uda);

#include "kdb.h"
bdev_decl(kdb);

#include "up.h"
bdev_decl(up);

#include "tj.h"
bdev_decl(ut);

#include "rb.h"
bdev_decl(idc);

#include "rx.h"
bdev_decl(rx);

#include "uu.h"
bdev_decl(uu);

#include "rl.h"
bdev_decl(rl);

#include "ccd.h"
bdev_decl(ccd);

#include "vnd.h"
bdev_decl(vnd);

struct bdevsw	bdevsw[] =
{
	bdev_disk_init(NHP,hp),		/* 0: RP0?/RM0? */
	bdev_tape_init(NTU,ht),		/* 1: TU77 w/ TM03 */
	bdev_disk_init(NUP,up),		/* 2: SC-21/SC-31 */
	bdev_disk_init(NRK,rk),		/* 3: RK06/07 */
	bdev_swap_init(1,sw),		/* 4: swap pseudo-device */
	bdev_tape_init(NTE,tm),		/* 5: TM11/TE10 */
	bdev_tape_init(NTS,ts),		/* 6: TS11 */
	bdev_tape_init(NMU,mt),		/* 7: TU78 */
	bdev_tape_init(NCTU,ctu),	/* 8: Console TU58 on 730/750 */
	bdev_disk_init(NUDA,uda),	/* 9: UDA50/RA?? */
	bdev_tape_init(NTJ,ut),		/* 10: TU45 */
	bdev_disk_init(NRB,idc),	/* 11: IDC (RB730) */
	bdev_disk_init(NRX,rx),		/* 12: RX01/02 on unibus */
	bdev_disk_init(NUU,uu),		/* 13: TU58 on DL11 */
	bdev_disk_init(NRL,rl),		/* 14: RL01/02 */
	bdev_tape_init(NTMSCP,tmscp),	/* 15: TMSCP tape */
	bdev_disk_init(NKDB,kdb),	/* 16: KDB50/RA?? */
	bdev_disk_init(NCCD,ccd),	/* 17: concatenated disk driver */
	bdev_disk_init(NVND,vnd),	/* 18: vnode disk driver */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

/*
 * Console routines for VAX console. There are always an generic console,
 * but maybe we should care about RD, QDSS etc?
 */
#include <dev/cons.h>

#define gencnpollc      nullcnpollc
cons_decl(gen);

struct	consdev	constab[]={
/* Generic console, should always be present */
	cons_init(gen),

#ifdef notyet
/* We may not always use builtin console, sometimes RD */
	{ rdcnprobe, rdcninit, rdcngetc, rdcnputc },
#endif
	{ 0 }
};

/* Special for console storage */
#define dev_type_rw(n)	int n __P((dev_t, int, int, struct proc *))

/* plotters - open, close, write, ioctl, select */
#define cdev_plotter_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, dev_init(c,n,select), (dev_type_mmap((*))) enodev }

/* console mass storage - open, close, read/write */
#define	cdev_cnstore_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), (dev_type_ioctl((*))) enodev, \
	(dev_type_stop((*))) enodev, 0, (dev_type_select((*))) enodev, \
	(dev_type_mmap((*))) enodev }

#define	cdev_lp_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	dev_init(c,n,write), (dev_type_ioctl((*))) enodev, \
	(dev_type_stop((*))) enodev, 0, seltrue, (dev_type_mmap((*))) enodev }

/* graphic display adapters */
#define	cdev_graph_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_init(c,n,stop), \
	0, dev_init(c,n,select), (dev_type_mmap((*))) enodev }

/* Ingres */
#define cdev_ingres_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) nullop, \
	(dev_type_write((*))) nullop, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) nullop, 0, (dev_type_select((*))) nullop, \
	(dev_type_mmap((*))) enodev }



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
#ifdef LKM
#define	NLKM	1
#else
#define	NLKM	0
#endif
cdev_decl(lkm);

cdev_decl(hp);
cdev_decl(rk);
cdev_decl(tm);
cdev_decl(tmscp);
cdev_decl(ts);
cdev_decl(mt);
cdev_decl(uda);
cdev_decl(up);
cdev_decl(ut);
cdev_decl(idc);
cdev_decl(fd);
cdev_decl(gencn);
cdev_decl(rx);
cdev_decl(rl);
cdev_decl(kdb);
cdev_decl(ccd);

#include "ct.h"
cdev_decl(ct);
#include "dh.h"
cdev_decl(dh);
#include "dmf.h"
cdev_decl(dmf);

#include "np.h"
cdev_decl(np);

#if VAX8600
#define	NCRL 1
#else
#define NCRL 0
#endif
#define	crlread	crlrw
#define crlwrite crlrw
cdev_decl(crl);

#if VAX8200
#define NCRX 1
#else
#define NCRX 0
#endif
#define	crxread	crxrw
#define	crxwrite crxrw
cdev_decl(crx);

#if VAX780
#define	NFL 1
#else
#define NFL 0
#endif
#define flread flrw
#define flwrite flrw
cdev_decl(fl);

#include "dz.h"
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

#include "qv.h"
cdev_decl(qv);

#include "qd.h"
cdev_decl(qd);

#if defined(INGRES)
#define	NII 1
#else
#define	NII 0
#endif
cdev_decl(ii);

cdev_decl(vnd);

#include "bpfilter.h"
cdev_decl(bpf);

#include "tun.h" 
cdev_decl(tun);

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
	cdev_cnstore_init(NFL,fl),	/* 8: 11/780 console floppy */
	cdev_disk_init(NUDA,uda),	/* 9: MSCP disk interface */
	cdev_plotter_init(NVA,va),	/* 10: Benson-Varian plotter */
	cdev_disk_init(NRK,rk),		/* 11: RK06/07 */
	cdev_tty_init(NDH,dh),		/* 12: DH-11/DM-11 */
	cdev_disk_init(NUP,up),		/* 13: SC-21/SC-31 */
	cdev_tape_init(NTE,tm),		/* 14: TM11/TE10 */
	cdev_lp_init(NLP,lp),		/* 15: LP-11 line printer */
	cdev_tape_init(NTS,ts),		/* 16: TS11 */
	cdev_tape_init(NTJ,ut),		/* 17: TU45 */
	cdev_lp_init(NCT,ct),		/* 18: phototypesetter interface */
	cdev_tape_init(NMU,mt),		/* 19: TU78 */
	cdev_tty_init(NPTY,pts),	/* 20: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 21: pseudo-tty master */
	cdev_tty_init(NDMF,dmf),	/* 22: DMF32 */
	cdev_disk_init(NRB,idc),	/* 23: IDC (RB730) */
	cdev_lp_init(NDN,dn),		/* 24: DN-11 autocall unit */
	cdev_tty_init(1,gencn),		/* 25: Generic console (mtpr...) */
	cdev_audio_init(NLPA,lpa),	/* 26 ??? */
	cdev_graph_init(NPS,ps),	/* 27: E/S graphics device */
	cdev_lkm_init(NLKM,lkm),	/* 28: loadable module driver */
	cdev_ch_init(NAD,ad),		/* 29: DT A/D converter */
	cdev_disk_init(NRX,rx),		/* 30: RX01/02 on unibus */
	cdev_graph_init(NIK,ik),	/* 31: Ikonas frame buffer */
	cdev_disk_init(NRL,rl),		/* 32: RL01/02 on unibus */
	cdev_log_init(1,log),		/* 33: /dev/klog */
	cdev_tty_init(NDHU,dhu),	/* 34: DHU-11 */
	cdev_cnstore_init(NCRL,crl),	/* 35: Console RL02 on 8600 */
	cdev_notdef(),			/* 36: was vs100 interface. ??? */
	cdev_tty_init(NDMZ,dmz),	/* 37: DMZ32 */
	cdev_tape_init(NTMSCP,tmscp),	/* 38: TMSCP tape */
	cdev_audio_init(NNP,np),	/* 39: NP Intelligent Board */
	cdev_graph_init(NQV,qv),	/* 40: QVSS graphic display */
	cdev_graph_init(NQD,qd),	/* 41: QDSS graphic display */
	cdev_notdef(),			/* 42 */
	cdev_ingres_init(NII,ii),	/* 43: Ingres device */
	cdev_notdef(),			/* 44  was Datakit */
	cdev_notdef(),			/* 45  was Datakit */
	cdev_notdef(),			/* 46  was Datakit */
	cdev_notdef(),			/* 47 */
	cdev_notdef(),			/* 48 */
	cdev_notdef(),			/* 49 */
	cdev_notdef(),			/* 50 */
	cdev_cnstore_init(NCRX,crx),	/* 51: Console RX50 at 8200 */
	cdev_disk_init(NKDB,kdb),	/* 52: KDB50/RA?? */
	cdev_fd_init(1,fd),		/* 53: file descriptor pseudo-device */
	cdev_disk_init(NCCD,ccd),	/* 54: concatenated disk driver */
	cdev_disk_init(NVND,vnd),	/* 55: vnode disk driver */
	cdev_bpftun_init(NBPFILTER,bpf),/* 56: berkeley packet filter */
	cdev_bpftun_init(NTUN,tun),	/* 57: tunnel filter */
};
int	nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

int	mem_no = 3; 	/* major device number of memory special file */

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
	0,    	/* 4 */
	1,    	/* 5 */
	NODEV,	/* 6 */
	NODEV,	/* 7 */
	NODEV,	/* 8 */
	9,   	/* 9 */
	NODEV,	/* 10 */
	3,    	/* 11 */
	NODEV,	/* 12 */
	2,    	/* 13 */
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
	NODEV, 	/* 51 */
	16,	/* 52 */
	NODEV,	/* 53 */
	17,	/* 54 */
	18,	/* 55 */
};

chrtoblk(dev)
	dev_t dev;
{
	if(major(dev)>=nchrdev) return(NODEV);
	return chrtoblktbl[major(dev)]==NODEV?NODEV:
		makedev(chrtoblktbl[major(dev)],minor(dev));
}

/*
 * Returns true if dev is /dev/mem or /dev/kmem.
 */
iskmemdev(dev)
	dev_t dev;
{

	return (major(dev) == 3 && minor(dev) < 2);
}

/*
 * Returns true if dev is /dev/zero.
 * ?? Shall I use 12 as /dev/zero?
 */
iszerodev(dev)
	dev_t dev;
{

	return (major(dev) == 3 && minor(dev) == 12);
}
