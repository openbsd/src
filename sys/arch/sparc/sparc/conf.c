/*	$OpenBSD: conf.c,v 1.57 2011/04/30 15:24:07 miod Exp $	*/
/*	$NetBSD: conf.c,v 1.40 1996/04/11 19:20:03 thorpej Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)conf.c	8.3 (Berkeley) 11/14/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/vnode.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <machine/conf.h>

#include "bio.h"
#include "pty.h"
#include "bpfilter.h"
#include "tun.h"
#include "audio.h"
#include "vnd.h"
#include "ccd.h"
#include "raid.h"
#include "ch.h"
#include "uk.h"
#include "sd.h"
#include "st.h"
#include "cd.h"
#include "rd.h"
#include "presto.h"

#include "zstty.h"

#include "fdc.h"		/* has NFDC and NFD; see files.sparc */
#include "xd.h"
#include "xy.h"
#include "bpp.h"
#include "magma.h"		/* has NMTTY and NMBPP */
#include "spif.h"		/* has NSTTY and NSBPP */
#include "scf.h"
#include "flash.h"
#include "fga.h"
#include "daadio.h"
#include "com.h"

#include "wsdisplay.h"
#include "wskbd.h"
#include "wsmouse.h"
#include "wsmux.h"

#ifdef NNPFS
#include <nnpfs/nnnpfs.h>
cdev_decl(nnpfs_dev);
#endif
#include "ksyms.h"

struct bdevsw	bdevsw[] =
{
	bdev_notdef(),			/* 0 */
	bdev_notdef(),			/* 1 */
	bdev_notdef(),			/* 2 */
	bdev_disk_init(NXY,xy),		/* 3: SMD disk */
	bdev_swap_init(1,sw),		/* 4 swap pseudo-device */
	bdev_notdef(),			/* 5 */
	bdev_notdef(),			/* 6 */
	bdev_disk_init(NSD,sd),		/* 7: SCSI disk */
	bdev_disk_init(NVND,vnd),	/* 8: vnode disk driver */
	bdev_disk_init(NCCD,ccd),	/* 9: concatenated disk driver */
	bdev_disk_init(NXD,xd),		/* 10: SMD disk */
	bdev_tape_init(NST,st),		/* 11: SCSI tape */
	bdev_notdef(),			/* 12 */
	bdev_notdef(),			/* 13 */
	bdev_notdef(),			/* 14 */
	bdev_notdef(),			/* 15 */
	bdev_disk_init(NFD,fd),		/* 16: floppy disk */
	bdev_disk_init(NRD,rd),		/* 17: ram disk driver */
	bdev_disk_init(NCD,cd),		/* 18: SCSI CD-ROM */
	bdev_lkm_dummy(),		/* 19 */
	bdev_lkm_dummy(),		/* 20 */
	bdev_lkm_dummy(),		/* 21 */
	bdev_lkm_dummy(),		/* 22 */
	bdev_lkm_dummy(),		/* 23 */
	bdev_lkm_dummy(),		/* 24 */
	bdev_disk_init(NRAID,raid),	/* 25: RAIDframe disk driver */
	bdev_disk_init(NPRESTO,presto),	/* 26: Prestoserve NVRAM */
};
int	nblkdev = nitems(bdevsw);

#include "pf.h"
#include "systrace.h"
#include "tctrl.h"
#include "vscsi.h"
#include "pppx.h"
#include "hotplug.h"

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_notdef(),			/* 1 */
	cdev_ctty_init(1,ctty),		/* 2: controlling terminal */
	cdev_mm_init(1,mm),		/* 3: /dev/{null,mem,kmem,...} */
	cdev_notdef(),			/* 4 */
	cdev_notdef(),			/* 5 */
	cdev_notdef(),			/* 6 */
	cdev_notdef(),			/* 7 was /dev/drum */
	cdev_notdef(),			/* 8 */
	cdev_disk_init(NXY,xy),		/* 9: SMD disk */
	cdev_notdef(),			/* 10 */
	cdev_notdef(),			/* 11 */
	cdev_tty_init(NZSTTY,zs),	/* 12: zs serial */
	cdev_notdef(),			/* 13: was /dev/mouse */
	cdev_notdef(),			/* 14 */
	cdev_notdef(),			/* 15: sun /dev/winNNN */
	cdev_log_init(1,log),		/* 16: /dev/klog */
	cdev_disk_init(NSD,sd),		/* 17: SCSI disk */
	cdev_tape_init(NST,st),		/* 18: SCSI tape */
	cdev_ch_init(NCH,ch),		/* 19: SCSI autochanger */
	cdev_tty_init(NPTY,pts),	/* 20: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 21: pseudo-tty master */
	cdev_notdef(),			/* 22: was /dev/fb */
	cdev_disk_init(NCCD,ccd),	/* 23: concatenated disk driver */
	cdev_fd_init(1,filedesc),	/* 24: file descriptor pseudo-device */
	cdev_disk_init(NPRESTO,presto),	/* 25: Prestoserve NVRAM */
	cdev_notdef(),			/* 26 */
	cdev_notdef(),			/* 27: was /dev/bwtwo */
	cdev_notdef(),			/* 28 */
	cdev_notdef(),			/* 29: was /dev/kbd */
	cdev_apm_init(NTCTRL,apm),	/* 30: tctrl APM interface */
	cdev_notdef(),			/* 31: was /dev/cgtwo */
	cdev_notdef(),			/* 32: should be /dev/gpone */
	cdev_notdef(),			/* 33 */
	cdev_notdef(),			/* 34 */
	cdev_notdef(),			/* 35 */
	cdev_tty_init(NCOM,com),	/* 36: SPARCbook modem */
	cdev_notdef(),			/* 37 */
	cdev_notdef(),			/* 38 */
	cdev_notdef(),			/* 39: was /dev/cgfour */
	cdev_notdef(),			/* 40 */
	cdev_notdef(),			/* 41 */
	cdev_disk_init(NXD,xd),		/* 42: SMD disk */
	cdev_svr4_net_init(NSVR4_NET,svr4_net),	/* 43: svr4 net pseudo-device */
	cdev_notdef(),			/* 44 */
	cdev_notdef(),			/* 45 */
	cdev_notdef(),			/* 46 */
	cdev_notdef(),			/* 47 */
	cdev_notdef(),			/* 48 */
	cdev_notdef(),			/* 49 */
	cdev_systrace_init(NSYSTRACE,systrace),	/* 50 system call tracing */
#ifdef NNPFS
	cdev_nnpfs_init(NNNPFS,nnpfs_dev),	/* 51: nnpfs communication device */
#else
	cdev_notdef(),			/* 51 */
#endif
	cdev_notdef(),			/* 52 */
	cdev_notdef(),			/* 53 */
	cdev_disk_init(NFD,fd),		/* 54: floppy disk */
	cdev_notdef(),			/* 55: was /dev/cgthree */
	cdev_notdef(),			/* 56 */
	cdev_notdef(),			/* 57 */
	cdev_disk_init(NCD,cd),		/* 58: SCSI CD-ROM */
	cdev_pf_init(NPF,pf),		/* 59: packet filter */
	cdev_notdef(),			/* 60 */
	cdev_notdef(),			/* 61 */
	cdev_notdef(),			/* 62 */
	cdev_notdef(),			/* 63 */
	cdev_notdef(),			/* 64: was /dev/cgeight */
	cdev_notdef(),			/* 65 */
	cdev_notdef(),			/* 66 */
	cdev_notdef(),			/* 67: was /dev/cgsix */
	cdev_notdef(),			/* 68 */
	cdev_gen_init(NAUDIO,audio),	/* 69: /dev/audio */
#if defined(SUN4) || defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	cdev_openprom_init(1,openprom),	/* 70: /dev/openprom */
#else
	cdev_notdef(),			/* 70 */
#endif
	cdev_notdef(),			/* 71 */
	cdev_notdef(),			/* 72 */
	cdev_notdef(),			/* 73 */
	cdev_notdef(),			/* 74 */
	cdev_notdef(),			/* 75 */
	cdev_notdef(),			/* 76 */
	cdev_notdef(),			/* 77 */
	cdev_wsdisplay_init(NWSDISPLAY,	/* 78: frame buffers, etc. */
	    wsdisplay),
	cdev_mouse_init(NWSKBD, wskbd),	/* 79: keyboards */
	cdev_mouse_init(NWSMOUSE, wsmouse), /* 80: mice */
	cdev_mouse_init(NWSMUX, wsmux),	/* 81: ws multiplexer */
	cdev_notdef(),			/* 82 */
	cdev_notdef(),			/* 83 */
	cdev_notdef(),			/* 84 */
	cdev_notdef(),			/* 85 */
	cdev_notdef(),			/* 86 */
	cdev_notdef(),			/* 87 */
	cdev_notdef(),			/* 88 */
	cdev_notdef(),			/* 89 */
	cdev_notdef(),			/* 90 */
	cdev_notdef(),			/* 91 */
	cdev_notdef(),			/* 92 */
	cdev_notdef(),			/* 93 */
	cdev_notdef(),			/* 94 */
	cdev_notdef(),			/* 95 */
	cdev_notdef(),			/* 96 */
	cdev_daadio_init(NDAADIO,daadio), /* 97: daadio */
	cdev_fga_init(NFGA,fga),	/* 98: fga */
	cdev_notdef(),			/* 99: was /dev/cgfourteen */
	cdev_tty_init(NMTTY,mtty),	/* 100: magma */
	cdev_gen_init(NMBPP,mbpp),	/* 101: magma */
	cdev_tty_init(NSTTY,stty),	/* 102: spif */
	cdev_gen_init(NSBPP,sbpp),	/* 103: spif */
	cdev_bpp_init(NBPP,bpp),	/* 104: bpp */
	cdev_bpf_init(NBPFILTER,bpf),	/* 105: packet filter */
	cdev_disk_init(NRD,rd),		/* 106: ram disk driver */
	cdev_scf_init(NSCF,scf),	/* 107: sysconfig regs */
	cdev_flash_init(NFLASH,flash),	/* 108: flash memory */
	cdev_notdef(),			/* 109: was /dev/tcx */
	cdev_disk_init(NVND,vnd),	/* 110: vnode disk driver */
	cdev_tun_init(NTUN,tun),	/* 111: network tunnel */
	cdev_lkm_init(NLKM,lkm),	/* 112: loadable module driver */
	cdev_lkm_dummy(),		/* 113 */
	cdev_lkm_dummy(),		/* 114 */
	cdev_lkm_dummy(),		/* 115 */
	cdev_lkm_dummy(),		/* 116 */
	cdev_lkm_dummy(),		/* 117 */
	cdev_lkm_dummy(),		/* 118 */
	cdev_random_init(1,random),	/* 119: random generator */
	cdev_uk_init(NUK,uk),		/* 120: unknown SCSI */
	cdev_notdef(),			/* 121 */
	cdev_ksyms_init(NKSYMS,ksyms),	/* 122: Kernel symbols device */
	cdev_disk_init(NRAID,raid),     /* 123: RAIDframe disk driver */
	cdev_bio_init(NBIO,bio),	/* 124: ioctl tunnel */
	cdev_ptm_init(NPTY,ptm),	/* 125: pseudo-tty ptm device */
	cdev_notdef(),			/* 126 */
	cdev_notdef(),			/* 127 */
	cdev_vscsi_init(NVSCSI,vscsi),	/* 128: vscsi */
	cdev_disk_init(1,diskmap),	/* 129: disk mapper */
	cdev_pppx_init(NPPPX,pppx),	/* 130: pppx */
	cdev_hotplug_init(NHOTPLUG,hotplug),	/* 131: devices hot plugging */
};
int	nchrdev = nitems(cdevsw);

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

/*
 * Routine that identifies /dev/mem and /dev/kmem.
 *
 * A minimal stub routine can always return 0.
 */
int
iskmemdev(dev)
	dev_t dev;
{

	return (major(dev) == mem_no && minor(dev) < 2);
}

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
	/* 19 */	NODEV,
	/* 20 */	NODEV,
	/* 21 */	NODEV,
	/* 22 */	NODEV,
	/* 23 */	9,
	/* 24 */	NODEV,
	/* 25 */	26,
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
	/* 52 */	NODEV,
	/* 53 */	NODEV,
	/* 54 */	16,
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
	/* 87 */	NODEV,
	/* 88 */	NODEV,
	/* 89 */	NODEV,
	/* 90 */	NODEV,
	/* 91 */	NODEV,
	/* 92 */	NODEV,
	/* 93 */	NODEV,
	/* 94 */	NODEV,
	/* 95 */	NODEV,
	/* 96 */	NODEV,
	/* 97 */	NODEV,
	/* 98 */	NODEV,
	/* 99 */	NODEV,
	/*100 */	NODEV,
	/*101 */	NODEV,
	/*102 */	NODEV,
	/*103 */	NODEV,
	/*104 */	NODEV,
	/*105 */	NODEV,
	/*106 */	17,
	/*107 */	NODEV,
	/*108 */	NODEV,
	/*109 */	NODEV,
	/*110 */	8,
	/*111 */	NODEV,
	/*112 */	NODEV,
	/*113 */	NODEV,
	/*114 */	NODEV,
	/*115 */	NODEV,
	/*116 */	NODEV,
	/*117 */	NODEV,
	/*118 */	NODEV,
	/*119 */	NODEV,
	/*120 */	NODEV,
	/*121 */	NODEV,
	/*122 */	NODEV,
	/*123 */	25,
};
int nchrtoblktbl = nitems(chrtoblktbl);
