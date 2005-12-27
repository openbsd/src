/*	$OpenBSD: autoconf.c,v 1.5 2005/12/27 18:31:11 miod Exp $	*/
/*	$NetBSD: autoconf.c,v 1.2 2001/09/05 16:17:36 matt Exp $	*/

/*
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
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
 *	This product includes software developed by Mark Brinicombe for
 *      the NetBSD project.
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
 * autoconf.c
 *
 * Autoconfiguration functions
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <machine/bootconfig.h>
#include <machine/intr.h>

#include <dev/cons.h>


struct device *booted_device;
int booted_partition;

struct device *bootdv = NULL;

int findblkmajor(struct device *dv);
char * findblkname(int maj);

void rootconf(void);
void diskconf(void);

static struct device * getdisk(char *str, int len, int defpart, dev_t *devp);
struct  device *parsedisk(char *, int, int, dev_t *);
extern char *boot_file;

#include "wd.h"
#if NWD > 0  
extern  struct cfdriver wd_cd;
#endif
#include "sd.h"
#if NSD > 0
extern  struct cfdriver sd_cd;
#endif
#include "cd.h"
#if NCD > 0
extern  struct cfdriver cd_cd;
#endif
#if NRD > 0
extern  struct cfdriver rd_cd;
#endif
#include "raid.h"
#if NRAID > 0
extern  struct cfdriver raid_cd;
#endif

struct  genericconf {
	struct cfdriver *gc_driver;
	char *gc_name;
	dev_t gc_major;
} genericconf[] = {
#if NWD > 0
	{ &wd_cd,  "wd",  16 },
#endif
#if NSD > 0
	{ &sd_cd,  "sd",  24 },
#endif
#if NCD > 0
	{ &cd_cd,  "cd",  26 },
#endif
#if NRD > 0
	{ &rd_cd,  "rd",  18 },
#endif
#if NRAID > 0
	{ &raid_cd,  "raid",  71 },
#endif
	{ 0 }
};

int
findblkmajor(dv)
	struct device *dv;
{
	char *name = dv->dv_xname;
	int i;

	for (i = 0; i < sizeof(genericconf)/sizeof(genericconf[0]); ++i)
		if (strncmp(name, genericconf[i].gc_name,
		    strlen(genericconf[i].gc_name)) == 0)
			return (genericconf[i].gc_major);
	return (-1);
}

char *
findblkname(maj)
	int maj;
{
	int i;

	for (i = 0; i < sizeof(genericconf)/sizeof(genericconf[0]); ++i)
		if (maj == genericconf[i].gc_major)
			return (genericconf[i].gc_name);
	return (NULL);
}

static struct device *
getdisk(char *str, int len, int defpart, dev_t *devp)
{
	struct device *dv;

	if ((dv = parsedisk(str, len, defpart, devp)) == NULL) {
		printf("use one of:");
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
parsedisk(char *str, int len, int defpart, dev_t *devp)
{
	struct device *dv;
	char *cp, c;
	int majdev, part;

	if (len == 0)
		return (NULL);
	cp = str + len - 1;
	c = *cp;
	if (c >= 'a' && (c - 'a') < MAXPARTITIONS) {
		part = c - 'a';
		*cp = '\0';
	} else
		part = defpart;

	for (dv = alldevs.tqh_first; dv != NULL; dv = dv->dv_list.tqe_next) {
		if (dv->dv_class == DV_DISK &&
		    strcmp(str, dv->dv_xname) == 0) {
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
#if 0
	dkcsumattach();

#endif
	rootconf();
#if 0
	dumpconf();
#endif
}


/*
 * void cpu_configure()
 *
 * Configure all the root devices
 * The root devices are expected to configure their own children
 */
void
cpu_configure(void)
{
	softintr_init();

	/*
	 * Since various PCI interrupts could be routed via the ICU
	 * (for PCI devices in the bridge) we need to set up the ICU
	 * now so that these interrupts can be established correctly
	 * i.e. This is a hack.
	 */

	config_rootfound("mainbus", NULL);

	/*
	 * We can not know which is our root disk, defer
	 * until we can checksum blocks to figure it out.
	 */
	md_diskconf = diskconf;
	cold = 0;

	/* Time to start taking interrupts so lets open the flood gates .... */
	(void)spl0();

}

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
void
rootconf()
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
		if (len == -1 || len >= sizeof(buf))
			panic("rootconf: device name too long");

		bootdv = getdisk(buf, len, part, &rootdev);
	}

	/* Lookup boot device from boot if not set by configuration */
	if (bootdv == NULL) {
		bootdv = parsedisk(boot_file, strlen(boot_file), 0, &temp);
	}
	if (bootdv == NULL) {
		printf("boot device: lookup '%s' failed.\n", boot_file);
		boothowto |= RB_ASKNAME; /* Don't Panic :-) */
		/* boothowto |= RB_SINGLE; */
	} else
		printf("boot device: %s.\n", bootdv->dv_xname);

	if (boothowto & RB_ASKNAME) {
		for (;;) {
			printf("root device ");
			if (bootdv != NULL)
				 printf("(default %s%c)",
					bootdv->dv_xname,
					bootdv->dv_class == DV_DISK
						? 'a' : ' ');
			printf(": ");
			s = splhigh();
			cnpollc(1);
			len = getsn(buf, sizeof(buf));

			cnpollc(0);
			splx(s);
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
			dv = getdisk(buf, len, 0, &nrootdev);
			if (dv != NULL) {
				bootdv = dv;
				break;
			}
		}
		/*
		 * because swap must be on same device as root, for
		 * network devices this is easy.
		 */
		if (bootdv->dv_class == DV_IFNET)
			goto gotswap;

		for (;;) {
			printf("swap device ");
			if (bootdv != NULL)
				printf("(default %s%c)",
					bootdv->dv_xname,
					bootdv->dv_class == DV_DISK?'b':' ');
			printf(": ");
			s = splhigh();
			cnpollc(1);
			len = getsn(buf, sizeof(buf));
			cnpollc(0);
			splx(s);
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
		if (bootdv == NULL)
			panic("boot device not known");

		majdev = findblkmajor(bootdv);

		if (majdev >= 0) {
			/*
			 * Root and Swap are on disk.
			 * Boot is always from partition 0.
			 */
			rootdev = MAKEDISKDEV(majdev, bootdv->dv_unit, 0);
			nswapdev = MAKEDISKDEV(majdev, bootdv->dv_unit, 1);
			dumpdev = nswapdev;
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
		mindev = minor(rootdev);
		unit = DISKUNIT(rootdev);
		part = DISKPART(rootdev);
		printf("root on %s%c\n", bootdv->dv_xname, part + 'a');
		break;
	default:
		printf("can't figure root, hope your kernel is right\n");
		return;
	}

	/*
	 * XXX: What is this doing?
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
/* End of autoconf.c */
