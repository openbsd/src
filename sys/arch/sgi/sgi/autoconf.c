/*	$OpenBSD: autoconf.c,v 1.7 2004/10/20 12:49:15 pefo Exp $	*/
/*
 * Copyright (c) 1996 Per Fogelstrom
 * Copyright (c) 1995 Theo de Raadt
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 */

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time.  Available
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

#include <machine/autoconf.h>
#include <mips64/archtype.h>

struct  device *parsedisk(char *, int, int, dev_t *);
void	disk_configure(void);
void    rootconf(void);
void	configure(void);
void	swapconf(void);
extern void dumpconf(void);
static int findblkmajor(struct device *);
static struct device * getdisk(char *, int, int, dev_t *);
struct device *getdevunit(char *, int);
struct devmap *boot_findtype(char *);
int makebootdev(const char *, int);
const char *boot_get_path_component(const char *, char *, int *);
const char *boot_getnr(const char *, int *);

/* Struct translating from ARCS to bsd. */
struct devmap {
	char	*att;
	char	*dev;
	int	what;
};
#define	DEVMAP_TYPE	0x01
#define	DEVMAP_UNIT	0x02
#define	DEVMAP_PART	0x04

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	cold = 1;			/* if 1, still working on cold-start */
char	bootdev[16] = "unknown";	/* to hold boot dev name */
struct device *bootdv = NULL;

/*
 *  Configure all devices found that we know about.
 *  This is done at boot time.
 */
void
cpu_configure()
{
	(void)splhigh();	/* Set mask to what we intend. */
	if (config_rootfound("mainbus", "mainbus") == 0) {
		panic("no mainbus found");
	}

	splinit();		/* Initialized, fire up interrupt system */

	md_diskconf = disk_configure;
	cold = 0;
}

void
disk_configure()
{
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
	struct swdevt *swp;
	int nblks;

	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		if (bdevsw[major(swp->sw_dev)].d_psize) {
			nblks = (*bdevsw[major(swp->sw_dev)].d_psize)(swp->sw_dev);
			if (nblks != -1 && (swp->sw_nblks == 0 || swp->sw_nblks > nblks)) {
				swp->sw_nblks = nblks;
			}
			swp->sw_nblks = ctod(dtoc(swp->sw_nblks));
		}
	}
}

/*
 * the rest of this file was influenced/copied from Theo de Raadt's
 * code in the sparc port to nuke the "options GENERIC" stuff.
 */

static	struct nam2blk {
	char *name;
	int  maj;
} nam2blk[] = {
	{ "sd",	0 },	/* 0 = sd */
	{ "wd",	4 },	/* 4 = wd */
};

static int
findblkmajor(dv)
	struct device *dv;
{
	char *name = dv->dv_xname;
	int i;

	for (i = 0; i < sizeof(nam2blk)/sizeof(nam2blk[0]); ++i)
		if (strncmp(name, nam2blk[i].name, strlen(nam2blk[0].name)) == 0)
			return (nam2blk[i].maj);
	 return (-1);
}

static struct device *
getdisk(str, len, defpart, devp)
	char *str;
	int len, defpart;
	dev_t *devp;
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
parsedisk(str, len, defpart, devp)
	char *str;
	int len, defpart;
	dev_t *devp;
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

#if defined(NFSCLIENT)
	extern char *nfsbootdevname;
#endif

	if (boothowto & RB_DFLTROOT)
		return;		/* Boot compiled in */

	/* Lookup boot device from boot if not set by configuration */
	if (bootdv == NULL) {
		bootdv = parsedisk(bootdev, strlen(bootdev), 0, &temp);
	}
	if (bootdv == NULL) {
		printf("boot device: lookup '%s' failed.\n", bootdev);
		boothowto |= RB_ASKNAME; /* Don't Panic :-) */
	}
	else {
		printf("boot device: %s.\n", bootdv->dv_xname);
	}

	if (boothowto & RB_ASKNAME) {
		for (;;) {
			printf("root device ");
			if (bootdv != NULL)
				 printf("(default %s%c)",
					bootdv->dv_xname,
					bootdv->dv_class == DV_DISK
						? 'a' : ' ');
			printf(": ");
			len = getsn(buf, sizeof(buf));
#ifdef DDB
			if (len && strcmp(buf, "ddb") == 0) {
				Debugger();
				continue;
			}
#endif
			if (len == 0 && bootdv != NULL) {
				strlcpy(buf, bootdv->dv_xname, sizeof(buf));
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
			len = getsn(buf, sizeof(buf));
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
		}
		else {
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

/*
 * find a device matching "name" and unit number
 */
struct device *
getdevunit(name, unit)
	char *name;
	int unit;
{
	struct device *dev = alldevs.tqh_first;
	char num[10], fullname[16];
	int lunit;

	/* compute length of name and decimal expansion of unit number */
	snprintf(num, sizeof(num), "%d", unit);
	lunit = strlen(num);
	if (strlen(name) + lunit >= sizeof(fullname) - 1)
		panic("config_attach: device name too long");

	strlcpy(fullname, name, sizeof(fullname));
	strlcat(fullname, num, sizeof(fullname));

	while (strcmp(dev->dv_xname, fullname) != 0) {
		if ((dev = dev->dv_list.tqe_next) == NULL)
			return NULL;
	}
	return dev;
}

struct devmap *
boot_findtype(char *s)
{
	static struct devmap devmap[] = {
		{ "scsi",	"sd",	DEVMAP_TYPE },
		{ "disk",	"",	DEVMAP_UNIT },
		{ "part",	"",	DEVMAP_PART },
		{ "partition",	"",	DEVMAP_PART },
		{ NULL, NULL }
	};
	struct devmap *dp = &devmap[0];

	while (dp->att) {
		if (strcmp (s, dp->att) == 0) {
			break;
		}
		dp++;
	}
	if (dp->att)
		return dp ;
	else
		return NULL;
}

/*
 * Look at the string 'bp' and decode the boot device.
 * Boot devices look like: 'scsi()disk(n)rdisk()partition(0)'
 *	 		  or
 *			   'dksc(0,1,0)'
 */
int
makebootdev(const char *bp, int offs)
{
	char namebuf[256];
	const char *cp, *ncp, *ecp, *devname;
	int	i, unit, partition;
	struct devmap *dp;

	if (bp == NULL)
		return -1;

	ecp = cp = bp;
	unit = partition = 0;
	devname = NULL;

	if (strncmp(cp, "dksc(", 5) == 0) {
		devname = "sd";
		cp += 5;
		cp = boot_getnr(cp, &i);
		if (*cp == ',') {
			cp = boot_getnr(cp, &i);
			unit = i - 1;
			if (*cp == ',') {
				cp = boot_getnr(cp, &i);
				partition = i;
			}
		}
	} else {
		ncp = boot_get_path_component(cp, namebuf, &i);
		while (ncp != NULL) {
			if ((dp = boot_findtype(namebuf)) != NULL) {
				switch(dp->what) {
				case DEVMAP_TYPE:
					devname = dp->dev;
					break;
				case DEVMAP_UNIT:
					unit = i - 1 + offs;
					break;
			case DEVMAP_PART:
					partition = i;
					break;
				}
			}
			cp = ncp;
			ncp = boot_get_path_component(cp, namebuf, &i);
		}
	}

	if (devname == NULL) {
		return -1;
	}

	snprintf(bootdev, sizeof(bootdev), "%s%d%c", devname, unit, 'a');
	return 0;
}

const char *
boot_get_path_component(const char *p, char *comp, int *no)
{
	while (*p && *p != '(') {
		*comp++ = *p++;
	}
	*comp = '\0';

	if (*p == NULL)
		return NULL;

	*no = 0;
	p++;
	while (*p && *p != ')') {
		if (*p >= '0' && *p <= '9')
			*no = *no * 10 + *p++ - '0';
		else
			return NULL;
	}
	return ++p;
}

const char *
boot_getnr(const char *p, int *no)
{
	*no = 0;
	while (*p >= '0' && *p <= '9')
		*no = *no * 10 + *p++ - '0';
	return p;
}
