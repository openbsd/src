/*	$OpenBSD: autoconf.c,v 1.53 2004/06/13 21:49:15 niklas Exp $	*/
/*	$NetBSD: autoconf.c,v 1.20 1996/05/03 19:41:56 christos Exp $	*/

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
#include <sys/reboot.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/gdt.h>
#include <machine/biosvar.h>

#include <dev/cons.h>

#include "ioapic.h"

#if NIOAPIC > 0
#include <machine/i82093var.h>
#endif

int findblkmajor(struct device *dv);
char *findblkname(int);

void rootconf(void);
void swapconf(void);
void setroot(void);
void diskconf(void);

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
dev_t	bootdev = 0;		/* bootdevice, initialized in locore.s */

/* Support for VIA C3 RNG */
#ifdef I686_CPU
extern struct timeout viac3_rnd_tmo;
extern int	viac3_rnd_present;
void		viac3_rnd(void *);

#ifdef CRYPTO
extern int	viac3_crypto_present;
void		viac3_crypto_setup(void);
#endif /* CRYPTO */
#endif

/*
 * Determine i/o configuration for a machine.
 */
void
cpu_configure()
{
	/*
	 * Note, on i386, configure is not running under splhigh unlike other
	 * architectures.  This fact is used by the pcmcia irq line probing.
	 */

	startrtclock();

	gdt_init();		/* XXX - pcibios uses gdt stuff */

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("cpu_configure: mainbus not configured");

	printf("biomask %x netmask %x ttymask %x\n", (u_short)IMASK(IPL_BIO),
	    (u_short)IMASK(IPL_NET), (u_short)IMASK(IPL_TTY));

#if NIOAPIC > 0
	ioapic_enable();
#endif

#ifdef MULTIPROCESSOR
	/* propagate TSS and LDT configuration to the idle pcb's. */
	cpu_init_idle_pcbs();
#endif
	spl0();

	/*
	 * We can not know which is our root disk, defer
	 * until we can checksum blocks to figure it out.
	 */
	md_diskconf = diskconf;
	cold = 0;

	/* Set up proc0's TSS and LDT (after the FPU is configured). */
	i386_proc0_tss_ldt_init();

#ifdef I686_CPU
	/*
	 * At this point the RNG is running, and if FSXR is set we can
	 * use it.  Here we setup a periodic timeout to collect the data.
	 */
	if (viac3_rnd_present) {
		timeout_set(&viac3_rnd_tmo, viac3_rnd, &viac3_rnd_tmo);
		viac3_rnd(&viac3_rnd_tmo);
	}
#ifdef CRYPTO
	/*
	 * Also, if the chip has crypto available, enable it.
	 */
	if (viac3_crypto_present)
		viac3_crypto_setup();
#endif /* CRYPTO */
#endif
}

/*
 * Now that we are fully operational, we can checksum the
 * disks, and using some heuristics, hopefully are able to
 * always determine the correct root disk.
 */
void
diskconf()
{
	/*
	 * Configure root, swap, and dump area.  This is
	 * currently done by running the same checksum
	 * algorithm over all known disks, as was done in
	 * /boot.  Then we basically fixup the *dev vars
	 * from the info we gleaned from this.
	 */
	dkcsumattach();

	rootconf();
	swapconf();
	dumpconf();
}

/*
 * Configure swap space and related parameters.
 */
void
swapconf()
{
	register struct swdevt *swp;
	register int nblks;

	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		int maj = major(swp->sw_dev);

		if (maj > nblkdev)
			break;
		if (bdevsw[maj].d_psize) {
			nblks = (*bdevsw[maj].d_psize)(swp->sw_dev);
			if (nblks != -1 &&
			    (swp->sw_nblks == 0 || swp->sw_nblks > nblks))
				swp->sw_nblks = nblks;
			swp->sw_nblks = ctod(dtoc(swp->sw_nblks));
		}
	}
}

#define	DOSWAP			/* change swdevt and dumpdev */

static struct {
	char *name;
	int maj;
} nam2blk[] = {
	{ "wd", 0 },
	{ "sw", 1 },
	{ "fd", 2 },
	{ "wt", 3 },
	{ "sd", 4 },
	{ "cd", 6 },
	{ "mcd", 7 },
	{ "rd", 17 },
	{ "raid", 19 }
};

int
findblkmajor(dv)
	struct device *dv;
{
	char *name = dv->dv_xname;
	int i;

	for (i = 0; i < sizeof(nam2blk)/sizeof(nam2blk[0]); ++i)
		if (strncmp(name, nam2blk[i].name, strlen(nam2blk[i].name))
		    == 0)
			return (nam2blk[i].maj);
	return (-1);
}

char *
findblkname(maj)
	int maj;
{
	int i;

	for (i = 0; i < sizeof(nam2blk)/sizeof(nam2blk[0]); ++i)
		if (maj == nam2blk[i].maj)
			return (nam2blk[i].name);
	return (NULL);
}

dev_t	argdev = NODEV;
int	nswap;
long	dumplo;

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
#endif
	struct swdevt *swp;

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
