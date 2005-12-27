/*	$OpenBSD: autoconf.c,v 1.22 2005/12/27 18:31:09 miod Exp $	*/
/*	$NetBSD: autoconf.c,v 1.38 1996/12/18 05:46:09 scottr Exp $	*/

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
 *	@(#)autoconf.c	8.4 (Berkeley) 10/1/93
 */

/*
 * Setup the system to run on the current machine.
 *
 * cpu_configure() is called at boot time.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/lock.h>

#include <dev/cons.h>

#include <machine/autoconf.h>
#include <machine/adbsys.h>
#include <machine/viareg.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ffs/ffs_extern.h>

struct device	*booted_device;
int		booted_partition;

struct device *parsedisk(char *, int, int, dev_t *);
struct device *getdisk(char *, int, int, dev_t *);
int findblkmajor(struct device *);
int getstr(char *, int);
void findbootdev(void);
int target_to_unit(u_long, u_long, u_long);

void	setroot(void);

#ifdef RAMDISK_HOOKS
static struct device fakerdrootdev = { DV_DISK, {}, NULL, 0, "rd0", NULL };
#endif

void
cpu_configure()
{
	mrg_init();		/* Init Mac ROM Glue */
	startrtclock();		/* start before adb_init() */
	adb_init();		/* ADB device subsystem & driver */

	if (config_rootfound("mainbus", "mainbus") == NULL)
		panic("No mainbus found!");

	findbootdev();
	setroot();
	dumpconf();
	cold = 0;
}

struct nam2blk {
	char *name;
	int maj;
} nam2blk[] = {
	{ "sd",         4 },
	{ "cd",         6 },
	{ "rd",		13 },
};

int
findblkmajor(dv)
	struct device *dv;
{
	char *name = dv->dv_xname;
	register int i;

	for (i = 0; i < sizeof(nam2blk)/sizeof(nam2blk[0]); ++i)
		if (!strncmp(name, nam2blk[i].name, strlen(nam2blk[0].name)))
			return (nam2blk[i].maj);
	return (-1);
}

struct device *
getdisk(str, len, defpart, devp)
	char *str;
	int len, defpart;
	dev_t *devp;
{
	register struct device *dv;

	if ((dv = parsedisk(str, len, defpart, devp)) == NULL) {
		printf("use one of:");
#ifdef RAMDISK_HOOKS
		printf(" %s[a-p]", fakerdrootdev.dv_xname);
#endif
		TAILQ_FOREACH(dv, &alldevs, dv_list) {
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
	register struct device *dv;
	register char *cp, c;
	int majdev, part;

	if (len == 0)
		return (NULL);
	cp = str + len - 1;
	c = *cp;
	if (c >= 'a' && c <= ('a' + MAXPARTITIONS - 1)) {
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
			if (majdev < 0)
				panic("parsedisk");
			*devp = MAKEDISKDEV(majdev, dv->dv_unit, part);
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

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 *
 * XXX Actually, swap and root must be on the same type of device,
 * (ie. DV_DISK or DV_IFNET) because of how (*mountroot) is written.
 * That should be fixed.
 */
void
setroot(void)
{
	register struct swdevt *swp;
	register struct device *dv;
	register int len;
	dev_t nrootdev, nswapdev = NODEV;
	char buf[128];
	dev_t temp;
	struct device *bootdv, *rootdv, *swapdv;
	int bootpartition;
#if defined(NFSCLIENT)
	extern char *nfsbootdevname;
#endif

#ifdef RAMDISK_HOOKS
	bootdv = &fakerdrootdev;
#else
	bootdv = booted_device;
#endif
	bootpartition = booted_partition;
	rootdv = swapdv = NULL;		/* XXX work around gcc warning */

	/*
	 * If `swap generic' and we couldn't determine boot device,
	 * ask the user.
	 */
	if (mountroot == NULL && bootdv == NULL)
		boothowto |= RB_ASKNAME;

	if (boothowto & RB_ASKNAME) {
		for (;;) {
			printf("root device");
			if (bootdv != NULL)
				printf(" (default %s%c)", bootdv->dv_xname,
				    bootdv->dv_class == DV_DISK ? 'a' : ' ');
			printf(": ");
			len = getstr(buf, sizeof(buf));
			if (len == 0 && bootdv != NULL) {
				strlcpy(buf, bootdv->dv_xname, sizeof buf);
				len = strlen(buf);
			}
			if (len == 4 && !strcmp(buf, "halt"))
				boot(RB_HALT);
			if (len > 0 && buf[len - 1] == '*') {
				buf[--len] = '\0';
				dv = getdisk(buf, len, 1, &nrootdev);
				if (dv != NULL) {
					rootdv = dv;
					nswapdev = nrootdev;
					goto gotswap;
				}
			}
			dv = getdisk(buf, len, bootpartition, &nrootdev);
			if (dv != NULL) {
				rootdv = dv;
				break;
			}
		}

		/*
		 * Because swap must be on same device as root, for
		 * network devices this is easy.
		 */
		if (rootdv->dv_class == DV_IFNET) {
			swapdv = NULL;
			goto gotswap;
		}
		for (;;) {
			printf("swap device");
			if (rootdv != NULL)
				printf(" (default %s%c)", rootdv->dv_xname,
				    rootdv->dv_class == DV_DISK ? 'b' : ' ');
			printf(": ");
			len = getstr(buf, sizeof(buf));
			if (len == 0) {
				switch (rootdv->dv_class) {
				case DV_IFNET:
					nswapdev = NODEV;
					break;
				case DV_DISK:
					nswapdev = MAKEDISKDEV(major(nrootdev),
					    DISKUNIT(minor(nrootdev)), 1);
					break;
				case DV_TAPE:
				case DV_TTY:
				case DV_DULL:
				case DV_CPU:
					break;
				}
				swapdv = rootdv;
				break;
			}
			if (len == 4 && !strcmp(buf, "halt"))
				boot(RB_HALT);
			dv = getdisk(buf, len, 1, &nswapdev);
			if (dv) {
				if (dv->dv_class == DV_IFNET)
					nswapdev = NODEV;
				swapdv = dv;
				break;
			}
		}
gotswap:
		rootdev = nrootdev;
		dumpdev = nswapdev;
		swdevt[0].sw_dev = nswapdev;
		swdevt[1].sw_dev = NODEV;

	} else if (mountroot == NULL) {
		int majdev;

		/*
		 * "swap generic"
		 */
		majdev = findblkmajor(bootdv);
		if (majdev >= 0) {
			/*
			 * Root and swap are on a disk.
			 */
			rootdv = swapdv = bootdv;
			rootdev =
			    MAKEDISKDEV(majdev, bootdv->dv_unit, bootpartition);
			nswapdev = dumpdev =
			    MAKEDISKDEV(majdev, bootdv->dv_unit, 1);
		} else {
			/*
			 * Root and swap are on a net.
			 */
			rootdv = swapdv = bootdv;
			nswapdev = dumpdev = NODEV;
		}
		swdevt[0].sw_dev = nswapdev;
		swdevt[1].sw_dev = NODEV;

	} else {

		/*
		 * `root DEV swap DEV': honour rootdev/swdevt.
		 * rootdev/swdevt/mountroot already properly set.
		 */
		return;
	}

	switch (rootdv->dv_class) {
#ifdef NFSCLIENT
	case DV_IFNET:
		mountroot = nfs_mountroot;
		nfsbootdevname = rootdv->dv_xname;
		return;
#endif
	case DV_DISK:
		mountroot = ffs_mountroot;
		printf("root on %s%c", rootdv->dv_xname,
		    DISKPART(rootdev) + 'a');
		if (nswapdev != NODEV)
			printf(" swap on %s%c", swapdv->dv_xname,
			    DISKPART(nswapdev) + 'a');
		printf("\n");
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
		if (major(rootdev) == major(swp->sw_dev) &&
		    DISKUNIT(rootdev) == DISKUNIT(minor(swp->sw_dev))) {
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
}

int
getstr(cp, size)
	register char *cp;
	register int size;
{
	register char *lp;
	register int c;
	register int len;

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


dev_t		bootdev;

/*
 * Yanked from i386/i386/autoconf.c (and tweaked a bit)
 */
void
findbootdev()
{
	struct device *dv;
	int major, unit;

	booted_device = NULL;
	booted_partition = 0;	/* Assume root is on partition a */

	major = B_TYPE(bootdev);
	if (major < 0 || major >= nblkdev)
		return;

	unit = B_UNIT(bootdev);

	bootdev &= ~(B_UNITMASK << B_UNITSHIFT);
	unit = target_to_unit(-1, unit, 0);
	bootdev |= (unit << B_UNITSHIFT);

	if (disk_count <= 0)
		return;

	TAILQ_FOREACH(dv, &alldevs, dv_list) {
		if (dv->dv_class == DV_DISK && major == findblkmajor(dv) &&
		    unit == dv->dv_unit) {
			booted_device = dv;
			return;
		}
	}
}

/*
 * Map a SCSI bus, target, lun to a device number.
 * This could be tape, disk, CD.  The calling routine, though,
 * assumes DISK.  It would be nice to allow CD, too...
 */
int
target_to_unit(bus, target, lun)
	u_long bus, target, lun;
{
	struct scsibus_softc	*scsi;
	struct scsi_link	*sc_link;
	struct device		*sc_dev;
extern	struct cfdriver		scsibus_cd;

	if (target < 0 || target > 7 || lun < 0 || lun > 7) {
		printf("scsi target to unit, target (%ld) or lun (%ld)"
			" out of range.\n", target, lun);
		return -1;
	}

	if (bus == -1) {
		for (bus = 0 ; bus < scsibus_cd.cd_ndevs ; bus++) {
			if (scsibus_cd.cd_devs[bus]) {
				scsi = (struct scsibus_softc *)
						scsibus_cd.cd_devs[bus];
				if (scsi->sc_link[target][lun]) {
					sc_link = scsi->sc_link[target][lun];
					sc_dev = (struct device *)
							sc_link->device_softc;
					return sc_dev->dv_unit;
				}
			}
		}
		return -1;
	}
	if (bus < 0 || bus >= scsibus_cd.cd_ndevs) {
		printf("scsi target to unit, bus (%ld) out of range.\n", bus);
		return -1;
	}
	if (scsibus_cd.cd_devs[bus]) {
		scsi = (struct scsibus_softc *) scsibus_cd.cd_devs[bus];
		if (scsi->sc_link[target][lun]) {
			sc_link = scsi->sc_link[target][lun];
			sc_dev = (struct device *) sc_link->device_softc;
			return sc_dev->dv_unit;
		}
	}
	return -1;
}
