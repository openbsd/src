/*	$OpenBSD: autoconf.c,v 1.9 1997/01/18 06:43:05 downsj Exp $	*/
/*	$NetBSD: autoconf.c,v 1.29 1996/12/17 08:41:19 thorpej Exp $	*/

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
 * from: Utah $Hdr: autoconf.c 1.36 92/12/20$
 *
 *	@(#)autoconf.c	8.2 (Berkeley) 1/12/94
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
#include <sys/map.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/dmap.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <dev/cons.h>

#include <machine/autoconf.h>
#include <machine/vmparam.h>
#include <machine/cpu.h>
#include <machine/pte.h>

#include <hp300/hp300/isr.h>

#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>

#include <hp300/dev/device.h>
#include <hp300/dev/grfreg.h>
#include <hp300/dev/hilreg.h>

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	cold;		    /* if 1, still working on cold-start */

/* XXX must be allocated statically because of early console init */
struct	map extiomap[EIOMAPSIZE/16];

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

#if 1			/* XXX for now */
struct scsi_link {
	int	target;
	int	lun;
};

struct scsibus_attach_args {
	struct	scsi_link *sa_scsi_link;
};

struct hpib_attach_args {
	int	ha_slave;
	int	ha_punit;
};
#endif /* XXX */

#ifndef NEWCONFIG	/* XXX */
struct	hp_hw sc_table[MAXCTLRS];

#ifdef DEBUG
int	acdebug = 0;
#endif

struct	devicelist alldevs;
struct	evcntlist allevents;

struct	dio_attach_args hp300_dio_attach_args;
struct	scsi_link hp300_scsi_link;
struct	scsibus_attach_args hp300_scsibus_attach_args;
struct	hpib_attach_args hp300_hpib_attach_args;
#endif /* ! NEWCONFIG */

void	setroot __P((void));
void	swapconf __P((void));
void	findbootdev __P((void));
void	findbootdev_slave __P((ddlist_t *, int, int, int));
void	setbootdev __P((void));

static	struct dev_data *dev_data_lookup __P((struct device *));
static	void dev_data_insert __P((struct dev_data *, ddlist_t *));

static	struct device *parsedisk __P((char *str, int len, int defpart,
	    dev_t *devp));
static	struct device *getdisk __P((char *str, int len, int defpart,
	    dev_t *devp));
static	int findblkmajor __P((struct device *dv));
static	char *findblkname __P((int));
static	int getstr __P((char *cp, int size));  

#ifdef NEWCONFIG
int	mainbusmatch __P((struct device *, struct cfdata *, void *));
void	mainbusattach __P((struct device *, struct device *, void *));
int	mainbussearch __P((struct device *, struct cfdata *, void *));

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbusmatch, mainbusattach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

int
mainbusmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
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
mainbussearch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{

	if ((*cf->cf_attach->ca_match)(parent, cf, NULL) > 0)
		config_attach(parent, cf, NULL, NULL);
	return (0);
}
#endif /* NEWCONFIG */

/*
 * Determine the device configuration for the running system.
 */
void
configure()
{
	register struct hp_hw *hw;
	int found;

	/*
	 * Initialize the dev_data_lists.
	 */
	LIST_INIT(&dev_data_list);
	LIST_INIT(&dev_data_list_hpib);
	LIST_INIT(&dev_data_list_scsi);

	/* Initialize the interrupt system. */
	isrinit();

	/*
	 * XXX Enable interrupts.  We have to do this now so that the
	 * XXX HIL configures.
	 */
	(void)spl0();

	/*
	 * XXX: these should be consolidated into some kind of table
	 */
	hilsoftinit(0, HILADDR);
	hilinit(0, HILADDR);
	dmainit();

#ifdef NEWCONFIG
	(void)splhigh();
	if (config_rootfound("mainbus", "mainbus") == NULL)
		panic("no mainbus found");
	(void)spl0();
#else
	/*
	 * Find out what hardware is attached to the machine.
	 */
	find_devs();

	/*
	 * Look over each hardware device actually found and attempt
	 * to match it with an ioconf.c table entry.
	 */
	for (hw = sc_table; hw->hw_type; hw++) {
		if (HW_ISCTLR(hw))
			found = find_controller(hw);
		else
			found = find_device(hw);

		if (!found) {
			extern char *dio_devinfo __P((struct dio_attach_args *,
			    char *, size_t));
			int sc = hw->hw_sc;
			char descbuf[80];

			bzero(&hp300_dio_attach_args,
			    sizeof(hp300_dio_attach_args));
			hp300_dio_attach_args.da_scode = sc;
			hp300_dio_attach_args.da_id = hw->hw_id;
			hp300_dio_attach_args.da_secid = hw->hw_secid;
			printf("%s", dio_devinfo(&hp300_dio_attach_args,
			    descbuf, sizeof(descbuf)));
			if (sc >= 0 && sc < 256)
				printf(" at scode %d", sc);
			else
				printf(" csr at 0x%lx", (u_long)hw->hw_pa);
			printf(" not configured\n");
		}
	}
#endif /* NEWCONFIG */

	isrprintlevels();

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
	static int seen_netdevice;

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

	if (bcmp(dev->dv_xname, "hpib", 4) == 0 ||
	    bcmp(dev->dv_xname, "scsi", 4) == 0) {
		struct dio_attach_args *da = aux;

		dd->dd_scode = da->da_scode;
		goto linkup;
	}

	if (bcmp(dev->dv_xname, "rd", 2) == 0) {
		struct hpib_attach_args *ha = aux;

		dd->dd_slave = ha->ha_slave;
		dd->dd_punit = ha->ha_punit;
		goto linkup;
	}

	if (bcmp(dev->dv_xname, "sd", 2) == 0) {
		struct scsibus_attach_args *sa = aux;

		dd->dd_slave = sa->sa_scsi_link->target;
		dd->dd_punit = sa->sa_scsi_link->lun;
		goto linkup;
	}

	/*
	 * Didn't need the dev_data.
	 */
	free(dd, M_DEVBUF);
	return;

 linkup:
	LIST_INSERT_HEAD(&dev_data_list, dd, dd_list);

	if (bcmp(dev->dv_xname, "hpib", 4) == 0) {
		dev_data_insert(dd, &dev_data_list_hpib);
		return;
	}

	if (bcmp(dev->dv_xname, "scsi", 4) == 0) {
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
	{ "rd",		2 },
	{ "sd",		4 },
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
		for (dv = alldevs.tqh_first; dv != NULL;
		    dv = dv->dv_list.tqe_next) {
			if (dv->dv_class == DV_DISK)
				printf(" %s[a-h]", dv->dv_xname);
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
	extern int (*mountroot) __P((void));
	dev_t temp;
	struct device *bootdv, *rootdv, *swapdv;
	int bootpartition = 0;
#ifdef NFSCLIENT
	extern char *nfsbootdevname;
	extern int nfs_mountroot __P((void));
#endif
	extern int dk_mountroot __P((void));

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
				strcpy(buf, bootdv->dv_xname);
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
			for (dd = dev_data_list.lh_first;
			    dd != NULL; dd = dd->dd_list.le_next) {
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
			printf("unknown root device major 0x%x\n", rootdev);
			panic("setroot");
		}
		bzero(buf, sizeof(buf));
		sprintf(buf, "%s%d", rootdevname, DISKUNIT(rootdev));
		
		for (dv = alldevs.tqh_first; dv != NULL;
		    dv = dv->dv_list.tqe_next) {
			if (strcmp(buf, dv->dv_xname) == 0) {
				root_device = dv;
				break;
			}
		}
		if (dv == NULL) {
			printf("device %s (0x%x) not configured\n",
			    buf, rootdev);
			panic("setroot");
		}

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
	hpibboot = (type == 0 || type == 2);	/* ct/rd major */
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
		for (dd = dev_data_list.lh_first; dd != NULL;
		    dd = dd->dd_list.le_next) {
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

		/*
		 * Sanity check.
		 */
		if ((type == 0 && bcmp(booted_device->dv_xname, "ct", 2)) ||
		    (type == 2 && bcmp(booted_device->dv_xname, "rd", 2))) {
			printf("WARNING: boot device/type mismatch!\n");
			printf("device = %s, type = %d\n",
			    booted_device->dv_xname, type);
			booted_device = NULL;
		}
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

		/*
		 * Sanity check.
		 */
		if ((type == 4 && bcmp(booted_device->dv_xname, "sd", 2))) {
			printf("WARNING: boot device/type mismatch!\n");
			printf("device = %s, type = %d\n",
			    booted_device->dv_xname, type);
			booted_device = NULL; 
		}
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
	for (cdd = ddlist->lh_first; ctlr != 0 && cdd != NULL;
	    cdd = cdd->dd_clist.le_next)
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
	for (dd = dev_data_list.lh_first; dd != NULL;
	    dd = dd->dd_list.le_next) {
		if (dd->dd_dev->dv_parent == cdd->dd_dev &&
		    dd->dd_slave == slave &&
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
	 *	2 == rd
	 *	4 == sd
	 *	6 == le
	 *
	 * Allare bdevsw major numbers, except for le, which
	 * is just special.
	 *
	 * We can't mount root on a tape, so we ignore those.
	 */

	/*
	 * Start with a clean slate.
	 */
	bootdev = 0;

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
	if (bcmp(root_device->dv_xname, "rd", 2) == 0)
		type = 2;
	else if (bcmp(root_device->dv_xname, "sd", 2) == 0)
		type = 4;
	else {
		printf("WARNING: strange root device!\n");
		goto out;
	}

	/*
	 * Get parent's info.
	 */
	cdd = dev_data_lookup(root_device->dv_parent);
	switch (type) {
	case 2:
		for (cdd = dev_data_list_hpib.lh_first, ctlr = 0;
		    cdd != NULL; cdd = cdd->dd_clist.le_next, ctlr++) {
			if (cdd->dd_dev == root_device->dv_parent) {
				/*
				 * Found it!
				 */
				bootdev = MAKEBOOTDEV(type,
				    ctlr, dd->dd_slave, dd->dd_punit,
				    DISKPART(rootdev));
				break;
			}
		}
		break;

	case 4:
		for (cdd = dev_data_list_scsi.lh_first, ctlr = 0;
		    cdd != NULL; cdd = cdd->dd_clist.le_next, ctlr++) { 
			if (cdd->dd_dev == root_device->dv_parent) {
				/*
				 * Found it!
				 */
				bootdev = MAKEBOOTDEV(type,
				    ctlr, dd->dd_slave, dd->dd_punit,
				    DISKPART(rootdev));
				break;
			}
		}
		break;
	}

 out:
	/* Don't need this anymore. */
	for (dd = dev_data_list.lh_first; dd != NULL; ) {
		cdd = dd;
		dd = dd->dd_list.le_next;
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

	for (dd = dev_data_list.lh_first; dd != NULL; dd = dd->dd_list.le_next)
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
		printf("bogus select code for %s\n", dd->dd_dev->dv_xname);
		panic("dev_data_insert");
	}
#endif

	de = ddlist->lh_first;

	/*
	 * Just insert at head if list is empty.
	 */
	if (de == NULL) {
		LIST_INSERT_HEAD(ddlist, dd, dd_clist);
		return;
	}

	/*
	 * Traverse the list looking for a device who's select code
	 * is greater than ours.  When we find it, insert ourselves
	 * into the list before it.
	 */
	for (; de->dd_clist.le_next != NULL; de = de->dd_clist.le_next) {
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
	int (*func) __P((int, caddr_t, void *));
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
	int ix, npf;
	caddr_t kva;

#ifdef DEBUG
	if (((int)pa & PGOFSET) || (size & PGOFSET))
		panic("iomap: unaligned");
#endif
	npf = btoc(size);
	ix = rmalloc(extiomap, npf);
	if (ix == 0)
		return(0);
	kva = extiobase + ctob(ix-1);
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
	int ix;

#ifdef DEBUG
	if (((int)kva & PGOFSET) || (size & PGOFSET))
		panic("iounmap: unaligned");
	if (kva < extiobase || kva >= extiobase + ctob(EIOMAPSIZE))
		panic("iounmap: bad address");
#endif
	physunaccess(kva, size);
	ix = btoc(kva - extiobase) + 1;
	rmfree(extiomap, btoc(size), ix);
}

/**********************************************************************
 * Old-style device configuration code
 **********************************************************************/

#ifndef NEWCONFIG

/*
 * Duplicate of the same in subr_autoconf.c
 */
void
config_init()
{

	TAILQ_INIT(&alldevs);
	TAILQ_INIT(&allevents);
}

#define dr_type(d, s)	\
	(strcmp((d)->d_name, (s)) == 0)

#define same_hw_ctlr(hw, hc) \
	(HW_ISHPIB(hw) && dr_type((hc)->hp_driver, "hpib") || \
	 HW_ISSCSI(hw) && dr_type((hc)->hp_driver, "scsi"))

find_controller(hw)
	register struct hp_hw *hw;
{
	register struct hp_ctlr *hc;
	struct hp_ctlr *match_c;
	caddr_t oaddr;
	int sc;

#ifdef DEBUG
	if (acdebug)
		printf("find_controller: hw: id%x at sc%d (%x), type %x...",
		       hw->hw_id, hw->hw_sc, hw->hw_kva, hw->hw_type);
#endif
	sc = hw->hw_sc;
	match_c = NULL;
	for (hc = hp_cinit; hc->hp_driver; hc++) {
		if (hc->hp_alive)
			continue;
		/*
		 * Make sure we are looking at the right
		 * controller type.
		 */
		if (!same_hw_ctlr(hw, hc))
			continue;
		/*
		 * Exact match; all done
		 */
		if ((int)hc->hp_addr == sc) {
			match_c = hc;
			break;
		}
		/*
		 * Wildcard; possible match so remember first instance
		 * but continue looking for exact match.
		 */
		if (hc->hp_addr == NULL && match_c == NULL)
			match_c = hc;
	}
#ifdef DEBUG
	if (acdebug) {
		if (match_c)
			printf("found %s%d\n",
			       match_c->hp_driver->d_name,
			       match_c->hp_unit);
		else
			printf("not found\n");
	}
#endif
	/*
	 * Didn't find an ioconf entry for this piece of hardware,
	 * just ignore it.
	 */
	if (match_c == NULL)
		return(0);
	/*
	 * Found a configuration match, now let's see if the hardware
	 * agrees with us.  If it does, attach it.
	 */
	hc = match_c;
	oaddr = hc->hp_addr;
	hc->hp_addr = hw->hw_kva;
	hc->hp_args = hw;
	if ((*hc->hp_driver->d_match)(hc)) {
		hc->hp_alive = 1;

		/*
		 * Fill in fake device structure.
		 */
		bzero(&hc->hp_dev, sizeof(hc->hp_dev));
		hc->hp_dev.dv_unit = hc->hp_unit;
		sprintf(hc->hp_dev.dv_xname, "%s%d", hc->hp_driver->d_name,
		    hc->hp_unit);
		hc->hp_dev.dv_class = DV_DULL;	/* all controllers are dull */
		TAILQ_INSERT_TAIL(&alldevs, &hc->hp_dev, dv_list);

		/* Print what we've found. */
		printf("%s at ", hc->hp_xname);
		sc = hw->hw_sc;
		if (sc >= 0 && sc < 256)
			printf("scode %d", sc);
		else
			printf("addr 0x%lx,", (u_long)hw->hw_pa);
		printf(" ipl %d", hc->hp_ipl);
		if (hc->hp_flags)
			printf(" flags 0x%x", hc->hp_flags);

		/*
		 * Call device "attach" routine.  It will print the
		 * newline for us.
		 */
		(*hc->hp_driver->d_attach)(hc);

		/*
		 * Register device.  Do this after attach because
		 * we need dv_class.
		 */
		hp300_dio_attach_args.da_scode = sc;
		device_register(&hc->hp_dev, &hp300_dio_attach_args);

		find_slaves(hc);	/* XXX do this in attach? */
	} else
		hc->hp_addr = oaddr;
	return(1);
}

find_device(hw)
	register struct hp_hw *hw;
{
	register struct hp_device *hd;
	struct hp_device *match_d;
	caddr_t oaddr;
	int sc;

#ifdef DEBUG
	if (acdebug)
		printf("find_device: hw: id%x at sc%d (%x), type %x...",
		       hw->hw_id, hw->hw_sc, hw->hw_kva, hw->hw_type);
#endif
	match_d = NULL;
	for (hd = hp_dinit; hd->hp_driver; hd++) {
		if (hd->hp_alive)
			continue;
		/* Must not be a slave */
		if (hd->hp_cdriver)
			continue;
		/*
		 * XXX: A graphics device that was found as part of the
		 * console init will have the hp_addr field already set
		 * (i.e. no longer the select code).  Gotta perform a
		 * slightly different check for an exact match.
		 */
		if (HW_ISDEV(hw, D_BITMAP) && hd->hp_addr >= intiobase) {
			/* must be an exact match */
			if (hd->hp_addr == hw->hw_kva) {
				match_d = hd;
				break;
			}
			continue;
		}
		sc = (int) hd->hp_addr;
		/*
		 * Exact match; all done.
		 */
		if (sc > 0 && sc == hw->hw_sc) {
			match_d = hd;
			break;
		}
		/*
		 * Wildcard; possible match so remember first instance
		 * but continue looking for exact match.
		 */
		if (sc == 0 && same_hw_device(hw, hd) && match_d == NULL)
			match_d = hd;
	}
#ifdef DEBUG
	if (acdebug) {
		if (match_d)
			printf("found %s%d\n",
			       match_d->hp_driver->d_name,
			       match_d->hp_unit);
		else
			printf("not found\n");
	}
#endif
	/*
	 * Didn't find an ioconf entry for this piece
	 * of hardware, just ignore it.
	 */
	if (match_d == NULL)
		return(0);
	/*
	 * Found a configuration match, now let's see if the hardware
	 * agrees with us.  If it does, attach it.
	 */
	hd = match_d;
	oaddr = hd->hp_addr;
	hd->hp_addr = hw->hw_kva;
	hd->hp_args = hw;
	if ((*hd->hp_driver->d_match)(hd)) {
		hd->hp_alive = 1;

		/*
		 * Fill in fake device structure.
		 */
		bzero(&hd->hp_dev, sizeof(sizeof hd->hp_dev));
		hd->hp_dev.dv_unit = hd->hp_unit;
		sprintf(hd->hp_dev.dv_xname, "%s%d", hd->hp_driver->d_name,
		    hd->hp_unit);
		/*
		 * Default to dull, driver attach will override if
		 * necessary.
		 */
		hd->hp_dev.dv_class = DV_DULL;
		TAILQ_INSERT_TAIL(&alldevs, &hd->hp_dev, dv_list);

		/* Print what we've found. */
		printf("%s at ", hd->hp_xname);
		sc = hw->hw_sc;
		if (sc >= 0 && sc < 256)
			printf("scode %d", sc);
		else
			printf("addr 0x%lx", (u_long)hw->hw_pa);
		if (hd->hp_ipl)
			printf(" ipl %d", hd->hp_ipl);
		if (hd->hp_flags)
			printf(" flags 0x%x", hd->hp_flags);

		/*
		 * Call device "attach" routine.  It will print the
		 * newline for us.
		 */
		(*hd->hp_driver->d_attach)(hd);

		/*
		 * Register device.  Do this after attach because we
		 * need dv_class.
		 */
		hp300_dio_attach_args.da_scode = sc;
		device_register(&hd->hp_dev, &hp300_dio_attach_args);
	} else
		hd->hp_addr = oaddr;
	return(1);
}

find_slaves(hc)
	struct hp_ctlr *hc;
{
	/*
	 * The SCSI bus is structured very much like the HP-IB 
	 * except that the host adaptor is slave 7 so we only want
	 * to look at the first 6 slaves.
	 */
	if (dr_type(hc->hp_driver, "hpib"))
		find_busslaves(hc, 0, MAXSLAVES-1);
	else if (dr_type(hc->hp_driver, "scsi"))
#ifdef SCSI_REVPRI
		/*
		 * Later releases of the HP boot ROM start searching for
		 * boot devices starting with slave 6 and working down.
		 * This is apparently the order in which priority is given
		 * to slaves on the host adaptor.
		 */
		find_busslaves(hc, MAXSLAVES-2, 0);
#else
		find_busslaves(hc, 0, MAXSLAVES-2);
#endif
}

/*
 * Search each BUS controller found for slaves attached to it.
 * The bad news is that we don't know how to uniquely identify all slaves
 * (e.g. PPI devices on HP-IB).  The good news is that we can at least
 * differentiate those from slaves we can identify.  At worst (a totally
 * wildcarded entry) this will cause us to locate such a slave at the first
 * unused position instead of where it really is.  To save grief, non-
 * identifing devices should always be fully qualified.
 */
find_busslaves(hc, startslave, endslave)
	register struct hp_ctlr *hc;
	int startslave, endslave;
{
	register int s;
	register struct hp_device *hd;
	struct hp_device *match_s;
	int new_s, new_c, old_s, old_c;
	int rescan;
	
#define NEXTSLAVE(s) (startslave < endslave ? (s)++ : (s)--)
#define LASTSLAVE(s) (startslave < endslave ? (s)-- : (s)++)
#ifdef DEBUG
	if (acdebug)
		printf("find_busslaves: for %s\n", hc->hp_xname);
#endif
	NEXTSLAVE(endslave);
	for (s = startslave; s != endslave; NEXTSLAVE(s)) {
		rescan = 1;
		match_s = NULL;
		for (hd = hp_dinit; hd->hp_driver; hd++) {
			/*
			 * Rule out the easy ones:
			 * 1. slave already assigned or not a slave
			 * 2. not of the proper type
			 * 3. controller specified but not this one
			 * 4. slave specified but not this one
			 */
			if (hd->hp_alive || hd->hp_cdriver == NULL)
				continue;
			if (!dr_type(hc->hp_driver, hd->hp_cdriver->d_name))
				continue;
			if (hd->hp_ctlr >= 0 && hd->hp_ctlr != hc->hp_unit)
				continue;
			if (hd->hp_slave >= 0 && hd->hp_slave != s)
				continue;
			/*
			 * Case 0: first possible match.
			 * Remember it and keep looking for better.
			 */
			if (match_s == NULL) {
				match_s = hd;
				new_c = hc->hp_unit;
				new_s = s;
				continue;
			}
			/*
			 * Case 1: exact match.
			 * All done.  Note that we do not attempt any other
			 * matches if this one fails.  This allows us to
			 * "reserve" locations for dynamic addition of
			 * disk/tape drives by fully qualifing the location.
			 */
			if (hd->hp_slave == s && hd->hp_ctlr == hc->hp_unit) {
				match_s = hd;
				rescan = 0;
				break;
			}
			/*
			 * Case 2: right controller, wildcarded slave.
			 * Remember first and keep looking for an exact match.
			 */
			if (hd->hp_ctlr == hc->hp_unit &&
			    match_s->hp_ctlr < 0) {
				match_s = hd;
				new_s = s;
				continue;
			}
			/*
			 * Case 3: right slave, wildcarded controller.
			 * Remember and keep looking for a better match.
			 */
			if (hd->hp_slave == s &&
			    match_s->hp_ctlr < 0 && match_s->hp_slave < 0) {
				match_s = hd;
				new_c = hc->hp_unit;
				continue;
			}
			/*
			 * OW: we had a totally wildcarded spec.
			 * If we got this far, we have found a possible
			 * match already (match_s != NULL) so there is no
			 * reason to remember this one.
			 */
			continue;
		}
		/*
		 * Found a match.  We need to set hp_ctlr/hp_slave properly
		 * for the init routines but we also need to remember all
		 * the old values in case this doesn't pan out.
		 */
		if (match_s) {
			hd = match_s;
			old_c = hd->hp_ctlr;
			old_s = hd->hp_slave;
			if (hd->hp_ctlr < 0)
				hd->hp_ctlr = new_c;
			if (hd->hp_slave < 0)
				hd->hp_slave = new_s;
#ifdef DEBUG
			if (acdebug)
				printf("looking for %s%d at slave %d...",
				       hd->hp_driver->d_name,
				       hd->hp_unit, hd->hp_slave);
#endif

			if ((*hd->hp_driver->d_match)(hd)) {
#ifdef DEBUG
				if (acdebug)
					printf("found\n");
#endif
				/*
				 * Fill in fake device strcuture.
				 */
				bzero(&hd->hp_dev, sizeof(hd->hp_dev));
				hd->hp_dev.dv_unit = hd->hp_unit;
				sprintf(hd->hp_dev.dv_xname, "%s%d",
				    hd->hp_driver->d_name,
				    hd->hp_unit);
				/*
				 * Default to dull, driver attach will
				 * override if necessary.
				 */
				hd->hp_dev.dv_class = DV_DULL;
				hd->hp_dev.dv_parent = &hc->hp_dev;
				TAILQ_INSERT_TAIL(&alldevs, &hd->hp_dev,
				    dv_list);

				/*
				 * Print what we've found.  Note that
				 * for `slave' devices, the flags are
				 * overloaded with the phys. unit
				 * locator.  They aren't used for anything
				 * else, so we always treat them as
				 * such.  This is a hack to make things
				 * a little more clear to folks configuring
				 * kernels and reading boot messages.
				 */
				printf("%s at %s slave %d punit %d",
				       hd->hp_xname, hc->hp_xname,
				       hd->hp_slave, hd->hp_flags);
				hd->hp_alive = 1;
				rescan = 1;

				/*
				 * Call the device "attach" routine.
				 * It will print the newline for us.
				 */
				 (*hd->hp_driver->d_attach)(hd);

				/*
				 * Register device.  Do this after attach
				 * because we need dv_class.
				 */
				if (dr_type(hc->hp_driver, "scsi")) {
					hp300_scsi_link.target = hd->hp_slave;
					hp300_scsi_link.lun = hd->hp_flags;
					hp300_scsibus_attach_args.sa_scsi_link=
					    &hp300_scsi_link;
					device_register(&hd->hp_dev,
					    &hp300_scsibus_attach_args);
				} else {
					hp300_hpib_attach_args.ha_slave =
					    hd->hp_slave;
					hp300_hpib_attach_args.ha_punit =
					    hd->hp_flags;
					device_register(&hd->hp_dev,
					    &hp300_hpib_attach_args);
				}
			} else {
#ifdef DEBUG
				if (acdebug)
					printf("not found\n");
#endif
				hd->hp_ctlr = old_c;
				hd->hp_slave = old_s;
			}
			/*
			 * XXX: This should be handled better.
			 * Re-scan a slave.  There are two reasons to do this.
			 * 1. It is possible to have both a tape and disk
			 *    (e.g. 7946) or two disks (e.g. 9122) at the
			 *    same slave address.  Here we need to rescan
			 *    looking only at entries with a different
			 *    physical unit number (hp_flags).
			 * 2. It is possible that an init failed because the
			 *    slave was there but of the wrong type.  In this
			 *    case it may still be possible to match the slave
			 *    to another ioconf entry of a different type.
			 *    Here we need to rescan looking only at entries
			 *    of different types.
			 * In both cases we avoid looking at undesirable
			 * ioconf entries of the same type by setting their
			 * alive fields to -1.
			 */
			if (rescan) {
				for (hd = hp_dinit; hd->hp_driver; hd++) {
					if (hd->hp_alive)
						continue;
					if (match_s->hp_alive == 1) {	/* 1 */
						if (hd->hp_flags == match_s->hp_flags)
							hd->hp_alive = -1;
					} else {			/* 2 */
						if (hd->hp_driver == match_s->hp_driver)
							hd->hp_alive = -1;
					}
				}
				LASTSLAVE(s);
				continue;
			}
		}
		/*
		 * Reset bogon alive fields prior to attempting next slave
		 */
		for (hd = hp_dinit; hd->hp_driver; hd++)
			if (hd->hp_alive == -1)
				hd->hp_alive = 0;
	}
#undef NEXTSLAVE
#undef LASTSLAVE
}

same_hw_device(hw, hd)
	struct hp_hw *hw;
	struct hp_device *hd;
{
	int found = 0;

	switch (hw->hw_type & ~B_MASK) {
	case C_HPIB:
		found = dr_type(hd->hp_driver, "hpib");
		break;
	case C_SCSI:
		found = dr_type(hd->hp_driver, "scsi");
		break;
	case D_BITMAP:
		found = dr_type(hd->hp_driver, "grf");
		break;
	case D_LAN:
		found = dr_type(hd->hp_driver, "le");
		break;
	case D_COMMDCA:
		found = dr_type(hd->hp_driver, "dca");
		break;
	case D_COMMDCL:
		found = dr_type(hd->hp_driver, "dcl");
		break;
	case D_COMMDCM:
		found = dr_type(hd->hp_driver, "dcm");
		break;
	default:
		break;
	}
	return(found);
}

char notmappedmsg[] = "WARNING: no space to map IO card, ignored\n";

/*
 * Scan the IO space looking for devices.
 */
find_devs()
{
	short sc;
	u_char *id_reg;
	register caddr_t addr;
	register struct hp_hw *hw = sc_table;
	int didmap, sctop;

	/*
	 * Probe all select codes + internal display addr
	 */
	sctop = DIO_SCMAX(machineid);
	for (sc = -1; sc < sctop; sc++) {
		/*
		 * Invalid select codes
		 */
		if (sc >= 32 && sc < 132)
			continue;

		if (sc == -1) {
			hw->hw_pa = (caddr_t) GRFIADDR;
			addr = (caddr_t) IIOV(hw->hw_pa);
			didmap = 0;
		} else if (sc == 7 && internalhpib) {
			hw->hw_pa = (caddr_t)DIO_IHPIBADDR;
			addr = internalhpib = (caddr_t) IIOV(hw->hw_pa);
			didmap = 0;
		} else if (sc == conscode) {
			/*
			 * If this is the console, it's already been
			 * mapped, and the address is known.
			 */
			hw->hw_pa = dio_scodetopa(sc);
			addr = conaddr;
			didmap = 0;
		} else {
			hw->hw_pa = dio_scodetopa(sc);
			addr = iomap(hw->hw_pa, NBPG);
			if (addr == 0) {
				printf(notmappedmsg);
				continue;
			}
			didmap = 1;
		}
		if (badaddr(addr)) {
			if (didmap)
				iounmap(addr, NBPG);
			continue;
		}

		hw->hw_size = DIO_SIZE(sc, addr);
		hw->hw_kva = addr;
		hw->hw_id = DIO_ID(addr);
		if (DIO_ISFRAMEBUFFER(hw->hw_id))
			hw->hw_secid = DIO_SECID(addr);
		hw->hw_sc = sc;

		/*
		 * Internal HP-IB on some machines (345/375) doesn't return
		 * consistant id info so we use the info gleaned from the
		 * boot ROMs SYSFLAG.
		 */
		if (sc == 7 && internalhpib) {
			hw->hw_type = C_HPIB;
			hw++;
			continue;
		}
		/*
		 * XXX: the following could be in a big static table
		 */
		switch (hw->hw_id) {
		/* Null device? */
		case 0:
			break;
		/* 98644A */
		case 2:
		case 2+128:
			hw->hw_type = D_COMMDCA;
			break;
		/* 98622A */
		case 3:
			hw->hw_type = D_MISC;
			break;
		/* 98623A */
		case 4:
			hw->hw_type = D_MISC;
			break;
		/* 98642A */
		case 5:
		case 5+128:
			hw->hw_type = D_COMMDCM;
			break;
		/* 345/375 builtin parallel port */
		case 6:
			hw->hw_type = D_PPORT;
			break;
		/* 98625A */
		case 7:
		case 7+32:
		case 7+64:
		case 7+96:
			hw->hw_type = C_SCSI;
			break;
		/* 98625B */
		case 8:
			hw->hw_type = C_HPIB;
			break;
		/* 98287A */
		case 9:
			hw->hw_type = D_KEYBOARD;
			break;
		/* 98635A */
		case 10:
			hw->hw_type = D_FPA;
			break;
		/* timer */
		case 11:
			hw->hw_type = D_MISC;
			break;
		/* 98640A */
		case 18:
			hw->hw_type = D_MISC;
			break;
		/* 98643A */
		case 21:
			hw->hw_type = D_LAN;
			break;
		/* 98659A */
		case 22:
			hw->hw_type = D_MISC;
			break;
		/* 237 display */
		case 25:
			hw->hw_type = D_BITMAP;
			break;
		/* quad-wide card */
		case 26:
			hw->hw_type = D_MISC;
			hw->hw_size *= 4;
			sc += 3;
			break;
		/* 98253A */
		case 27:
			hw->hw_type = D_MISC;
			break;
		/* 98627A */
		case 28:
			hw->hw_type = D_BITMAP;
			break;
		/* 98633A */
		case 29:
			hw->hw_type = D_BITMAP;
			break;
		/* 98259A */
		case 30:
			hw->hw_type = D_MISC;
			break;
		/* 8741 */
		case 31:
			hw->hw_type = D_MISC;
			break;
		/* 98577A */
		case 49:
			hw->hw_type = C_VME;
			if (sc < 132) {
				hw->hw_size *= 2;
				sc++;
			}
			break;
		/* 98628A */
		case 52:
		case 52+128:
			hw->hw_type = D_COMMDCL;
			break;
		/* bitmap display */
		case 57:
			hw->hw_type = D_BITMAP;
			hw->hw_secid = id_reg[0x15];
			switch (hw->hw_secid) {
			/* 98700/98710 */
			case 1:
				break;
			/* 98544-547 topcat */
			case 2:
				break;
			/* 98720/721 renassiance */
			case 4:
				if (sc < 132) {
					hw->hw_size *= 2;
					sc++;
				}
				break;
			/* 98548-98556 catseye */
			case 5:
			case 6:
			case 7:
			case 9:
				break;
			/* 98730/731 davinci */
			case 8:
				if (sc < 132) {
					hw->hw_size *= 2;
					sc++;
				}
				break;
			/* A1096A hyperion */
			case 14:
				break;
			/* 987xx */
			default:
				break;
			}
			break;
		/* 98644A */
		case 66:
		case 66+128:
			hw->hw_type = D_COMMDCA;
			break;
		/* 98624A */
		case 128:
			hw->hw_type = C_HPIB;
			break;
		default:
			hw->hw_type = D_MISC;
			break;
		}
		/*
		 * Re-map to proper size
		 */
		if (didmap) {
			iounmap(addr, NBPG);
			addr = iomap(hw->hw_pa, hw->hw_size);
			if (addr == 0) {
				printf(notmappedmsg);
				continue;
			}
			hw->hw_kva = addr;
		}
		/*
		 * Encode bus type
		 */
		if (sc >= 132)
			hw->hw_type |= B_DIOII;
		else
			hw->hw_type |= B_DIO;
		hw++;
	}
}
#endif /* ! NEWCONFIG */
