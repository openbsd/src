/*	$OpenBSD: autoconf.c,v 1.28 2004/12/30 21:26:14 miod Exp $	*/
/*	$NetBSD: autoconf.c,v 1.45 1999/04/10 17:31:02 kleink Exp $	*/

/*
 * Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
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
 * from: Utah $Hdr: autoconf.c 1.36 92/12/20$
 *
 *	@(#)autoconf.c	8.2 (Berkeley) 1/12/94
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
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/malloc.h>
#include <sys/extent.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/reboot.h>
#include <sys/tty.h>

#include <dev/cons.h>

#include <machine/autoconf.h>
#include <machine/vmparam.h>
#include <machine/cpu.h>
#include <machine/hp300spu.h>
#include <machine/intr.h>
#include <machine/pte.h>

#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>

#include <hp300/dev/dmavar.h>
#include <hp300/dev/grfreg.h>
#include <hp300/dev/hilreg.h>
#include <hp300/dev/hilioctl.h>
#include <hp300/dev/hilvar.h>

#include <hp300/dev/hpibvar.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */

struct	extent *extio;

extern	caddr_t internalhpib;
extern	char *extiobase;

/* The boot device. */
struct	device *booted_device;

/* The device we mount as root. */
struct	device *root_device;

/* How we were booted. */
u_int	bootdev;

/*
 * This information is built during the autoconfig process.
 * A little explanation about the way this works is in order.
 *
 *	device_register() links all devices into dev_data_list.
 *	If the device is an hpib controller, it is also linked
 *	into dev_data_list_hpib.  If the device is a scsi controller,
 *	it is also linked into dev_data_list_scsi.
 *
 *	dev_data_list_hpib and dev_data_list_scsi are sorted
 *	by select code, from lowest to highest.
 *
 *	After autoconfiguration is complete, we need to determine
 *	which device was the boot device.  The boot block assigns
 *	controller unit numbers in order of select code.  Thus,
 *	providing the controller is configured in the kernel, we
 *	can determine our version of controller unit number from
 *	the sorted hpib/scsi list.
 *
 *	At this point, we know the controller (device type
 *	encoded in bootdev tells us "scsi disk", or "hpib tape",
 *	etc.).  The next step is to find the device which
 *	has the following properties:
 *
 *		- A child of the boot controller.
 *		- Same slave as encoded in bootdev.
 *		- Same physical unit as encoded in bootdev.
 *
 *	Later, after we've set the root device in stone, we
 *	reverse the process to re-encode bootdev so it can be
 *	passed back to the boot block.
 */
struct dev_data {
	LIST_ENTRY(dev_data)	dd_list;  /* dev_data_list */
	LIST_ENTRY(dev_data)	dd_clist; /* ctlr list */
	struct device		*dd_dev;  /* device described by this entry */
	int			dd_scode; /* select code of device */
	int			dd_slave; /* ...or slave */
	int			dd_punit; /* and punit... */
};
typedef LIST_HEAD(, dev_data) ddlist_t;
ddlist_t	dev_data_list;	  	/* all dev_datas */
ddlist_t	dev_data_list_hpib;	/* hpib controller dev_datas */
ddlist_t	dev_data_list_scsi;	/* scsi controller dev_datas */

void	setroot(void);
void	swapconf(void);
void	findbootdev(void);
void	findbootdev_slave(ddlist_t *, int, int, int);
void	setbootdev(void);

static	struct dev_data *dev_data_lookup(struct device *);
static	void dev_data_insert(struct dev_data *, ddlist_t *);

static	struct device *parsedisk(char *str, int len, int defpart,
	    dev_t *devp);
static	struct device *getdisk(char *str, int len, int defpart,
	    dev_t *devp);
static	int findblkmajor(struct device *dv);
static	char *findblkname(int);
static	int getstr(char *cp, int size);  

int	mainbusmatch(struct device *, void *, void *);
void	mainbusattach(struct device *, struct device *, void *);
int	mainbussearch(struct device *, void *, void *);

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbusmatch, mainbusattach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

int
mainbusmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	static int mainbus_matched = 0;

	/* Allow only one instance. */
	if (mainbus_matched)
		return (0);

	mainbus_matched = 1;
	return (1);
}

void
mainbusattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{

	printf("\n");

	/* Search for and attach children. */
	config_search(mainbussearch, self, NULL);
}

int
mainbussearch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;

	if ((*cf->cf_attach->ca_match)(parent, cf, NULL) > 0)
		config_attach(parent, cf, NULL, NULL);
	return (0);
}

/*
 * Determine the device configuration for the running system.
 */
void
cpu_configure()
{
	/*
	 * Initialize the dev_data_lists.
	 */
	LIST_INIT(&dev_data_list);
	LIST_INIT(&dev_data_list_hpib);
	LIST_INIT(&dev_data_list_scsi);

	/*
	 * XXX In order for the HIL to configure, interrupts need to be
	 * XXX enabled.  However, we need to initialize the HIL driver's
	 * XXX software state prior to that, since a pending interrupt
	 * XXX might cause the HIL's interrupt handler to be run in an
	 * XXX uninitialized environment otherwise.
	 *
	 * XXX These should be consolidated into some kind of table.
	 */
	hilsoftinit(0, HILADDR);
	(void)spl0();
	hilinit(0, HILADDR);

	(void)splhigh();
	if (config_rootfound("mainbus", "mainbus") == NULL)
		panic("no mainbus found");
	(void)spl0();

	intr_printlevels();

	/*
	 * Find boot device.
	 */
	if ((bootdev & B_MAGICMASK) != B_DEVMAGIC) {
		printf("WARNING: boot program didn't supply boot device.\n");
		printf("Please update your boot program.\n");
	} else {
		findbootdev();
		if (booted_device == NULL) {
			printf("WARNING: can't find match for bootdev:\n");
			printf(
		    "type = %d, ctlr = %d, slave = %d, punit = %d, part = %d\n",
			    B_TYPE(bootdev), B_ADAPTOR(bootdev),
			    B_CONTROLLER(bootdev), B_UNIT(bootdev),
			    B_PARTITION(bootdev));
			bootdev = 0;		/* invalidate bootdev */
		} else {
			printf("boot device: %s\n", booted_device->dv_xname);
		}
	}

	setroot();
	swapconf();

	/*
	 * Set bootdev based on how we mounted root.
	 * This is given to the boot program when we reboot.
	 */
	setbootdev();

	cold = 0;
}

/**********************************************************************
 * Code to find and set the boot device
 **********************************************************************/

/*
 * Register a device.  We're passed the device and the arguments
 * used to attach it.  This is used to find the boot device.
 */
void
device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	struct dev_data *dd;
	static int seen_netdevice = 0;

	/*
	 * Allocate a dev_data structure and fill it in.
	 * This means making some tests twice, but we don't
	 * care; this doesn't really have to be fast.
	 *
	 * Note that we only really care about devices that
	 * we can mount as root.
	 */
	dd = (struct dev_data *)malloc(sizeof(struct dev_data),
	    M_DEVBUF, M_NOWAIT);
	if (dd == NULL)
		panic("device_register: can't allocate dev_data");
	bzero(dd, sizeof(struct dev_data));

	dd->dd_dev = dev;

	/*
	 * BOOTROM and boot program can really only understand
	 * using the lowest select code network interface,
	 * so we ignore all but the first.
	 */
	if (dev->dv_class == DV_IFNET && seen_netdevice == 0) {
		struct dio_attach_args *da = aux;

		seen_netdevice = 1;
		dd->dd_scode = da->da_scode;
		goto linkup;
	}

	if (strncmp(dev->dv_xname, "fhpib", 5) == 0 ||
	    strncmp(dev->dv_xname, "nhpib", 5) == 0 ||
	    strncmp(dev->dv_xname, "spc", 3) == 0) {
		struct dio_attach_args *da = aux;

		dd->dd_scode = da->da_scode;
		goto linkup;
	}

	if (strncmp(dev->dv_xname, "hd", 2) == 0) {
		struct hpibbus_attach_args *ha = aux;

		dd->dd_slave = ha->ha_slave;
		dd->dd_punit = ha->ha_punit;
		goto linkup;
	}

	if (strncmp(dev->dv_xname, "cd", 2) == 0 ||
	    strncmp(dev->dv_xname, "sd", 2) == 0 ||
	    strncmp(dev->dv_xname, "st", 2) == 0) {
		struct scsibus_attach_args *sa = aux;

		dd->dd_slave = sa->sa_sc_link->target;
		dd->dd_punit = sa->sa_sc_link->lun;
		goto linkup;
	}

	/*
	 * Didn't need the dev_data.
	 */
	free(dd, M_DEVBUF);
	return;

 linkup:
	LIST_INSERT_HEAD(&dev_data_list, dd, dd_list);

	if (strncmp(dev->dv_xname, "fhpib", 5) == 0 ||
	    strncmp(dev->dv_xname, "nhpib", 5) == 0) {
		dev_data_insert(dd, &dev_data_list_hpib);
		return;
	}

	if (strncmp(dev->dv_xname, "spc", 3) == 0) {
		dev_data_insert(dd, &dev_data_list_scsi);
		return;
	}
}

/*
 * Configure swap space and related parameters.
 */
void
swapconf()
{
	struct swdevt *swp;
	int nblks, maj;

	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		maj = major(swp->sw_dev);
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
	dumpconf();
}

struct nam2blk {
	char *name;
	int maj;
} nam2blk[] = {
	{ "ct",		0 },
	{ "hd",		2 },
	{ "sd",		4 },
	{ "st",		7 },
	{ "rd",		8 },
	{ "cd",		9 },
};

static int
findblkmajor(dv)
	struct device *dv;
{
	char *name = dv->dv_xname;
	register int i;

	for (i = 0; i < sizeof(nam2blk) / sizeof(nam2blk[0]); ++i)
		if (strncmp(name, nam2blk[i].name, strlen(nam2blk[0].name))
		    == 0)
			return (nam2blk[i].maj);
	return (-1);
}

static char *
findblkname(maj)
	int maj;
{
	register int i;

	for (i = 0; i < sizeof(nam2blk) / sizeof(nam2blk[0]); ++i)
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
		boot(RB_HALT);

	cp = str + len - 1;
	c = *cp;
	if (c >= 'a' && c <= ('a' + MAXPARTITIONS - 1)) {
		part = c - 'a';
		*cp = '\0';
	} else
		part = defpart;

	TAILQ_FOREACH(dv, &alldevs, dv_list) {
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
	register int len;
	dev_t nrootdev, nswapdev = NODEV;
	char buf[128], *rootdevname;
	dev_t temp;
	struct device *bootdv, *rootdv, *swapdv;
	int bootpartition = 0;
#ifdef NFSCLIENT
	extern char *nfsbootdevname;
#endif

	bootdv = booted_device;

	/*
	 * If 'swap generic' and we couldn't determine root device,
	 * ask the user.
	 */
	if (mountroot == NULL && bootdv == NULL)
		boothowto |= RB_ASKNAME;

	/*
	 * If bootdev is bogus, ask the user anyhow.
	 */
	if (bootdev == 0)
		boothowto |= RB_ASKNAME;
	else
		bootpartition = B_PARTITION(bootdev);

	/*
	 * If we booted from tape, ask the user.
	 */
	if (bootdv != NULL && bootdv->dv_class == DV_TAPE)
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
		 * Because swap must be on the same device type as root,
		 * for network devices this is easy.
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
				case DV_TAPE:
				case DV_TTY:
				case DV_DULL:
				case DV_CPU:
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

#ifdef NFSCLIENT
		if (mountroot == nfs_mountroot) {
			struct dev_data *dd;
			/*
			 * `root on nfs'.  Find the first network
			 * interface.
			 */
			LIST_FOREACH(dd, &dev_data_list, dd_list) {
				if (dd->dd_dev->dv_class == DV_IFNET) {
					/* Got it! */
					break;
				}
			}
			if (dd == NULL) {
				printf("no network interface for NFS root");
				panic("setroot");
			}
			root_device = dd->dd_dev;
			return;
		}
#endif
		rootdevname = findblkname(major(rootdev));
		if (rootdevname == NULL) {
			root_device = NULL;	/* no device */
			return;
		}
		bzero(buf, sizeof(buf));
		snprintf(buf, sizeof buf, "%s%d", rootdevname,
		    DISKUNIT(rootdev));
		
		TAILQ_FOREACH(dv, &alldevs, dv_list) {
			if (strcmp(buf, dv->dv_xname) == 0) {
				root_device = dv;
				break;
			}
		}
		root_device = dv;	/* NULL is fine. */
		return;
	}

	root_device = rootdv;
	switch (rootdv->dv_class) {
#ifdef NFSCLIENT
	case DV_IFNET:
		mountroot = nfs_mountroot;
		nfsbootdevname = rootdv->dv_xname;
		return;
#endif
	case DV_DISK:
		mountroot = dk_mountroot;
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
findbootdev()
{
	int type, ctlr, slave, punit;
	int scsiboot, hpibboot, netboot;
	struct dev_data *dd;

	booted_device = NULL;

	if ((bootdev & B_MAGICMASK) != B_DEVMAGIC)
		return;

	type  = B_TYPE(bootdev);
	ctlr  = B_ADAPTOR(bootdev);
	slave = B_CONTROLLER(bootdev);
	punit = B_UNIT(bootdev);

	scsiboot = (type == 4);			/* sd major */
	hpibboot = (type == 0 || type == 2);	/* ct/hd major */
	netboot  = (type == 6);			/* le - special */

	/*
	 * Check for network boot first, since it's a little
	 * different.  The BOOTROM/boot program can only boot
	 * off of the first (lowest select code) ethernet
	 * device.  device_register() knows this and only
	 * registers one DV_IFNET.  This is a safe assumption
	 * since the code that finds devices on the DIO bus
	 * always starts at scode 0 and works its way up.
	 */
	if (netboot) {
		LIST_FOREACH(dd, &dev_data_list, dd_list) {
			if (dd->dd_dev->dv_class == DV_IFNET) {
				/*
				 * Found it!
				 */
				booted_device = dd->dd_dev;
				break;
			}
		}
		return;
	}

	/*
	 * Check for HP-IB boots next.
	 */
	if (hpibboot) {
		findbootdev_slave(&dev_data_list_hpib, ctlr,
		    slave, punit);
		if (booted_device == NULL)
			return;

#ifdef DIAGNOSTIC
		/*
		 * Sanity check.
		 */
		if ((type == 0 && strncmp(booted_device->dv_xname, "ct", 2)) ||
		    (type == 2 && strncmp(booted_device->dv_xname, "hd", 2))) {
			printf("WARNING: boot device/type mismatch!\n");
			printf("device = %s, type = %d\n",
			    booted_device->dv_xname, type);
			booted_device = NULL;
		}
#endif
		return;
	}

	/*
	 * Check for SCSI boots last.
	 */
	if (scsiboot) {
		findbootdev_slave(&dev_data_list_scsi, ctlr,
		     slave, punit);
		if (booted_device == NULL)  
			return; 

#ifdef DIAGNOSTIC
		/*
		 * Sanity check.
		 */
		if (strncmp(booted_device->dv_xname, "cd", 2) != 0 &&
		    strncmp(booted_device->dv_xname, "sd", 2) != 0 &&
		    strncmp(booted_device->dv_xname, "st", 2) != 0) {
			printf("WARNING: boot device/type mismatch!\n");
			printf("device = %s, type = %d\n",
			    booted_device->dv_xname, type);
			booted_device = NULL; 
		}
#endif
		return;
	}

	/* Oof! */
	printf("WARNING: UNKNOWN BOOT DEVICE TYPE = %d\n", type);
}

void
findbootdev_slave(ddlist, ctlr, slave, punit)
	ddlist_t *ddlist;
	int ctlr, slave, punit;
{
	struct dev_data *cdd, *dd;

	/*
	 * Find the booted controller.
	 */
	for (cdd = LIST_FIRST(ddlist); ctlr != 0 && cdd != LIST_END(ddlist);
	    cdd = LIST_NEXT(cdd, dd_clist))
		ctlr--;
	if (cdd == NULL) {
		/*
		 * Oof, couldn't find it...
		 */
		return;
	}

	/*
	 * Now find the device with the right slave/punit
	 * that's a child of the controller.
	 */
	LIST_FOREACH(dd, &dev_data_list, dd_list) {
		/*
		 * "sd" / "st" / "cd" -> "scsibus" -> "spc"
		 * "hd" -> "hpibbus" -> "fhpib"
		 */
		if (dd->dd_dev->dv_parent->dv_parent != cdd->dd_dev)
			continue;

		if (dd->dd_slave == slave &&
		    dd->dd_punit == punit) {
			/*
			 * Found it!
			 */
			booted_device = dd->dd_dev;
			break;
		}
	}
}

void
setbootdev()
{
	struct dev_data *cdd, *dd;
	int type, ctlr;

	/*
	 * Note our magic numbers for type:
	 *
	 *	0 == ct
	 *	2 == hd
	 *	4 == scsi
	 *	6 == le
	 *
	 * All are bdevsw major numbers, except for le, which
	 * is just special. SCSI needs specific care since the
	 * ROM wants to see 4, but depending upon the real device
	 * we booted from, we might have a different major value.
	 */

	/*
	 * Start with a clean slate.
	 */
	bootdev = 0;

	/*
	 * If we don't have a saveable root_device, just punt.
	 */
	if (root_device == NULL)
		goto out;

	dd = dev_data_lookup(root_device);

	/*
	 * If the root device is network, we're done
	 * early.
	 */
	if (root_device->dv_class == DV_IFNET) {
		bootdev = MAKEBOOTDEV(6, 0, 0, 0, 0);
		goto out;
	}

	/*
	 * Determine device type.
	 */
	if (strncmp(root_device->dv_xname, "hd", 2) == 0)
		type = 2;
	else if (strncmp(root_device->dv_xname, "cd", 2) == 0 ||
	    strncmp(root_device->dv_xname, "sd", 2) == 0 ||
	    strncmp(root_device->dv_xname, "st", 2) == 0)
		/* force scsi disk regardless of the actual device */
		type = 4;
	else {
		printf("WARNING: strange root device!\n");
		goto out;
	}

	/*
	 * Get parent's info.
	 *
	 * "hd" -> "hpibbus" -> "fhpib"
	 * "sd" / "cd" / "st" -> "scsibus" -> "spc"
	 */
	for (cdd = LIST_FIRST(&dev_data_list_hpib), ctlr = 0;
	    cdd != LIST_END(&dev_data_list_hpib);
	    cdd = LIST_NEXT(cdd, dd_clist), ctlr++) {
		if (cdd->dd_dev == root_device->dv_parent->dv_parent) {
			/*
			 * Found it!
			 */
			bootdev = MAKEBOOTDEV(type, ctlr, dd->dd_slave,
			    dd->dd_punit, DISKPART(rootdev));
			break;
		}
	}

 out:
	/* Don't need this anymore. */
	for (dd = LIST_FIRST(&dev_data_list);
	    dd != LIST_END(&dev_data_list); ) {
		cdd = dd;
		dd = LIST_NEXT(dd, dd_list);
		free(cdd, M_DEVBUF);
	}
}

/*
 * Return the dev_data corresponding to the given device.
 */
static struct dev_data *
dev_data_lookup(dev)
	struct device *dev;
{
	struct dev_data *dd;

	LIST_FOREACH(dd, &dev_data_list, dd_list)
		if (dd->dd_dev == dev)
			return (dd);

	panic("dev_data_lookup");
}

/*
 * Insert a dev_data into the provided list, sorted by select code.
 */
static void
dev_data_insert(dd, ddlist)
	struct dev_data *dd;
	ddlist_t *ddlist;
{
	struct dev_data *de;

#ifdef DIAGNOSTIC
	if (dd->dd_scode < 0 || dd->dd_scode > 255) {
		panic("bogus select code for %s", dd->dd_dev->dv_xname);
	}
#endif

	/*
	 * Just insert at head if list is empty.
	 */
	if (LIST_EMPTY(ddlist)) {
		LIST_INSERT_HEAD(ddlist, dd, dd_clist);
		return;
	}

	/*
	 * Traverse the list looking for a device who's select code
	 * is greater than ours.  When we find it, insert ourselves
	 * into the list before it.
	 */
	for (de = LIST_FIRST(ddlist);
	    LIST_NEXT(de, dd_clist) != LIST_END(ddlist);
	    de = LIST_NEXT(de, dd_clist)) {
		if (de->dd_scode > dd->dd_scode) {
			LIST_INSERT_BEFORE(de, dd, dd_clist);
			return;
		}
	}

	/*
	 * Our select code is greater than everyone else's.  We go
	 * onto the end.
	 */
	LIST_INSERT_AFTER(de, dd, dd_clist);
}

/**********************************************************************
 * Code to find and initialize the console
 **********************************************************************/

/*
 * Scan all select codes, passing the corresponding VA to (*func)().
 * (*func)() is a driver-specific routine that looks for the console
 * hardware.
 */
void
console_scan(func, arg)
	int (*func)(int, caddr_t, void *);
	void *arg;
{
	int size, scode, sctop;
	caddr_t pa, va;

	/*
	 * Scan all select codes.  Check each location for some
	 * hardware.  If there's something there, call (*func)().
	 */
	sctop = DIO_SCMAX(machineid);
	for (scode = 0; scode < sctop; ++scode) {
		/*
		 * Abort mission if console has been forced.
		 */
		if (conforced)
			return;

		/*
		 * Skip over the select code hole and
		 * the internal HP-IB controller.
		 */
		if (((scode >= 32) && (scode < 132)) ||
		    ((scode == 7) && internalhpib))
			continue;

		/* Map current PA. */
		pa = dio_scodetopa(scode);
		va = iomap(pa, NBPG);
		if (va == 0)
			continue;

		/* Check to see if hardware exists. */
		if (badaddr(va)) {
			iounmap(va, NBPG);
			continue;
		}

		/*
		 * Hardware present, call callback.  Driver returns
		 * size of region to map if console probe successful
		 * and worthwhile.
		 */
		size = (*func)(scode, va, arg);
		iounmap(va, NBPG);
		if (size) {
			/* Free last mapping. */
			if (convasize)
				iounmap(conaddr, convasize);
			convasize = 0;

			/* Remap to correct size. */
			va = iomap(pa, size);
			if (va == 0)
				continue;

			/* Save this state for next time. */
			conscode = scode;
			conaddr = va;
			convasize = size;
		}
	}
}

/*
 * Special version of cninit().  Actually, crippled somewhat.
 * This version lets the drivers assign cn_tab.
 */
void
hp300_cninit()
{
	struct consdev *cp;
	extern struct consdev constab[];

	cn_tab = NULL;

	/*
	 * Call all of the console probe functions.
	 */
	for (cp = constab; cp->cn_probe; cp++)
		(*cp->cn_probe)(cp);

	/*
	 * No console, we can handle it.
	 */
	if (cn_tab == NULL)
		return;

	/*
	 * Turn on the console.
	 */
	(*cn_tab->cn_init)(cn_tab);
}

/**********************************************************************
 * Mapping functions
 **********************************************************************/

/*
 * Allocate/deallocate a cache-inhibited range of kernel virtual address
 * space mapping the indicated physical address range [pa - pa+size)
 */
caddr_t
iomap(pa, size)
	caddr_t pa;
	int size;
{
	int error;
	caddr_t kva;

	if (size == 0)
		return NULL;

#ifdef DEBUG
	if (((int)pa & PGOFSET) || (size & PGOFSET))
		panic("iomap: unaligned");
#endif
	error = extent_alloc(extio, size, PAGE_SIZE, 0, EX_NOBOUNDARY,
	    EX_NOWAIT | EX_MALLOCOK, (u_long *)&kva);

	if (error != 0)
		return NULL;

	physaccess(kva, pa, size, PG_RW|PG_CI);
	return(kva);
}

/*
 * Unmap a previously mapped device.
 */
void
iounmap(kva, size)
	caddr_t kva;
	int size;
{
	int error;

#ifdef DEBUG
	extern int eiomapsize;

	if (((int)kva & PGOFSET) || (size & PGOFSET))
		panic("iounmap: unaligned");
	if (kva < extiobase || kva >= extiobase + ctob(eiomapsize))
		panic("iounmap: bad address");
#endif

	physunaccess(kva, size);

	error = extent_free(extio, (u_long)kva, size, EX_NOWAIT);

	if (error != 0)
		printf("iounmap: extent_free failed\n");
}
