/*	$OpenBSD: conf.c,v 1.48 1998/08/31 05:16:53 art Exp $	*/
/*	$NetBSD: conf.c,v 1.75 1996/05/03 19:40:20 christos Exp $	*/

/*
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include <machine/conf.h>

#include "wdc.h"
#include "wd.h"
bdev_decl(wd);
bdev_decl(sw);
#include "fdc.h"
#include "fd.h"
bdev_decl(fd);
#include "wt.h"
bdev_decl(wt);
#include "sd.h"
#include "st.h"
#include "cd.h"
#include "uk.h"
#include "acd.h"
bdev_decl(acd);
#include "mcd.h"
bdev_decl(mcd);
#include "vnd.h"
#include "scd.h"
bdev_decl(scd);
#include "ccd.h"
#include "rd.h"
bdev_decl(rd);
cdev_decl(rd);

struct bdevsw	bdevsw[] =
{
	bdev_disk_init(NWD,wd),		/* 0: ST506/ESDI/IDE disk */
	bdev_swap_init(1,sw),		/* 1: swap pseudo-device */
	bdev_disk_init(NFD,fd),		/* 2: floppy diskette */
	bdev_tape_init(NWT,wt),		/* 3: QIC-02/QIC-36 tape */
	bdev_disk_init(NSD,sd),		/* 4: SCSI disk */
	bdev_tape_init(NST,st),		/* 5: SCSI tape */
	bdev_disk_init(NCD,cd),		/* 6: SCSI CD-ROM */
	bdev_disk_init(NMCD,mcd),	/* 7: Mitsumi CD-ROM */
	bdev_lkm_dummy(),		/* 8 */
	bdev_lkm_dummy(),		/* 9 */
	bdev_lkm_dummy(),		/* 10 */
	bdev_lkm_dummy(),		/* 11 */
	bdev_lkm_dummy(),		/* 12 */
	bdev_lkm_dummy(),		/* 13 */
	bdev_disk_init(NVND,vnd),	/* 14: vnode disk driver */
	bdev_disk_init(NSCD,scd),	/* 15: Sony CD-ROM */
	bdev_disk_init(NCCD,ccd),	/* 16: concatenated disk driver */
	bdev_disk_init(NRD,rd),		/* 17: ram disk driver */
	bdev_disk_init(NACD,acd),	/* 18: ATAPI CD-ROM */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

/* open, close, read, write, ioctl, tty, mmap */
#define cdev_pc_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_init(c,n,stop), \
	dev_init(c,n,tty), ttselect, dev_init(c,n,mmap), D_TTY }

/* open, close, write, ioctl */
#define	cdev_lpt_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev }

/* open, close, write, ioctl */
#define	cdev_spkr_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev }

/* open, close, read, ioctl */
#define cdev_joy_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, seltrue, \
	(dev_type_mmap((*))) enodev }

/* open, close, ioctl, select -- XXX should be a generic device */
#define cdev_ocis_init(c,n) { \
        dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
        (dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
        (dev_type_stop((*))) enodev, 0,  dev_init(c,n,select), \
        (dev_type_mmap((*))) enodev, 0 }

/* open, close, read, ioctl, mmap */
#define	cdev_bktr_init(c, n) { \
        dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
        (dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, seltrue, \
        dev_init(c,n,mmap) }


#define	mmread	mmrw
#define	mmwrite	mmrw
cdev_decl(mm);
cdev_decl(wd);
cdev_decl(sw);
#include "pty.h"
#include "com.h"
#include "pccom.h"
cdev_decl(com);
cdev_decl(fd);
cdev_decl(wt);
cdev_decl(scd);
#include "pc.h"
#include "vt.h"
cdev_decl(pc);
#include "ss.h"
cdev_decl(acd);
#include "lpt.h"
cdev_decl(lpt);
#include "ch.h"
dev_decl(filedesc,open);
#include "bpfilter.h"
#include "pcmcia.h"
cdev_decl(pcmcia);
#include "spkr.h"
cdev_decl(spkr);
#include "mms.h"
cdev_decl(mms);
#include "lms.h"
cdev_decl(lms);
#include "pms.h"
cdev_decl(pms);
#include "cy.h"
cdev_decl(cy);
cdev_decl(mcd);
#include "tun.h"
#include "audio.h"
cdev_decl(audio);
cdev_decl(svr4_net);
#include "joy.h"
#include "apm.h"
#include "pctr.h"
#include "bios.h"
#ifdef XFS
#include <xfs/nxfs.h>
cdev_decl(xfs_dev);
#endif
#include "bktr.h"
cdev_decl(bktr);
#include "ksyms.h"
cdev_decl(ksyms);   

#ifdef IPFILTER
#define NIPF 1
#else
#define NIPF 0
#endif

/* XXX -- this needs to be supported by config(8)! */
#if (NCOM > 0) && (NPCCOM > 0)
#error com and pccom are mutually exclusive.  Sorry.
#endif
#if (NVT > 0) && (NPC > 0)
#error vt and pc are mutually exclusive.  Sorry.
#endif


struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_ctty_init(1,ctty),		/* 1: controlling terminal */
	cdev_mm_init(1,mm),		/* 2: /dev/{null,mem,kmem,...} */
	cdev_disk_init(NWD,wd),		/* 3: ST506/ESDI/IDE disk */
	cdev_swap_init(1,sw),		/* 4: /dev/drum (swap pseudo-device) */
	cdev_tty_init(NPTY,pts),	/* 5: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 6: pseudo-tty master */
	cdev_log_init(1,log),		/* 7: /dev/klog */
#if NPCCOM > 0
	cdev_tty_init(NPCCOM,com),	/* 8: serial port */
#else
	cdev_tty_init(NCOM,com),	/* 8: serial port */
#endif
	cdev_disk_init(NFD,fd),		/* 9: floppy disk */
	cdev_tape_init(NWT,wt),		/* 10: QIC-02/QIC-36 tape */
	cdev_disk_init(NSCD,scd),	/* 11: Sony CD-ROM */
	cdev_pc_init(NPC + NVT,pc),	/* 12: PC console */
	cdev_disk_init(NSD,sd),		/* 13: SCSI disk */
	cdev_tape_init(NST,st),		/* 14: SCSI tape */
	cdev_disk_init(NCD,cd),		/* 15: SCSI CD-ROM */
	cdev_lpt_init(NLPT,lpt),	/* 16: parallel printer */
	cdev_ch_init(NCH,ch),		/* 17: SCSI autochanger */
	cdev_disk_init(NCCD,ccd),	/* 18: concatenated disk driver */
	cdev_ss_init(NSS,ss),           /* 19: SCSI scanner */
	cdev_uk_init(NUK,uk),		/* 20: unknown SCSI */
	cdev_ocis_init(NAPM,apm),	/* 21: Advancded Power Management */
	cdev_fd_init(1,filedesc),	/* 22: file descriptor pseudo-device */
	cdev_bpftun_init(NBPFILTER,bpf),/* 23: Berkeley packet filter */
	cdev_disk_init(NACD,acd),	/* 24: ATAPI CD-ROM */
	cdev_ocis_init(NPCMCIA,pcmcia), /* 25: PCMCIA Bus */
	cdev_joy_init(NJOY,joy),        /* 26: joystick */
	cdev_spkr_init(NSPKR,spkr),	/* 27: PC speaker */
	cdev_lkm_init(NLKM,lkm),	/* 28: loadable module driver */
	cdev_lkm_dummy(),		/* 29 */
	cdev_lkm_dummy(),		/* 30 */
	cdev_lkm_dummy(),		/* 31 */
	cdev_lkm_dummy(),		/* 32 */
	cdev_lkm_dummy(),		/* 33 */
	cdev_lkm_dummy(),		/* 34 */
	cdev_mouse_init(NMMS,mms),	/* 35: Microsoft mouse */
	cdev_mouse_init(NLMS,lms),	/* 36: Logitech mouse */
	cdev_mousewr_init(NPMS,pms),    /* 37: Extended PS/2 mouse */
	cdev_tty_init(NCY,cy),		/* 38: Cyclom serial port */
	cdev_disk_init(NMCD,mcd),	/* 39: Mitsumi CD-ROM */
	cdev_bpftun_init(NTUN,tun),	/* 40: network tunnel */
	cdev_disk_init(NVND,vnd),	/* 41: vnode disk driver */
	cdev_audio_init(NAUDIO,audio),	/* 42: generic audio I/O */
#ifdef COMPAT_SVR4
	cdev_svr4_net_init(1,svr4_net),	/* 43: svr4 net pseudo-device */
#else
	cdev_notdef(),			/* 43 */
#endif
	cdev_gen_ipf(NIPF,ipl),         /* 44: ip filtering */
	cdev_random_init(1,random),	/* 45: random data source */
	cdev_ocis_init(NPCTR,pctr),	/* 46: pentium performance counters */
	cdev_disk_init(NRD,rd),		/* 47: ram disk driver */
	cdev_ocis_init(NBIOS,bios),	/* 48: onboard BIOS PROM */
	cdev_bktr_init(NBKTR,bktr),     /* 49: Bt848 video capture device */
	cdev_ksyms_init(NKSYMS,ksyms),	/* 50: Kernel symbols device */
#ifdef XFS
	cdev_xfs_init(NXFS,xfs_dev),	/* 51: xfs communication device */
#else
	cdev_notdef(),			/* 51 */
#endif
};
int	nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

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
dev_t	swapdev = makedev(1, 0);

/*
 * Returns true if dev is /dev/mem or /dev/kmem.
 */
int
iskmemdev(dev)
	dev_t dev;
{

	return (major(dev) == mem_no && (minor(dev) < 2 || minor(dev) == 14));
}

/*
 * Returns true if dev is /dev/zero.
 */
int
iszerodev(dev)
	dev_t dev;
{

	return (major(dev) == mem_no && minor(dev) == 12);
}

static int chrtoblktbl[] = {
	/* XXXX This needs to be dynamic for LKMs. */
	/*VCHR*/	/*VBLK*/
	/*  0 */	NODEV,
	/*  1 */	NODEV,
	/*  2 */	NODEV,
	/*  3 */	0,
	/*  4 */	NODEV,
	/*  5 */	NODEV,
	/*  6 */	NODEV,
	/*  7 */	NODEV,
	/*  8 */	NODEV,
	/*  9 */	2,
	/* 10 */	3,
	/* 11 */	15,
	/* 12 */	NODEV,
	/* 13 */	4,
	/* 14 */	5,
	/* 15 */	6,
	/* 16 */	NODEV,
	/* 17 */	NODEV,
	/* 18 */	16,
	/* 19 */	NODEV,
	/* 20 */	NODEV,
	/* 21 */	NODEV,
	/* 22 */	NODEV,
	/* 23 */	NODEV,
	/* 24 */	18,
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
	/* 39 */	7,
	/* 40 */	NODEV,
	/* 41 */	14,
	/* 42 */	NODEV,
	/* 43 */	NODEV,
	/* 44 */	NODEV,
	/* 45 */	NODEV,
	/* 46 */	NODEV,
	/* 47 */	17,
};

/*
 * Convert a character device number to a block device number.
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
 * Convert a block device number to a character device number.
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

/*
 * In order to map BSD bdev numbers of disks to their BIOS equivalents
 * we use several heuristics, one being using checksums of the first
 * few blocks of a disk to get a signature we can match with /boot's
 * computed signatures.  To know where from to read, we must provide a
 * disk driver name -> bdev major number table, which follows.
 * Note: floppies are not included as those are differentiated by the BIOS.
 */
static struct {
	char *name;
	int maj;
} disk_maj[] = {
	{ "wd", 0 },
	{ "sd", 4 },
#if 0
	/* XXX It's not clear at all that recognizing these will help us */
	{ "acd", 18 },
	{ "cd", 6 },
	{ "mcd", 7 },		/* XXX I wonder if any BIOSes support this */
	{ "scd", 15 }		/* 	-	   "		-	   */
#endif
};

dev_t dev_rawpart __P((struct device *));	/* XXX */

dev_t
dev_rawpart(dv)
	struct device *dv;
{
	int i;

	for (i = 0; i < sizeof disk_maj / sizeof disk_maj[0]; i++)
		if (strcmp(dv->dv_cfdata->cf_driver->cd_name,
		    disk_maj[i].name) == 0)
			return (MAKEDISKDEV(disk_maj[i].maj, dv->dv_unit,
			    RAW_PART));
	return (NODEV);
}

/*
 * This entire table could be autoconfig()ed but that would mean that
 * the kernel's idea of the console would be out of sync with that of
 * the standalone boot.  I think it best that they both use the same
 * known algorithm unless we see a pressing need otherwise.
 */
#include <dev/cons.h>

cons_decl(pc);
cons_decl(com);

struct	consdev constab[] = {
#if NPC + NVT > 0
	cons_init(pc),
#endif
#if NCOM + NPCCOM > 0
	cons_init(com),
#endif
	{ 0 },
};
