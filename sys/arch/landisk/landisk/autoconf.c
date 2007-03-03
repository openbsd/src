/*	$OpenBSD: autoconf.c,v 1.5 2007/03/03 21:37:27 miod Exp $	*/
/*	$NetBSD: autoconf.c,v 1.1 2006/09/01 21:26:18 uwe Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/reboot.h>

#include <dev/cons.h>

#include <machine/cpu.h>
#include <machine/intr.h>

int cold = 1;

struct device *booted_device;
struct device *root_device;

void	diskconf(void);
struct device *parsedisk(char *, int, int, dev_t *);
void	setroot(void);
int	findblkmajor(struct device *);
char	*findblkname(int);
struct device *getdisk(char *, int, int, dev_t *);

void
cpu_configure(void)
{
	/* Start configuration */
	splhigh();
	softintr_init();
	intr_init();

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("no mainbus found");

	md_diskconf = diskconf;

	/* Configuration is finished, turn on interrupts. */
	spl0();

	cold = 0;
}

void
diskconf(void)
{
	printf("boot device: %s\n",
	    booted_device ? booted_device->dv_xname : "<unknown>");
	setroot();
	dumpconf();
}

static	struct nam2blk {
	char *name;
	int  maj;
} nam2blk[] = {
	{ "wd",	16 },
	{ "rd",	18 },
	{ "sd",	24 }
};

int
findblkmajor(struct device *dv)
{
	char *name = dv->dv_xname;
	int i;

	for (i = 0; i < sizeof(nam2blk)/sizeof(nam2blk[0]); ++i)
		if (strncmp(name, nam2blk[i].name, strlen(nam2blk[i].name)) ==
		    0)
			return (nam2blk[i].maj);
	return (-1);
}

char *
findblkname(int maj)
{
	int i;

	for (i = 0; i < sizeof(nam2blk)/sizeof(nam2blk[0]); i++)
		if (nam2blk[i].maj == maj)
			return (nam2blk[i].name);
	 return (NULL);
}

struct device *
getdisk(char *str, int len, int defpart, dev_t *devp)
{
	struct device *dv;

	if ((dv = parsedisk(str, len, defpart, devp)) == NULL) {
		printf("use one of:");
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

struct device *
parsedisk(char *str, int len, int defpart, dev_t *devp)
{
	struct device *dv;
	char *cp, c;
	int majdev, part;

	if (len == 0)
		return (NULL);

	if (len == 4 && strcmp(str, "halt") == 0)
		boot(RB_HALT);

	cp = str + len - 1;
	c = *cp;
	if (c >= 'a' && (c - 'a') < MAXPARTITIONS) {
		part = c - 'a';
		*cp = '\0';
	} else
		part = defpart;

	TAILQ_FOREACH(dv, &alldevs, dv_list) {
		if (dv->dv_class == DV_DISK &&
		    strcmp(str, dv->dv_xname) == 0) {
			majdev = findblkmajor(dv);
			if (majdev < 0) {
				printf("%s is not a valid boot device\n", str);
				dv = NULL;
				break;
			}
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
 */
void
setroot()
{
	int  majdev, mindev, unit, part, len;
	dev_t temp;
	struct swdevt *swp;
	struct device *dv;
	dev_t nrootdev, nswapdev = NODEV;
	char buf[128];
	int s;

#if defined(NFSCLIENT)
	extern char *nfsbootdevname;
#endif

	if (boothowto & RB_DFLTROOT)
		return;		/* Boot compiled in */

	if (booted_device == NULL && rootdev == NULL) {
		boothowto |= RB_ASKNAME; /* Don't Panic :-) */
		/* boothowto |= RB_SINGLE; */
	}

	if (boothowto & RB_ASKNAME) {
		for (;;) {
			printf("root device");
			if (booted_device != NULL) {
				 printf(" (default %s",
					booted_device->dv_xname);
				if (booted_device->dv_class == DV_DISK)
					printf("a");
				printf(")");
			}
			printf(": ");
			s = splhigh();
			cnpollc(1);
			len = getsn(buf, sizeof(buf));
			cnpollc(0);
			splx(s);
			if (len == 0 && booted_device != NULL) {
				strlcpy(buf, booted_device->dv_xname,
				    sizeof buf);
				len = strlen(buf);
			}
			if (len > 0 && buf[len - 1] == '*') {
				buf[--len] = '\0';
				dv = getdisk(buf, len, 1, &nrootdev);
				if (dv != NULL) {
					booted_device = dv;
					nswapdev = nrootdev;
					goto gotswap;
				}
			}
			dv = getdisk(buf, len, 0, &nrootdev);
			if (dv != NULL) {
				booted_device = dv;
				break;
			}
		}

		/*
		 * because swap must be on same device as root, for
		 * network devices this is easy.
		 */
		if (booted_device->dv_class == DV_IFNET)
			goto gotswap;

		for (;;) {
			printf("swap device");
			if (booted_device != NULL) {
				printf(" (default %s",
				    booted_device->dv_xname);
				if (booted_device->dv_class == DV_DISK)
					printf("b");
				printf(")");
			}
			printf(": ");
			s = splhigh();
			cnpollc(1);
			len = getsn(buf, sizeof(buf));
			cnpollc(0);
			splx(s);
			if (len == 0 && booted_device != NULL) {
				switch (booted_device->dv_class) {
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
	}
	else if(mountroot == NULL) {
		/*
		 * `swap generic'
		 */
		majdev = findblkmajor(booted_device);
		if (majdev >= 0) {
			/*
			 * Root and Swap are on disk.
			 * Boot is always from partition 0.
			 */
			rootdev = MAKEDISKDEV(majdev, booted_device->dv_unit, 0);
			nswapdev = dumpdev =
			    MAKEDISKDEV(majdev, booted_device->dv_unit, 1);
		} else {
			/*
			 *  Root and Swap are on net.
			 */
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

	switch (booted_device->dv_class) {
#if defined(NFSCLIENT)
	case DV_IFNET:
		mountroot = nfs_mountroot;
		nfsbootdevname = booted_device->dv_xname;
		return;
#endif
	case DV_DISK:
		mountroot = dk_mountroot;
		majdev = major(rootdev);
		mindev = minor(rootdev);
		unit = DISKUNIT(rootdev);
		part = DISKPART(rootdev);
		printf("root on %s%c\n", booted_device->dv_xname, part + 'a');
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
	if (swp->sw_dev == NODEV)
		return;

	/*
	 * If dumpdev was the same as the old primary swap device, move
	 * it to the new primary swap device.
	 */
	if (temp == dumpdev)
		dumpdev = swdevt[0].sw_dev;
}
