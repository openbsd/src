/*	$OpenBSD: autoconf.c,v 1.4 2004/02/09 20:39:25 mickey Exp $	*/
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
 *	@(#)autoconf.c	7.1 (Berkeley) 5/9/91
 */

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time and initializes the vba 
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
void swapconf(void);
void diskconf(void);
int findblkmajor(struct device *);
char *findblkname(int);
static	struct device *getdisk(char *, int, int, dev_t *);
struct device * parsedisk(char *, int, int, dev_t *);
static	int getstr(char *, int);

extern struct disklist *x86_64_alldisks;
extern int x86_64_ndisks;

#if 0
#include "bios32.h"
#if NBIOS32 > 0
#include <machine/bios32.h>
#endif
#endif

int	cold = 1;	/* if 1, still working on cold-start */
struct device *booted_device;
int booted_partition;
dev_t bootdev;

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
	setroot();
	swapconf();
	dumpconf();
}

void
setroot()
{
	struct swdevt *swp;
	struct device *dv;
	int len, majdev, unit, part;
	dev_t nrootdev, nswapdev = NODEV;
	char buf[128];
	dev_t temp;
	struct device *bootdv;
	struct bootpath *bp;
#if defined(NFSCLIENT)
	extern char *nfsbootdevname;
#endif

#ifdef RAMDISK_HOOKS
	bootdv = &fakerdrootdev;
#else
	bootdv = booted_device;
#endif

	/*
	 * (raid) device auto-configuration could have returned
	 * the root device's id in rootdev.  Check this case.
	 */
	if (rootdev != NODEV) {
		majdev = major(rootdev);
		unit = DISKUNIT(rootdev);
		part = DISKPART(rootdev);

		len = snprintf(buf, sizeof buf, "%s%d", findblkname(majdev),
			unit);
		if (len >= sizeof(buf))
			panic("setroot: device name too long");

		bootdv = getdisk(buf, len, part, &rootdev);
	}

	/*
	 * If `swap generic' and we couldn't determine boot device,
	 * ask the user.
	 */
	if (mountroot == NULL && bootdv == NULL)
		boothowto |= RB_ASKNAME;

	if (boothowto & RB_ASKNAME) {
		for (;;) {
			printf("root device ");
			if (bootdv != NULL)
				printf("(default %s%c)",
					bootdv->dv_xname,
					bootdv->dv_class == DV_DISK
						? booted_partition + 'a' : ' ');
			printf(": ");
			len = getstr(buf, sizeof(buf));
			if (len == 0 && bootdv != NULL) {
				strlcpy(buf, bootdv->dv_xname, sizeof buf);
				len = strlen(buf);
			}
			if (len > 0 && buf[len - 1] == '*') {
				buf[--len] = '\0';
				dv = getdisk(buf, len, 1, &nrootdev);
				if (dv != NULL) {
					bootdv = dv;
					nswapdev = nrootdev;
					goto gotswap;
				}
			}
			if (len == 4 && strncmp(buf, "exit", 4) == 0)
				boot(RB_HALT);
			dv = getdisk(buf, len, bp ? booted_partition : 0,
			    &nrootdev);
			if (dv != NULL) {
				bootdv = dv;
				break;
			}
		}

		/*
		 * because swap must be on same device as root, for
		 * network devices this is easy.
		 */
		if (bootdv->dv_class == DV_IFNET) {
			goto gotswap;
		}
		for (;;) {
			printf("swap device ");
			if (bootdv != NULL)
				printf("(default %s%c)",
					bootdv->dv_xname,
					bootdv->dv_class == DV_DISK?'b':' ');
			printf(": ");
			len = getstr(buf, sizeof(buf));
			if (len == 0 && bootdv != NULL) {
				switch (bootdv->dv_class) {
				case DV_IFNET:
					nswapdev = NODEV;
					break;
				case DV_DISK:
					nswapdev = MAKEDISKDEV(major(nrootdev),
					    DISKUNIT(nrootdev), 1);
					break;
				case DV_TAPE:
				case DV_TTY:
				case DV_DULL:
				case DV_CPU:
					break;
				}
				break;
			}
			if (len == 4 && strncmp(buf, "exit", 4) == 0)
				boot(RB_HALT);
			dv = getdisk(buf, len, 1, &nswapdev);
			if (dv) {
				if (dv->dv_class == DV_IFNET)
					nswapdev = NODEV;
				break;
			}
		}
gotswap:
		rootdev = nrootdev;
		dumpdev = nswapdev;
		swdevt[0].sw_dev = nswapdev;
		swdevt[1].sw_dev = NODEV;

	} else if (mountroot == NULL) {

		/*
		 * `swap generic': Use the device the ROM told us to use.
		 */
		majdev = findblkmajor(bootdv);
		if (majdev >= 0) {
			/*
			 * Root and swap are on a disk.
			 * val[2] of the boot device is the partition number.
			 * Assume swap is on partition b.
			 */
			part = booted_partition;
			unit = bootdv->dv_unit;
			rootdev = MAKEDISKDEV(majdev, unit, part);
			nswapdev = dumpdev = MAKEDISKDEV(major(rootdev),
			    DISKUNIT(rootdev), 1);
		} else {
			/*
			 * Root and swap are on a net.
			 */
			nswapdev = dumpdev = NODEV;
		}
		swdevt[0].sw_dev = nswapdev;
		/* swdevt[1].sw_dev = NODEV; */

	} else {

		/*
		 * `root DEV swap DEV': honour rootdev/swdevt.
		 * rootdev/swdevt/mountroot already properly set.
		 */
		if (bootdv->dv_class == DV_DISK)
			printf("root on %s%c\n", bootdv->dv_xname,
			    part + 'a');
		majdev = major(rootdev);
		unit = DISKUNIT(rootdev);
		part = DISKPART(rootdev);
		return;
	}

	switch (bootdv->dv_class) {
#if defined(NFSCLIENT)
	case DV_IFNET:
		mountroot = nfs_mountroot;
		nfsbootdevname = bootdv->dv_xname;
		return;
#endif
	case DV_DISK:
		mountroot = dk_mountroot;
		majdev = major(rootdev);
		unit = DISKUNIT(rootdev);
		part = DISKPART(rootdev);
		printf("root on %s%c\n", bootdv->dv_xname,
		    part + 'a');
		break;
	default:
		printf("can't figure root, hope your kernel is right\n");
		return;
	}

	/*
	 * Make the swap partition on the root drive the primary swap.
	 */
	temp = NODEV;
	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		if (majdev == major(swp->sw_dev) &&
		    unit == DISKUNIT(swp->sw_dev)) {
			temp = swdevt[0].sw_dev;
			swdevt[0].sw_dev = swp->sw_dev;
			swp->sw_dev = temp;
			break;
		}
	}
	if (swp->sw_dev != NODEV) {
		/*
		 * If dumpdev was the same as the old primary swap device,
		 * move it to the new primary swap device.
		 */
		if (temp == dumpdev)
			dumpdev = swdevt[0].sw_dev;
	}
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

static struct device *
getdisk(str, len, defpart, devp)
	char *str;
	int len, defpart;
	dev_t *devp;
{
	struct device *dv;

	if ((dv = parsedisk(str, len, defpart, devp)) == NULL) {
		printf("use one of: exit");
#ifdef RAMDISK_HOOKS
		printf(" %s[a-p]", fakerdrootdev.dv_xname);
#endif
		for (dv = alldevs.tqh_first; dv != NULL;
		    dv = dv->dv_list.tqe_next) {        
			if (dv->dv_class == DV_DISK)
				printf(" %s[a-p]", dv->dv_xname);
#ifdef NFSCLIENT
			if (dv->dv_class == DV_IFNET)
				printf(" %s", dv->dv_xname);
#endif
		}
		printf("\n");
	}
	return (dv);
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

	for (dv = alldevs.tqh_first; dv != NULL; dv = dv->dv_list.tqe_next) {
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

static int
getstr(cp, size)
	char *cp;
	int size;
{
	char *lp;
	int c;
	int len;

	lp = cp;
	len = 0;
	for (;;) {
		c = cngetc();
		switch (c) {
		case '\n':
		case '\r':
			printf("\n");
			*lp++ = '\0';
			return (len);
		case '\b':
		case '\177':
		case '#':
			if (len) {
				--len;
				--lp;
				printf("\b \b");
			}
			continue;
		case '@':
		case 'u'&037:
			len = 0;
			lp = cp;
			printf("\n");
			continue;
		default:
			if (len + 1 >= size || c < ' ') {
				printf("\007");
				continue;
			}
			printf("%c", c);
			++len;
			*lp++ = c;
		}
	}
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
