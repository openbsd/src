/*	$OpenBSD: conf.c,v 1.6 2004/12/30 23:41:58 drahn Exp $	*/
/*	$NetBSD: conf.c,v 1.10 2002/04/19 01:04:38 wiz Exp $	*/

/*
 * Copyright (c) 1994-1998 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * conf.c
 *
 * Character and Block Device configuration
 * Console configuration
 *
 * Defines the structures cdevsw and constab
 *
 * Created      : 17/09/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/vnode.h>

#include <machine/conf.h>

#include "inet.h"

/*
 * From this point, these need to be MI foo.h files.
 */

/*
 * Standard MI devices (e.g. ones in dev/ic)
 */
#include "com.h"		/* NS164x0 serial ports */

/*
 * Standard pseudo-devices
 */
#include "bpfilter.h"
#include "pf.h"
#include "pty.h"
#include "tun.h"
#include "ksyms.h"

/*
 * Disk/Filesystem pseudo-devices
 */
#include "ccd.h"			/* concatenated disk driver */
#include "rd.h"				/* memory disk driver */
#include "raid.h"			/* RAIDframe */
#include "vnd.h"			/* vnode disk driver */

/*
 * WD/ATA devices
 */
#include "wd.h"
bdev_decl(wd);
bdev_decl(sw);

#ifdef CONF_HAVE_PCI
#include "iop.h"
#include "pci.h"
#else
#define NIOP	0
#define NMLX	0
#define NMLY	0
#define NPCI	0
#endif
#define	NAGP	0
/*
 * SCSI/ATAPI devices
 */
#include "sd.h"
#include "st.h"
#include "cd.h"
#include "ch.h"
#include "uk.h"
#include "ss.h"

/*
 * Audio devices
 */
#include "audio.h"
#include "midi.h"
#include "sequencer.h"

/*
 * USB devices
 */
#include "usb.h"
#include "ucom.h"
#include "ugen.h"
#include "uhid.h"
#include "ulpt.h"
#include "urio.h"
#include "uscanner.h"

/*
 * WSCONS devices
 */
#include "wsdisplay.h"
/*
#include "wsfont.h"
*/
#include "wskbd.h"
#include "wsmouse.h"
#include "wsmux.h"
cdev_decl(wskbd);
cdev_decl(wsmouse);

#include "lpt.h"
#include "fcom.h"

#include "radio.h"
cdev_decl(radio);

#include <arm/conf.h>

/* Block devices */

struct bdevsw bdevsw[] = {
	bdev_lkm_dummy(),		/*  0: */
	bdev_swap_init(1, sw),		/*  1: swap pseudo-device */
	bdev_lkm_dummy(),		/*  2: */
	bdev_lkm_dummy(),		/*  3: */
	bdev_lkm_dummy(),		/*  4: */
	bdev_lkm_dummy(),		/*  5: */
	bdev_lkm_dummy(),		/*  6: */
	bdev_lkm_dummy(),		/*  7: */
	bdev_lkm_dummy(),		/*  8: */
	bdev_lkm_dummy(),		/*  9: */
	bdev_lkm_dummy(),		/* 10: */
	bdev_lkm_dummy(),		/* 11: */
	bdev_lkm_dummy(),		/* 12: */
	bdev_lkm_dummy(),		/* 13: */
	bdev_lkm_dummy(),		/* 14: */
	bdev_lkm_dummy(),		/* 15: */
	bdev_disk_init(NWD,wd),		/* 16: Internal IDE disk */
	bdev_lkm_dummy(),		/* 17: */
	bdev_disk_init(NRD,rd),		/* 18: memory disk */
	bdev_disk_init(NVND,vnd),	/* 19: vnode disk driver */
	bdev_lkm_dummy(),		/* 20: */
 	bdev_disk_init(NCCD,ccd),	/* 21: concatenated disk driver */
	bdev_lkm_dummy(),		/* 22: */
	bdev_lkm_dummy(),		/* 23: */
	bdev_disk_init(NSD,sd),		/* 24: SCSI disk */
	bdev_tape_init(NST,st),		/* 25: SCSI tape */
	bdev_disk_init(NCD,cd),		/* 26: SCSI cdrom */
	bdev_lkm_dummy(),		/* 27: */
	bdev_lkm_dummy(),		/* 28: */
	bdev_lkm_dummy(),		/* 29: */
	bdev_lkm_dummy(),		/* 30: */
	bdev_lkm_dummy(),		/* 31: */
	bdev_lkm_dummy(),		/* 32: */
	bdev_lkm_dummy(),		/* 33: */
	bdev_lkm_dummy(),		/* 34: */
	bdev_lkm_dummy(),		/* 35: */
	bdev_lkm_dummy(),		/* 36: */
	bdev_lkm_dummy(),		/* 37: */
	bdev_lkm_dummy(),		/* 38: */
	bdev_lkm_dummy(),		/* 39: */
	bdev_lkm_dummy(),		/* 40: */
	bdev_lkm_dummy(),		/* 41: */
	bdev_lkm_dummy(),		/* 42: */
	bdev_lkm_dummy(),		/* 43: */
	bdev_lkm_dummy(),		/* 44: */
	bdev_lkm_dummy(),		/* 45: */
	bdev_lkm_dummy(),		/* 46: */
	bdev_lkm_dummy(),		/* 47: */
	bdev_lkm_dummy(),		/* 48: */
	bdev_lkm_dummy(),		/* 49: */
	bdev_lkm_dummy(),		/* 50: */
	bdev_lkm_dummy(),		/* 51: */
	bdev_lkm_dummy(),		/* 52: */
	bdev_lkm_dummy(),		/* 53: */
	bdev_lkm_dummy(),		/* 54: */
	bdev_lkm_dummy(),		/* 55: */
	bdev_lkm_dummy(),		/* 56: */
	bdev_lkm_dummy(),		/* 57: */
	bdev_lkm_dummy(),		/* 58: */
	bdev_lkm_dummy(),		/* 59: */
	bdev_lkm_dummy(),		/* 60: */
	bdev_lkm_dummy(),		/* 61: */
	bdev_lkm_dummy(),		/* 62: */
	bdev_lkm_dummy(),		/* 63: */
	bdev_lkm_dummy(),		/* 64: */
	bdev_lkm_dummy(),		/* 65: */
	bdev_lkm_dummy(),		/* 66: */
	bdev_lkm_dummy(),		/* 67: */
	bdev_lkm_dummy(),		/* 68: */
	bdev_lkm_dummy(),		/* 69: */
	bdev_lkm_dummy(),		/* 70: */
	bdev_disk_init(NRAID,raid),	/* 71: RAIDframe disk driver */
	bdev_lkm_dummy(),		/* 72: */
	bdev_lkm_dummy(),		/* 73: */
	bdev_lkm_dummy(),		/* 74: */
	bdev_lkm_dummy(),		/* 75: */
	bdev_lkm_dummy(),		/* 76: */
	bdev_lkm_dummy(),		/* 77: */
	bdev_lkm_dummy(),		/* 78: */
	bdev_lkm_dummy(),		/* 79: */
	bdev_lkm_dummy(),		/* 80: */
	bdev_lkm_dummy(),		/* 81: */
	bdev_lkm_dummy(),		/* 82: */
	bdev_lkm_dummy(),		/* 83: */
	bdev_lkm_dummy(),		/* 84: */
	bdev_lkm_dummy(),		/* 85: */
	bdev_lkm_dummy(),		/* 86: */
	bdev_lkm_dummy(),		/* 87: */
	bdev_lkm_dummy(),		/* 88: */
	bdev_lkm_dummy(),		/* 89: */
	bdev_lkm_dummy(),		/* 90: */
	bdev_lkm_dummy(),		/* 91: */
	bdev_lkm_dummy(),		/* 93: */
	bdev_lkm_dummy(),		/* 94: */
	bdev_lkm_dummy(),		/* 95: */
	bdev_lkm_dummy(),		/* 96: */
	bdev_lkm_dummy(),		/* 97: */
};

/* Character devices */
#define ptstty          ptytty
#define ptsioctl        ptyioctl
#define ptctty          ptytty
#define ptcioctl        ptyioctl

#ifdef XFS
#include <xfs/nxfs.h>
cdev_decl(xfs_dev);
#endif
#include "systrace.h"

struct cdevsw cdevsw[] = {
	cdev_cn_init(1,cn),			/*  0: virtual console */
	cdev_ctty_init(1,ctty),			/*  1: controlling terminal */
	cdev_mm_init(1,mm),			/*  2: /dev/{null,mem,kmem,...} */
	cdev_swap_init(1,sw),			/*  3: /dev/drum (swap pseudo-device) */
	cdev_tty_init(NPTY,pts),		/*  4: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),		/*  5: pseudo-tty master */
	cdev_log_init(1,log),			/*  6: /dev/klog */
	cdev_fd_init(1,filedesc),		/*  7: file descriptor pseudo-device */
	cdev_ksyms_init(NKSYMS,ksyms),		/*  8: Kernel symbols device */
	cdev_lpt_init(NLPT,lpt),		/*  9: parallel printer */
	cdev_lkm_dummy(),			/* 10: */
	cdev_lkm_dummy(),			/* 11: */
	cdev_tty_init(NCOM,com),		/* 12: serial port */
	cdev_lkm_dummy(),			/* 13: */
	cdev_lkm_dummy(),			/* 14: */
	cdev_lkm_dummy(),			/* 15: */
	cdev_disk_init(NWD,wd),			/* 16: ST506/ESDI/IDE disk */
	cdev_lkm_dummy(),			/* 17: */
	cdev_disk_init(NRD,rd),			/* 18: ram disk driver */
	cdev_disk_init(NVND,vnd),		/* 19: vnode disk driver */
	cdev_lkm_dummy(),			/* 20: */
	cdev_disk_init(NCCD,ccd),		/* 21: concatenated disk driver */
	cdev_bpftun_init(NBPFILTER,bpf),	/* 22: Berkeley packet filter */
	cdev_lkm_dummy(),			/* 23: */
	cdev_disk_init(NSD,sd),			/* 24: SCSI disk */
	cdev_tape_init(NST,st),			/* 25: SCSI tape */
	cdev_disk_init(NCD,cd),			/* 26: SCSI CD-ROM */
	cdev_ch_init(NCH,ch),	 		/* 27: SCSI autochanger */
	cdev_uk_init(NUK,uk),	 		/* 28: SCSI unknown */
	cdev_scanner_init(NSS,ss),		/* 29: SCSI scanner */
	cdev_lkm_dummy(),			/* 30: */
	cdev_lkm_dummy(),			/* 31: */
	cdev_lkm_dummy(),			/* 32: */
	cdev_bpftun_init(NTUN,tun),		/* 33: network tunnel */
	cdev_lkm_dummy(),			/* 34: */
	cdev_lkm_init(NLKM,lkm),		/* 35: loadable module driver */
	cdev_audio_init(NAUDIO,audio),		/* 36: generic audio I/O */
	cdev_notdef(),				/* 37: removed cpu device */
	cdev_notdef(),				/* 38: removed cpu device */
	cdev_lkm_dummy(),			/* 39: reserved */
	cdev_lkm_dummy(),			/* 40: reserved */
	cdev_lkm_dummy(),			/* 41: reserved */
	cdev_lkm_dummy(),			/* 42: reserved */
	cdev_lkm_dummy(),			/* 43: reserved */
	cdev_lkm_dummy(),			/* 44: reserved */
	cdev_lkm_dummy(),			/* 45: reserved */
	cdev_pf_init(NPF,pf),           	/* 46: packet filter */
	cdev_crypto_init(NCRYPTO,crypto), 	/* 47: /dev/crypto */
	cdev_lkm_dummy(),			/* 48: reserved */
	cdev_lkm_dummy(),			/* 49: reserved */
	cdev_systrace_init(NSYSTRACE,systrace),	/* 50: system call tracing */
#ifdef XFS
	cdev_xfs_init(NXFS,xfs_dev),		/* 51: xfs communication device */
#else
	cdev_notdef(),				/* 51: reserved */
#endif
	cdev_notdef(),				/* 52: reserved */
	cdev_notdef(),				/* 53: reserved */
	cdev_tty_init(NFCOM,fcom),		/* 54: FOOTBRIDGE console */
	cdev_lkm_dummy(),			/* 55: Reserved for bypass device */	
	cdev_notdef(),				/* 56: reserved */
	cdev_midi_init(NMIDI,midi),		/* 57: MIDI I/O */
	cdev_midi_init(NSEQUENCER,sequencer),	/* 58: sequencer I/O */
	cdev_notdef(),				/* 59: reserved */
	cdev_wsdisplay_init(NWSDISPLAY,wsdisplay), /* 60: frame buffers, etc.*/
	cdev_mouse_init(NWSKBD,wskbd),		/* 61: keyboards */
	cdev_mouse_init(NWSMOUSE,wsmouse),	/* 62: mice */
	cdev_mouse_init(NWSMUX,wsmux),		/* 63: ws multiplexor */
	cdev_usb_init(NUSB,usb),		/* 64: USB controller */
	cdev_usbdev_init(NUHID,uhid),		/* 65: USB generic HID */
	cdev_lpt_init(NULPT,ulpt),		/* 66: USB printer */
	cdev_urio_init(NURIO,urio),		/* 67: Diamond Rio 500 */
	cdev_tty_init(NUCOM,ucom),		/* 68: USB tty */
	cdev_usbdev_init(NUSCANNER,uscanner),	/* 69: USB scanner */
	cdev_usbdev_init(NUGEN,ugen),		/* 70: USB generic driver */
	cdev_disk_init(NRAID,raid),    		/* 71: RAIDframe disk driver */
	cdev_lkm_dummy(),			/* 72: reserved */
	cdev_lkm_dummy(),			/* 73: reserved */
	cdev_lkm_dummy(),			/* 74: reserved */
	cdev_lkm_dummy(),			/* 75: reserved */
	cdev_lkm_dummy(),			/* 76: reserved */
	cdev_notdef(),                          /* 77: removed device */
	cdev_notdef(),                          /* 78: removed device */
	cdev_notdef(),                          /* 79: removed device */
	cdev_notdef(),                          /* 80: removed device */
	cdev_notdef(),                          /* 81: removed device */
	cdev_notdef(),                          /* 82: removed device */
	cdev_notdef(),                          /* 83: removed device */
	cdev_notdef(),                          /* 84: removed device */
	cdev_notdef(),                          /* 85: removed device */
	cdev_notdef(),                          /* 86: removed device */
	cdev_notdef(),                          /* 87: removed device */
	cdev_notdef(),                          /* 88: removed device */
	cdev_notdef(),                          /* 89: removed device */
	cdev_notdef(),                          /* 90: removed device */
	cdev_notdef(),                          /* 91: removed device */
	cdev_notdef(),                          /* 92: removed device */
	cdev_notdef(),                          /* 93: removed device */
	cdev_notdef(),                          /* 94: removed device */
	cdev_notdef(),                          /* 95: removed device */
	cdev_notdef(),                          /* 96: removed device */
	cdev_radio_init(NRADIO,radio),		/* 97: generic radio I/O */
	cdev_ptm_init(NPTY,ptm),		/* 98: pseudo-tty ptm device */
};

int nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);
int nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

int mem_no = 2; 	/* major device number of memory special file */

/*
 * Swapdev is a fake device implemented
 * in sw.c used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */
dev_t	swapdev = makedev(1, 0);

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
	return (major(dev) == mem_no && minor(dev) == 3);
}


int chrtoblktbl[] = {
/* XXXX This needs to be dynamic for LKMs. */
    /*VCHR*/        /*VBLK*/
    /*  0 */        NODEV,
    /*  1 */        1,
    /*  2 */        NODEV,
    /*  3 */        NODEV,
    /*  4 */        NODEV,
    /*  5 */        NODEV,
    /*  6 */        NODEV,
    /*  7 */        NODEV,
    /*  8 */        NODEV,
    /*  9 */        NODEV,
    /* 10 */        NODEV,
    /* 11 */        NODEV,
    /* 12 */        NODEV,
    /* 13 */        NODEV,
    /* 14 */        NODEV,
    /* 15 */        NODEV,
    /* 16 */        16,
    /* 17 */        17,
    /* 18 */        18,
    /* 19 */        19,
    /* 20 */        NODEV,
    /* 21 */        21,
    /* 22 */        NODEV,
    /* 23 */        NODEV,
    /* 24 */        24,
    /* 25 */        25,
    /* 26 */        26,
    /* 27 */        NODEV,
    /* 28 */        NODEV,
    /* 29 */        NODEV,
    /* 30 */        NODEV,
    /* 31 */        NODEV,
    /* 32 */        NODEV,
    /* 33 */        NODEV,
    /* 34 */        NODEV,
    /* 35 */        NODEV,
    /* 36 */        NODEV,
    /* 37 */        NODEV,
    /* 38 */        NODEV,
    /* 39 */        NODEV,
    /* 40 */        NODEV,
    /* 41 */        NODEV,
    /* 42 */        NODEV,
    /* 43 */        NODEV,
    /* 44 */        NODEV,
    /* 45 */        NODEV,
    /* 46 */        NODEV,
    /* 47 */        NODEV,
    /* 48 */        NODEV,
    /* 49 */        NODEV,
    /* 50 */        NODEV,
    /* 51 */        NODEV,
    /* 52 */        NODEV,
    /* 53 */        NODEV,
    /* 54 */        NODEV,
    /* 55 */        NODEV,
    /* 56 */	    NODEV,
    /* 57 */	    NODEV,
    /* 58 */	    NODEV,
    /* 59 */        NODEV,
    /* 60 */        NODEV,
    /* 61 */        NODEV,
    /* 62 */        NODEV,
    /* 63 */        NODEV,
    /* 64 */        NODEV,
    /* 65 */        NODEV,
    /* 66 */	    NODEV,
    /* 67 */	    NODEV,
    /* 68 */	    NODEV,
    /* 69 */	    NODEV,
    /* 70 */	    NODEV,
    /* 71 */	    71,
    /* 72 */	    NODEV,
    /* 73 */	    NODEV,
    /* 74 */	    NODEV,
    /* 75 */	    NODEV,
    /* 76 */	    NODEV,
    /* 77 */	    NODEV,
    /* 78 */	    NODEV,
    /* 79 */	    NODEV,
    /* 80 */	    NODEV,
    /* 81 */	    NODEV,
    /* 82 */	    NODEV,
    /* 83 */	    NODEV,
    /* 84 */	    NODEV,
    /* 85 */	    NODEV,
    /* 86 */	    NODEV,
    /* 87 */	    NODEV,
    /* 88 */	    NODEV,
    /* 89 */	    NODEV,
    /* 90 */	    NODEV,
    /* 91 */	    NODEV,
    /* 92 */	    92,
    /* 93 */	    NODEV,
    /* 94 */	    NODEV,
    /* 95 */	    NODEV,
    /* 96 */	    NODEV,
    /* 97 */	    NODEV,
};
int nchrtoblktbl = sizeof(chrtoblktbl) / sizeof(chrtoblktbl[0]);


dev_t
getnulldev()
{
	return makedev(mem_no, 2);
}
