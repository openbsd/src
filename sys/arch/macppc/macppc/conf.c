/*	$OpenBSD: conf.c,v 1.19 2002/09/15 09:01:58 deraadt Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
 * Copyright (c) 1997 RTMX Inc, North Carolina
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
 *	This product includes software developed under OpenBSD for RTMX Inc,
 *	North Carolina, USA, by Per Fogelstrom, Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include <machine/conf.h>

#include "sd.h"
#include "st.h"
#include "cd.h"
#include "ss.h"
#include "uk.h"
#include "rd.h"
#include "wd.h"
bdev_decl(wd);
cdev_decl(wd);

#include "ofdisk.h"
bdev_decl(ofd);

#include "vnd.h"
#include "ccd.h"
#include "raid.h"

struct bdevsw bdevsw[] = {
	bdev_disk_init(NWD,wd),		/* 0: ST506/ESDI/IDE disk */
	bdev_swap_init(1,sw),		/* 1 swap pseudo device */
	bdev_disk_init(NSD,sd),		/* 2 SCSI Disk */
	bdev_disk_init(NCD,cd),		/* 3 SCSI CD-ROM */
	bdev_disk_init(NOFDISK,ofd),	/* 4 Openfirmware disk */
	bdev_notdef(),			/* 5 unknown*/
	bdev_notdef(),			/* 6 unknown*/
	bdev_notdef(),			/* 7 unknown*/
	bdev_lkm_dummy(),		/* 8 */
	bdev_lkm_dummy(),		/* 9 */
	bdev_lkm_dummy(),		/* 10 */
	bdev_lkm_dummy(),		/* 11 */
	bdev_lkm_dummy(),		/* 12 */
	bdev_lkm_dummy(),		/* 13 */
	bdev_disk_init(NVND,vnd),	/* 14 vnode disk driver*/
	bdev_notdef(),			/* 15 unknown*/
	bdev_disk_init(NCCD,ccd),	/* 16 concatenated disk driver*/
	bdev_disk_init(NRD,rd),		/* 17 ram disk driver*/
	bdev_notdef(),			/* 18 unknown*/
	bdev_disk_init(NRAID,raid),	/* 19: RAIDframe disk driver */
};
int nblkdev = sizeof bdevsw / sizeof bdevsw[0];

#include "pty.h"
#include "zstty.h"

#include "ofcons.h"
cdev_decl(ofc);
cdev_decl(ofd);

#include "ofrtc.h"
cdev_decl(ofrtc);

cdev_decl(kbd);
cdev_decl(ms);

#include "wsdisplay.h"
#include "wskbd.h"
#include "wsmouse.h"

#include "iop.h"

#include "bpfilter.h"

#include "tun.h"

#ifdef XFS
#include <xfs/nxfs.h>
cdev_decl(xfs_dev);
#endif

#ifdef LKM
#define NLKM 1
#else
#define NLKM 0
#endif

#include "ksyms.h"
#include "usb.h"
#include "uhid.h"
#include "ugen.h"
#include "ulpt.h"
#include "urio.h"
#include "ucom.h"
#include "uscanner.h"

#include "inet.h"

#include "apm.h"

#include "wsmux.h"

#ifdef USER_PCICONF
#include "pci.h"
cdev_decl(pci);
#endif

#include "audio.h"

#include "pf.h"

#include <altq/altqconf.h>

#include "systrace.h"

struct cdevsw cdevsw[] = {
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_ctty_init(1,ctty),		/* 1: controlling terminal */
	cdev_mm_init(1,mm),		/* 2: /dev/{null,mem,kmem,...} */
	cdev_swap_init(1,sw),		/* 3: /dev/drum (swap pseudo-device) */
	cdev_tty_init(NPTY,pts),	/* 4: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 5: pseudo-tty master */
	cdev_log_init(1,log),		/* 6: /dev/klog */
	cdev_tty_init(NZSTTY,zs),	/* 7: Serial ports */
	cdev_disk_init(NSD,sd),		/* 8: SCSI disk */
	cdev_disk_init(NCD,cd),		/* 9: SCSI CD-ROM */
	cdev_notdef(),			/* 10: SCSI changer */
	cdev_disk_init(NWD,wd),		/* 11: ST506/ESDI/IDE disk */
	cdev_notdef(),			/* 12 */
	cdev_disk_init(NOFDISK,ofd),	/* 13 Openfirmware disk */
	cdev_tty_init(NOFCONS,ofc),	/* 14 Openfirmware console */
	cdev_notdef(),			/* 15 */
	cdev_notdef(),			/* 16 */
	cdev_disk_init(NRD,rd),		/* 17 ram disk driver*/
	cdev_disk_init(NCCD,ccd),	/* 18 concatenated disk driver */
	cdev_disk_init(NVND,vnd),	/* 19: vnode disk */
	cdev_tape_init(NST,st),		/* 20: SCSI tape */
	cdev_fd_init(1,filedesc),	/* 21: file descriptor pseudo-dev */
	cdev_bpftun_init(NBPFILTER,bpf),/* 22: berkeley packet filter */
	cdev_bpftun_init(NTUN,tun),	/* 23: network tunnel */
	cdev_lkm_init(NLKM,lkm),	/* 24: loadable module driver */
        cdev_apm_init(NAPM,apm),	/* 25: APM interface */
	cdev_notdef(),			/* 26 */
	cdev_notdef(),			/* 27 */
	cdev_notdef(),			/* 28 */
	cdev_notdef(),			/* 29 */
	cdev_notdef(),			/* 30 */
	cdev_notdef(),			/* 31 */
	cdev_notdef(),			/* 32 */
	cdev_lkm_dummy(),		/* 33 */
	cdev_lkm_dummy(),		/* 34 */
	cdev_lkm_dummy(),		/* 35 */
	cdev_lkm_dummy(),		/* 36 */
	cdev_lkm_dummy(),		/* 37 */
	cdev_lkm_dummy(),		/* 38 */
	cdev_pf_init(NPF,pf),		/* 39: packet filter */
	cdev_random_init(1,random),	/* 40: random data source */
	cdev_uk_init(NUK,uk),		/* 41: unknown SCSI */
	cdev_ss_init(NSS,ss),		/* 42: SCSI scanner */
	cdev_ksyms_init(NKSYMS,ksyms),	/* 43: Kernel symbols device */
	cdev_audio_init(NAUDIO,audio),	/* 44: generic audio I/O */
	cdev_notdef(),			/* 45 */
	cdev_notdef(),			/* 46 */
	cdev_crypto_init(NCRYPTO,crypto), /* 47: /dev/crypto */
	cdev_notdef(),			/* 48 */
	cdev_notdef(),			/* 49 */
	cdev_systrace_init(NSYSTRACE,systrace),	/* 50 system call tracing */
#ifdef XFS
	cdev_xfs_init(NXFS,xfs_dev),	/* 51: xfs communication device */
#else
	cdev_notdef(),			/* 51 */
#endif
	cdev_notdef(),			/* 52 */
	cdev_notdef(),			/* 53 */
	cdev_disk_init(NRAID,raid),	/* 54: RAIDframe disk driver */
	cdev_notdef(),			/* 55 */
	/* The following slots are reserved for isdn4bsd. */
	cdev_notdef(),			/* 56: i4b main device */
	cdev_notdef(),			/* 57: i4b control device */
	cdev_notdef(),			/* 58: i4b raw b-channel access */
	cdev_notdef(),			/* 59: i4b trace device */
	cdev_notdef(),			/* 60: i4b phone device */
	/* End of reserved slots for isdn4bsd. */
	cdev_usb_init(NUSB,usb),	/* 61: USB controller */
	cdev_usbdev_init(NUHID,uhid),	/* 62: USB generic HID */
	cdev_usbdev_init(NUGEN,ugen),	/* 63: USB generic driver */
	cdev_ulpt_init(NULPT,ulpt),	/* 64: USB printers */
	cdev_usbdev_init(NURIO,urio),	/* 65: USB Diamond Rio 500 */
	cdev_tty_init(NUCOM,ucom),	/* 66: USB tty */
	cdev_wsdisplay_init(NWSDISPLAY,	/* 67: frame buffers, etc. */
		wsdisplay),
	cdev_mouse_init(NWSKBD, wskbd),	/* 68: keyboards */
	cdev_mouse_init(NWSMOUSE,	/* 69: mice */
		wsmouse),
	cdev_mouse_init(NWSMUX, wsmux),	/* 70: ws multiplexor */
#ifdef USER_PCICONF
	cdev_pci_init(NPCI,pci),	/* 71: PCI user */
#else
	cdev_notdef(),
#endif
	cdev_altq_init(NALTQ,altq),	/* 72: ALTQ control interface */
	cdev_iop_init(NIOP,iop),	/* 73: I2O IOP control interface */
	cdev_usbdev_init(NUSCANNER,uscanner), /* 74: usb scanner */
};
int nchrdev = sizeof cdevsw / sizeof cdevsw[0];

int mem_no = 2;				/* major number of /dev/mem */

/*
 * Swapdev is a fake device implemented in sw.c.
 * It is used only internally to get to swstrategy.
 */
dev_t swapdev = makedev(1, 0);

/*
 * Check whether dev is /dev/mem or /dev/kmem.
 */
int
iskmemdev(dev)
	dev_t dev;
{
	return major(dev) == mem_no && minor(dev) < 2;
}

/*
 * Check whether dev is /dev/zero.
 */
int
iszerodev(dev)
	dev_t dev;
{
	return major(dev) == mem_no && minor(dev) == 12;
}

dev_t
getnulldev()
{
	return makedev(mem_no, 2);
}

static int chrtoblktbl[] = {
	/*VCHR*/	/*VBLK*/
	/*  0 */	NODEV,
	/*  1 */	NODEV,
	/*  2 */	NODEV,
	/*  3 */	NODEV,
	/*  4 */	NODEV,
	/*  5 */	NODEV,
	/*  6 */	NODEV,
	/*  7 */	NODEV,
	/*  8 */	2,
	/*  9 */	NODEV,
	/* 10 */	NODEV,
	/* 11 */	0,
	/* 12 */	NODEV,
	/* 13 */	4,
	/* 14 */	NODEV,
	/* 15 */	NODEV,
	/* 16 */	NODEV,
	/* 17 */	17,
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
	/* 47 */	NODEV,
	/* 48 */	NODEV,
	/* 49 */	NODEV,
	/* 50 */	NODEV,
	/* 51 */	NODEV,
	/* 52 */	NODEV,
	/* 53 */	NODEV,
	/* 54 */	19,
};

/*
 * Return accompanying block dev for a char dev.
 */
int
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

#include <dev/cons.h>
#include "vgafb_pci.h"

cons_decl(ws);
cons_decl(zs);
cons_decl(ofc);

struct consdev constab[] = {
#if NWSDISPLAY > 0
	cons_init(ws),
#endif
#if NZSTTY > 0
	cons_init(zs),
#endif
#if NOFCONS > 0
	cons_init(ofc),
#endif
	{ 0 },
};
