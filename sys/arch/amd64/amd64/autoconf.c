/*	$OpenBSD: autoconf.c,v 1.10 2005/12/13 00:18:19 jsg Exp $	*/
/*	$NetBSD: autoconf.c,v 1.1 2003/04/26 18:39:26 fvdl Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)autoconf.c	7.1 (Berkeley) 5/9/91
 */

/*
 * Setup the system to run on the current machine.
 *
 * cpu_configure() is called at boot time and initializes the vba 
 * device tables and the memory controller monitoring.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/dkio.h>
#include <sys/reboot.h>

#include <machine/pte.h>
#include <machine/cpu.h>

#include <dev/cons.h>

#include "ioapic.h"
#include "lapic.h"

#if NIOAPIC > 0
#include <machine/i82093var.h>
#endif

#if NLAPIC > 0
#include <machine/i82489var.h>
#endif

void setroot(void);
void rootconf(void);
void swapconf(void);
void diskconf(void);
int findblkmajor(struct device *);
char *findblkname(int);
struct device * parsedisk(char *, int, int, dev_t *);

#if 0
#include "bios32.h"
#if NBIOS32 > 0
#include <machine/bios32.h>
#endif
#endif

int	cold = 1;	/* if 1, still working on cold-start */
struct device *booted_device;
int booted_partition;
extern dev_t bootdev;

#ifdef RAMDISK_HOOKS
static struct device fakerdrootdev = { DV_DISK, {}, NULL, 0, "rd0", NULL };
#endif

/*
 * Determine i/o configuration for a machine.
 */
void
cpu_configure(void)
{
#if NBIOS32 > 0
	bios32_init();
#endif

	x86_64_proc0_tss_ldt_init();

	startrtclock();

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("configure: mainbus not configured");

	intr_printconfig();

#if NIOAPIC > 0
	lapic_set_lvt();
	ioapic_enable();
#endif

#ifdef MULTIPROCESSOR
	cpu_init_idle_pcbs();
#endif

	lcr8(0);
	spl0();
	cold = 0;

	md_diskconf = diskconf;
}

void
diskconf(void)
{
	/* Checksum disks, same as /boot did, then fixup *dev vars */
	dkcsumattach();

	rootconf();
	swapconf();
	dumpconf();
}

void
swapconf(void)
{
	struct swdevt *swp;
	int nblks;

	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		if (bdevsw[major(swp->sw_dev)].d_psize) {
			nblks =
			    (*bdevsw[major(swp->sw_dev)].d_psize)(swp->sw_dev);
			if (nblks != -1 &&
			    (swp->sw_nblks == 0 || swp->sw_nblks > nblks))
				swp->sw_nblks = nblks;
			swp->sw_nblks = ctod(dtoc(swp->sw_nblks));
		}
	}
}

struct device *
parsedisk(str, len, defpart, devp)
	char *str;
	int len, defpart;
	dev_t *devp;
{
	struct device *dv;
	char *cp, c;
	int majdev, unit, part;

	if (len == 0)
		return (NULL);
	cp = str + len - 1;
	c = *cp;
	if (c >= 'a' && (c - 'a') < MAXPARTITIONS) {
		part = c - 'a';
		*cp = '\0';
	} else
		part = defpart;

#ifdef RAMDISK_HOOKS
	if (strcmp(str, fakerdrootdev.dv_xname) == 0) {
		dv = &fakerdrootdev;
		goto gotdisk;
	}
#endif

	TAILQ_FOREACH(dv, &alldevs, dv_list) {
		if (dv->dv_class == DV_DISK &&
		    strcmp(str, dv->dv_xname) == 0) {
#ifdef RAMDISK_HOOKS
gotdisk:
#endif
			majdev = findblkmajor(dv);
			unit = dv->dv_unit;
			if (majdev < 0)
				panic("parsedisk");
			*devp = MAKEDISKDEV(majdev, unit, part);
			break;
		}
#ifdef NFSCLIENT
		if (dv->dv_class == DV_IFNET &&
		    strcmp(str, dv->dv_xname) == 0) {
			*devp = NODEV;
			break;
		}
#endif
	}

	*cp = c;
	return (dv);
}

/* XXX */
static	struct nam2blk {
	char *name;
	int  maj;
} nam2blk[] = {
	{ "wd",		0 },	/* 0 = wd */
	{ "sd",		4 },	/* 2 = sd */
	{ "rd",		17 },	/* 17 = rd */
	{ "raid",	19 },	/* 19 = raid */
};

int
findblkmajor(struct device *dv)
{
	char *name = dv->dv_xname;
	int i;

	for (i = 0; i < sizeof(nam2blk)/sizeof(nam2blk[0]); ++i)
		if (strncmp(name, nam2blk[i].name, strlen(nam2blk[i].name)) == 0)
			return (nam2blk[i].maj);
	 return (-1);
}

char *
findblkname(maj)
	int maj;
{
	int i;

	for (i = 0; i < sizeof(nam2blk)/sizeof(nam2blk[0]); i++)
		if (nam2blk[i].maj == maj)
			return (nam2blk[i].name);
	 return (NULL);
}

/* Code from here to handle "bsd swap generic" */

dev_t	argdev = NODEV;

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
void
setroot()
{
	int  majdev, mindev, unit, part, adaptor;
	dev_t orootdev;
#ifdef DOSWAP
	dev_t temp = 0;
	struct swdevt *swp;
#endif

	if (boothowto & RB_DFLTROOT ||
	    (bootdev & B_MAGICMASK) != (u_long)B_DEVMAGIC)
		return;
	majdev = B_TYPE(bootdev);
	if (findblkname(majdev) == NULL)
		return;
	adaptor = B_ADAPTOR(bootdev);
	part = B_PARTITION(bootdev);
	unit = B_UNIT(bootdev);
	mindev = (unit * MAXPARTITIONS) + part;
	orootdev = rootdev;
	rootdev = makedev(majdev, mindev);
	/*
	 * If the original rootdev is the same as the one
	 * just calculated, don't need to adjust the swap configuration.
	 */
	printf("root on %s%d%c\n", findblkname(majdev), unit, part + 'a');
	if (rootdev == orootdev)
		return;

#ifdef DOSWAP
	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		if (majdev == major(swp->sw_dev) &&
		    mindev/MAXPARTITIONS == minor(swp->sw_dev)/MAXPARTITIONS) {
			temp = swdevt[0].sw_dev;
			swdevt[0].sw_dev = swp->sw_dev;
			swp->sw_dev = temp;
			break;
		}
	}
	if (swp->sw_dev == NODEV)
		return;

	/*
	 * If dumpdev was the same as the old primary swap device, move
	 * it to the new primary swap device.
	 */
	if (temp == dumpdev)
		dumpdev = swdevt[0].sw_dev;
#endif
}

#include "wd.h"
#if NWD > 0
extern	struct cfdriver wd_cd;
#endif
#include "sd.h"
#if NSD > 0
extern	struct cfdriver sd_cd;
#endif
#include "cd.h"
#if NCD > 0
extern	struct cfdriver cd_cd;
#endif
#include "mcd.h"
#if NMCD > 0
extern	struct cfdriver mcd_cd;
#endif
#include "fd.h"
#if NFD > 0
extern	struct cfdriver fd_cd;
#endif
#include "rd.h"
#if NRD > 0
extern	struct cfdriver rd_cd;
#endif
#include "raid.h"
#if NRAID > 0
extern	struct cfdriver raid_cd;
#endif

struct	genericconf {
	struct cfdriver *gc_driver;
	char *gc_name;
	dev_t gc_major;
} genericconf[] = {
#if NWD > 0
	{ &wd_cd,  "wd",  0 },
#endif
#if NFD > 0
	{ &fd_cd,  "fd",  2 },
#endif
#if NSD > 0
	{ &sd_cd,  "sd",  4 },
#endif
#if NCD > 0
	{ &cd_cd,  "cd",  6 },
#endif
#if NMCD > 0
	{ &mcd_cd, "mcd", 7 },
#endif
#if NRD > 0
	{ &rd_cd,  "rd",  17 },
#endif
#if NRAID > 0
	{ &raid_cd,  "raid",  19 },
#endif
	{ 0 }
};

void
rootconf()
{
	register struct genericconf *gc;
	int unit, part = 0;
	char *num;

#ifdef INSTALL
	if (B_TYPE(bootdev) == 2) {
		printf("\n\nInsert file system floppy...\n");
		if (!(boothowto & RB_ASKNAME))
			cngetc();
	}
#endif

	if (boothowto & RB_ASKNAME) {
		char name[128];
retry:
		printf("root device? ");
		cnpollc(TRUE);
		getsn(name, sizeof name);
		cnpollc(FALSE);
		if (*name == '\0')
			goto noask;
		for (gc = genericconf; gc->gc_driver; gc++)
			if (gc->gc_driver->cd_ndevs &&
			    strncmp(gc->gc_name, name,
			    strlen(gc->gc_name)) == 0)
				break;
		if (gc->gc_driver) {
			num = &name[strlen(gc->gc_name)];

			unit = -2;
			do {
				if (unit != -2 && *num >= 'a' &&
				    *num <= 'a'+MAXPARTITIONS-1 &&
				    num[1] == '\0') {
					part = *num++ - 'a';
					break;
				}
				if (unit == -2)
					unit = 0;
				unit = (unit * 10) + *num - '0';
				if (*num < '0' || *num > '9')
					unit = -1;
			} while (unit != -1 && *++num);

			if (unit < 0) {
				printf("%s: not a unit number\n",
				    &name[strlen(gc->gc_name)]);
			} else if (unit > gc->gc_driver->cd_ndevs ||
			    gc->gc_driver->cd_devs[unit] == NULL) {
				printf("%d: no such unit\n", unit);
			} else {
				printf("root on %s%d%c\n", gc->gc_name, unit,
				    'a' + part);
				rootdev = makedev(gc->gc_major,
				    unit * MAXPARTITIONS + part);
				goto doswap;
			}
		}
		printf("use one of: ");
		for (gc = genericconf; gc->gc_driver; gc++) {
			for (unit=0; unit < gc->gc_driver->cd_ndevs; unit++) {
				if (gc->gc_driver->cd_devs[unit])
					printf("%s%d[a-%c] ", gc->gc_name,
					    unit, 'a'+MAXPARTITIONS-1);
			}
		}
		printf("\n");
		goto retry;
	}
noask:
	if (mountroot == NULL) {
		/* `swap generic' */
		setroot();
	} else {
		/* preconfigured */
		int  majdev, unit, part;

		majdev = major(rootdev);
		if (findblkname(majdev) == NULL)
			return;
		part = minor(rootdev) % MAXPARTITIONS;
		unit = minor(rootdev) / MAXPARTITIONS;
		printf("root on %s%d%c\n", findblkname(majdev), unit, part + 'a');
		return;
	}

doswap:
#ifndef DISKLESS
	mountroot = dk_mountroot;
#endif
	swdevt[0].sw_dev = argdev = dumpdev =
	    makedev(major(rootdev), minor(rootdev) + 1);
	/* swap size and dumplo set during autoconfigure */
}
