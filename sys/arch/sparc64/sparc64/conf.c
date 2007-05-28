/*	$OpenBSD: conf.c,v 1.44 2007/05/28 22:26:03 todd Exp $	*/
/*	$NetBSD: conf.c,v 1.17 2001/03/26 12:33:26 lukem Exp $ */

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
#include "ch.h"
#include "ss.h"
#include "sd.h"
#include "st.h"
#include "cd.h"
#include "uk.h"
#include "wd.h"
#include "raid.h"

#ifdef notyet
#include "fb.h"
#endif
#include "zstty.h"
#include "sab.h"
#include "pcons.h"
#include "com.h"
#include "lpt.h"
#include "bpp.h"
#include "magma.h"		/* has NMTTY and NMBPP */
#include "spif.h"		/* has NSTTY and NSBPP */
#include "uperf.h"

#include "fdc.h"		/* has NFDC and NFD; see files.sparc */

#include "wsdisplay.h"
#include "wskbd.h"
#include "wsmouse.h"
#include "wsmux.h"

#ifdef USER_PCICONF
#include "pci.h"
cdev_decl(pci);
#endif

#include "rd.h"

#include "usb.h"
#include "uhid.h"
#include "ugen.h"
#include "ulpt.h"
#include "urio.h"
#include "ucom.h"
#include "uscanner.h"

#include "pf.h"

#ifdef XFS
#include <xfs/nxfs.h>
cdev_decl(xfs_dev);
#endif

#include "ksyms.h"
#include "inet.h"

#include "systrace.h"
#include "hotplug.h"

struct bdevsw	bdevsw[] =
{
	bdev_notdef(),			/* 0 */
	bdev_notdef(),			/* 1 */
	bdev_notdef(),			/* 2 */
	bdev_notdef(),			/* 3: SMD disk -- not this arch */
	bdev_swap_init(1,sw),		/* 4 swap pseudo-device */
	bdev_disk_init(NRD,rd),		/* 5: ram disk */
	bdev_notdef(),			/* 6 */
	bdev_disk_init(NSD,sd),		/* 7: SCSI disk */
	bdev_disk_init(NVND,vnd),	/* 8: vnode disk driver */
	bdev_disk_init(NCCD,ccd),	/* 9: concatenated disk driver */
	bdev_notdef(),			/* 10: SMD disk -- not this arch */
	bdev_tape_init(NST,st),		/* 11: SCSI tape */
	bdev_disk_init(NWD,wd),		/* 12: IDE disk */
	bdev_notdef(),			/* 13 */
	bdev_notdef(),			/* 14 */
	bdev_notdef(),			/* 15 */
	bdev_disk_init(NFD,fd),		/* 16: floppy disk */
	bdev_notdef(),			/* 17 */
	bdev_disk_init(NCD,cd),		/* 18: SCSI CD-ROM */
	bdev_lkm_dummy(),		/* 19 */
	bdev_lkm_dummy(),		/* 20 */
	bdev_lkm_dummy(),		/* 21 */
	bdev_lkm_dummy(),		/* 22 */
	bdev_lkm_dummy(),		/* 23 */
	bdev_lkm_dummy(),		/* 24 */
	bdev_disk_init(NRAID,raid),	/* 25: RAIDframe disk driver */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_notdef(),			/* 1: tapemaster tape */
	cdev_ctty_init(1,ctty),		/* 2: controlling terminal */
	cdev_mm_init(1,mm),		/* 3: /dev/{null,mem,kmem,...} */
	cdev_notdef(),			/* 4 */
	cdev_notdef(),			/* 5: tapemaster tape */
	cdev_notdef(),			/* 6: systech/versatec */
	cdev_swap_init(1,sw),		/* 7: /dev/drum (swap pseudo-device) */
	cdev_notdef(),			/* 8: Archive QIC-11 tape */
	cdev_notdef(),			/* 9: SMD disk on Xylogics 450/451 */
	cdev_notdef(),			/* 10: systech multi-terminal board */
	cdev_notdef(),			/* 11: DES encryption chip */
	cdev_tty_init(NZSTTY,zs),	/* 12: Zilog 8530 serial port */
	cdev_notdef(),			/* 13: /dev/mouse */
	cdev_notdef(),			/* 14: cgone */
	cdev_notdef(),			/* 15: sun /dev/winNNN */
	cdev_log_init(1,log),		/* 16: /dev/klog */
	cdev_disk_init(NSD,sd),		/* 17: SCSI disk */
	cdev_tape_init(NST,st),		/* 18: SCSI tape */
	cdev_ch_init(NCH,ch),		/* 19: SCSI autochanger */
	cdev_tty_init(NPTY,pts),	/* 20: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 21: pseudo-tty master */
	cdev_notdef(),			/* 22 */
	cdev_disk_init(NCCD,ccd),	/* 23: concatenated disk driver */
	cdev_fd_init(1,filedesc),	/* 24: file descriptor pseudo-device */
	cdev_uperf_init(NUPERF,uperf),	/* 25: performance counters */
	cdev_disk_init(NWD,wd),		/* 26: IDE disk */
	cdev_notdef(),			/* 27 */
	cdev_notdef(),			/* 28: Systech VPC-2200 versatec/centronics */
	cdev_notdef(),			/* 29 */
	cdev_notdef(),			/* 30: Xylogics tape */
	cdev_notdef(),			/* 31: /dev/cgtwo */
	cdev_notdef(),			/* 32: should be /dev/gpone */
	cdev_notdef(),			/* 33 */
	cdev_notdef(),			/* 34 */
	cdev_notdef(),			/* 35 */
	cdev_tty_init(NCOM,com),	/* 36: NS16x50 compatible ports */
	cdev_lpt_init(NLPT,lpt),	/* 37: parallel printer */
	cdev_notdef(),			/* 38 */
	cdev_notdef(),			/* 39 */
	cdev_notdef(),			/* 40 */
	cdev_notdef(),			/* 41 */
	cdev_notdef(),			/* 42: SMD disk */
	cdev_svr4_net_init(NSVR4_NET,svr4_net),	/* 43: svr4 net pseudo-device */
	cdev_notdef(),			/* 44 */
	cdev_notdef(),			/* 45 */
	cdev_notdef(),			/* 46 */
	cdev_notdef(),			/* 47 */
	cdev_notdef(),			/* 48 */
	cdev_notdef(),			/* 49 */
	cdev_systrace_init(NSYSTRACE,systrace),	/* 50 system call tracing */
#ifdef XFS
	cdev_xfs_init(NXFS,xfs_dev),	/* 51: xfs communication device */
#else
	cdev_notdef(),			/* 51 */
#endif
#ifdef USER_PCICONF
	cdev_pci_init(NPCI,pci),	/* 52: PCI user */
#else
	cdev_notdef(),			/* 52 */
#endif
	cdev_notdef(),			/* 53 */
	cdev_disk_init(NFD,fd),		/* 54: floppy disk */
	cdev_notdef(),			/* 55 */
	cdev_notdef(),			/* 56 */
	cdev_notdef(),			/* 57 */
	cdev_disk_init(NCD,cd),		/* 58: SCSI CD-ROM */
	cdev_scanner_init(NSS,ss),	/* 59: SCSI scanner */
	cdev_uk_init(NUK,uk),		/* 60: SCSI unknown */
	cdev_disk_init(NRD,rd),		/* 61: memory disk */
	cdev_notdef(),			/* 62 */
	cdev_notdef(),			/* 63 */
	cdev_notdef(),			/* 64: /dev/cgeight */
	cdev_notdef(),			/* 65 */
	cdev_notdef(),			/* 66 */
	cdev_notdef(),			/* 67 */
	cdev_notdef(),			/* 68 */
	cdev_audio_init(NAUDIO,audio),	/* 69: /dev/audio */
	cdev_openprom_init(1,openprom),	/* 70: /dev/openprom */
	cdev_tty_init(NMTTY,mtty),	/* 71: magma serial ports */
	cdev_gen_init(NMBPP,mbpp),	/* 72: magma parallel ports */
	cdev_pf_init(NPF,pf),		/* 73: packet filter */
	cdev_notdef(),			/* 74: ALTQ (deprecated) */
	cdev_crypto_init(NCRYPTO,crypto), /* 75: /dev/crypto */
	cdev_ksyms_init(NKSYMS,ksyms),	/* 76 *: Kernel symbols device */
	cdev_tty_init(NSABTTY,sabtty),	/* 77: sab82532 serial ports */
	cdev_wsdisplay_init(NWSDISPLAY,	/* 78: frame buffers, etc. */
	    wsdisplay),
	cdev_mouse_init(NWSKBD, wskbd),	/* 79: keyboards */
	cdev_mouse_init(NWSMOUSE, wsmouse), /* 80: mice */
	cdev_mouse_init(NWSMUX, wsmux),	/* 81: ws multiplexor */
	cdev_notdef(),			/* 82 */
	cdev_notdef(),			/* 83 */
	cdev_notdef(),			/* 84 */
	cdev_notdef(),			/* 85 */
	cdev_notdef(),			/* 86 */
	cdev_notdef(),			/* 87 */
	cdev_notdef(),			/* 88 */
	cdev_notdef(),			/* 89 */
	cdev_usb_init(NUSB,usb),	/* 90: USB controller */
	cdev_usbdev_init(NUHID,uhid),	/* 91: USB generic HID */
	cdev_usbdev_init(NUGEN,ugen),	/* 92: USB generic driver */
	cdev_ulpt_init(NULPT,ulpt),	/* 93: USB printers */
	cdev_urio_init(NURIO,urio),	/* 94: USB Diamond Rio 500 */
	cdev_tty_init(NUCOM,ucom),	/* 95: USB tty */
	cdev_usbdev_init(NUSCANNER,uscanner), /* 96: USB scanners */
	cdev_notdef(),			/* 97 */
	cdev_notdef(),			/* 98 */
	cdev_notdef(),			/* 99 */
	cdev_notdef(),			/* 100 */
	cdev_notdef(),			/* 101 */
	cdev_notdef(),			/* 102 */
	cdev_notdef(),			/* 103 */
	cdev_notdef(),			/* 104 */
	cdev_bpftun_init(NBPFILTER,bpf),/* 105: packet filter */
	cdev_notdef(),			/* 106 */
	cdev_bpp_init(NBPP,bpp),	/* 107: on-board parallel port */
	cdev_tty_init(NSTTY,stty),	/* 108: spif serial ports */
	cdev_gen_init(NSBPP,sbpp),	/* 109: spif parallel ports */
	cdev_disk_init(NVND,vnd),	/* 110: vnode disk driver */
	cdev_bpftun_init(NTUN,tun),	/* 111: network tunnel */
	cdev_lkm_init(NLKM,lkm),	/* 112: loadable module driver */
	cdev_lkm_dummy(),		/* 113 */
	cdev_lkm_dummy(),		/* 114 */
	cdev_lkm_dummy(),		/* 115 */
	cdev_lkm_dummy(),		/* 116 */
	cdev_lkm_dummy(),		/* 117 */
	cdev_lkm_dummy(),		/* 118 */
	cdev_random_init(1,random),	/* 119: random data source */
	cdev_bio_init(NBIO,bio),	/* 120: ioctl tunnel */
	cdev_disk_init(NRAID,raid),	/* 121: RAIDframe disk driver */
	cdev_tty_init(NPCONS,pcons),	/* 122: PROM console */
	cdev_ptm_init(NPTY,ptm),	/* 123: pseudo-tty ptm device */
	cdev_hotplug_init(NHOTPLUG,hotplug), /* 124: devices hot plugging */
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
getnulldev(void)
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
	/*  9 */	NODEV,
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
	/* 25 */	NODEV,
	/* 26 */	12,
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
	/* 61 */	5,
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
	/*106 */	NODEV,
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
	/*121 */	25,
	/*122 */	NODEV,
};
int nchrtoblktbl = sizeof(chrtoblktbl) / sizeof(chrtoblktbl[0]);
