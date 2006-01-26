/*	$OpenBSD: autoconf.c,v 1.27 2006/01/26 02:43:28 aaron Exp $	*/
/*	$NetBSD: autoconf.c,v 1.16 1996/11/13 21:13:04 cgd Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/rpb.h>
#include <machine/prom.h>
#include <machine/cpuconf.h>
#include <machine/intr.h>

#include <dev/cons.h>

extern char		root_device[17];		/* XXX */

struct device		*booted_device;
int			booted_partition;
struct bootdev_data	*bootdev_data;
char			boot_dev[128];

void	parse_prom_bootdev(void);
int	atoi(char *);
void	setroot(void);

static struct device *parsedisk(char *str, int len, int defpart,
				     dev_t *devp);
static struct device *getdisk(char *str, int len, int defpart,
				   dev_t *devp);
int findblkmajor(struct device *dv);
char *findblkname(int);
static int getstr(char *cp, int size);

/*
 * cpu_configure:
 * called at boot time, configure all devices on system
 */
void
cpu_configure()
{
	parse_prom_bootdev();
	softintr_init();

	/*
	 * Disable interrupts during autoconfiguration.  splhigh() won't
	 * work, because it simply _raises_ the IPL, so if machine checks
	 * are disabled, they'll stay disabled.  Machine checks are needed
	 * during autoconfig.
	 */
	(void)alpha_pal_swpipl(ALPHA_PSL_IPL_HIGH);
	if (config_rootfound("mainbus", "mainbus") == NULL)
		panic("no mainbus found");
	(void)spl0();

	hwrpb_restart_setup();

	if (booted_device == NULL)
		printf("WARNING: can't figure what device matches \"%s\"\n",
		    boot_dev);
	setroot();
	dumpconf();
	cold = 0;
}

struct nam2blk {
	char *name;
	int maj;
} nam2blk[] = {
#include "st.h"
#if NST > 0
	{ "st",		2 },
#endif
#include "cd.h"
#if NCD > 0
	{ "cd",		3 },
#endif
#include "fd.h"
#if NFD > 0
	{ "fd",		4 },
#endif
#include "rd.h"
#if NRD > 0
	{ "rd",		6 },
#endif
	{ "sd",		8 },
	{ "wd",		0 },
#include "raid.h"
#if NRAID > 0
	{ "raid",	16 },
#endif
};

#ifdef RAMDISK_HOOKS
static struct device fakerdrootdev = { DV_DISK, {}, NULL, 0, "rd0", NULL };
#endif

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

static struct device *
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
		printf(" halt\n");
	}
	return (dv);
}

static struct device *
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

	if (len == 4 && !strcmp(str, "halt"))
		boot(RB_HALT /* , NULL */);

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
setroot()
{
	struct swdevt *swp;
	struct device *dv;
	int len, majdev, unit, part;
	dev_t nrootdev, nswapdev = NODEV;
	char buf[128], *rootdevname;
	dev_t temp;
	struct device *bootdv, *rootdv, *swapdv;
	int bootpartition;
#if defined(NFSCLIENT)
	extern char *nfsbootdevname;
#endif

#ifdef RAMDISK_HOOKS
	bootdv = &fakerdrootdev;
	bootpartition = 0;
#else
	bootdv = booted_device;
	bootpartition = booted_partition;
#endif

	/*
	 * (raid) device auto-configuration could have returned
	 * the root device's id in rootdev.  Check this case.
	 */
	if (rootdev != NODEV) {
		majdev = major(rootdev);
		unit = DISKUNIT(rootdev);
		part = DISKPART(rootdev);

		len = snprintf(buf, sizeof buf, "%s%d",
		    findblkname(majdev), unit);
		if (len == -1 || len >= sizeof(buf))
			panic("setroot: device name too long");

		bootdv = getdisk(buf, len, part, &rootdev);
	}

	/*
	 * If 'swap generic' and we couldn't determine root device,
	 * ask the user.
	 */
	if (mountroot == NULL && bootdv == NULL)
		boothowto |= RB_ASKNAME;

	if (boothowto & RB_ASKNAME) {
		for (;;) {
			printf("root device");
			if (bootdv != NULL) {
				printf(" (default %s", bootdv->dv_xname);
				if (bootdv->dv_class == DV_DISK)
					printf("%c", bootpartition + 'a');
				printf(")");
			}
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
					rootdv = swapdv = dv;
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
		 * because swap must be on same device type as root, for
		 * network devices this is easy.
		 */
		if (rootdv->dv_class == DV_IFNET) {
			swapdv = NULL;
			goto gotswap;
		}
		for (;;) {
			printf("swap device");
			printf(" (default %s", rootdv->dv_xname);
			if (rootdv->dv_class == DV_DISK)
				printf("b");
			printf(")");
			printf(": ");
			len = getstr(buf, sizeof(buf));
			if (len == 0) {
				switch (rootdv->dv_class) {
				case DV_IFNET:
					nswapdev = NODEV;
					break;
				case DV_DISK:
					nswapdev = MAKEDISKDEV(major(nrootdev),
					    DISKUNIT(nrootdev), 1);
					break;
				default:
					break;
				}
				swapdv = rootdv;
				break;
			}
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
		
		/*
		 * "swap generic"
		 */
		majdev = findblkmajor(bootdv);
		if (majdev >= 0) {
			/*
			 * Root and swap are on a disk.
			 */
			rootdv = swapdv = bootdv;
			rootdev = MAKEDISKDEV(majdev, bootdv->dv_unit,
			    bootpartition);
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
		 * `root DEV swap DEV': honor rootdev/swdevt.
		 * rootdev/swdevt/mountroot already properly set.
		 */

		rootdevname = findblkname(major(rootdev));
		if (rootdevname == NULL) {
			/* Root on NFS or unknown device. */
			strlcpy(root_device, "??", sizeof root_device);
		} else {
			/* Root on known block device. */
			snprintf(root_device, sizeof root_device,
			    "%s%d%c", rootdevname,
			    DISKUNIT(rootdev), DISKPART(rootdev) + 'a');
		}
			
		return;
	}

	switch (rootdv->dv_class) {
#if defined(NFSCLIENT)
	case DV_IFNET:
		strlcpy(root_device, "??", sizeof root_device);
		mountroot = nfs_mountroot;
		nfsbootdevname = rootdv->dv_xname;
		return;
#endif
	case DV_DISK:
		mountroot = dk_mountroot;
		snprintf(root_device, sizeof root_device,
		    "%s%c", rootdv->dv_xname,
		    DISKPART(rootdev) + 'a');
		printf("root on %s", root_device);
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
		    DISKUNIT(rootdev) == DISKUNIT(swp->sw_dev)) {
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

static int
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

void
parse_prom_bootdev()
{
	static char hacked_boot_dev[128];
	static struct bootdev_data bd;
	char *cp, *scp, *boot_fields[8];
	int i, done;

	booted_device = NULL;
	booted_partition = 0;
	bootdev_data = NULL;

	bcopy(bootinfo.booted_dev, hacked_boot_dev, sizeof hacked_boot_dev);
#if 0
	printf("parse_prom_bootdev: boot dev = \"%s\"\n", boot_dev);
#endif

	i = 0;
	scp = cp = hacked_boot_dev;
	for (done = 0; !done; cp++) {
		if (*cp != ' ' && *cp != '\0')
			continue;
		if (*cp == '\0')
			done = 1;

		*cp = '\0';
		boot_fields[i++] = scp;
		scp = cp + 1;
		if (i == 8)
			done = 1;
	}
	if (i != 8)
		return;		/* doesn't look like anything we know! */

#if 0
	printf("i = %d, done = %d\n", i, done);
	for (i--; i >= 0; i--)
		printf("%d = %s\n", i, boot_fields[i]);
#endif

	bd.protocol = boot_fields[0];
	bd.bus = atoi(boot_fields[1]);
	bd.slot = atoi(boot_fields[2]);
	bd.channel = atoi(boot_fields[3]);
	bd.remote_address = boot_fields[4];
	bd.unit = atoi(boot_fields[5]);
	bd.boot_dev_type = atoi(boot_fields[6]);
	bd.ctrl_dev_type = boot_fields[7];

#if 0
	printf("parsed: proto = %s, bus = %d, slot = %d, channel = %d,\n",
	    bd.protocol, bd.bus, bd.slot, bd.channel);
	printf("\tremote = %s, unit = %d, dev_type = %d, ctrl_type = %s\n",
	    bd.remote_address, bd.unit, bd.boot_dev_type, bd.ctrl_dev_type);
#endif

	bootdev_data = &bd;
}

int
atoi(s)
	char *s;
{
	int n, neg;

	n = 0;
	neg = 0;

	while (*s == '-') {
		s++;
		neg = !neg;
	}

	while (*s != '\0') {
		if (*s < '0' || *s > '9')
			break;

		n = (10 * n) + (*s - '0');
		s++;
	}

	return (neg ? -n : n);
}

void
device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	if (bootdev_data == NULL) {
		/*
		 * There is no hope.
		 */
		return;
	}

	if (platform.device_register)
		(*platform.device_register)(dev, aux);
}
