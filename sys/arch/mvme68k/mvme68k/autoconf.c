/*	$Id: autoconf.c,v 1.4 1995/12/30 09:24:29 deraadt Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 *	This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 * 	The Regents of the University of California. All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	@(#)autoconf.c  8.2 (Berkeley) 1/12/94
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
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/conf.h>
#include <sys/dmap.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/disklabel.h>

#include <machine/vmparam.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/pte.h>

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
extern int cold;		/* if 1, still working on cold-start */

/* XXX must be allocated statically because of early console init */
struct	map extiomap[EIOMAPSIZE/16];
extern	char *extiobase;

void mainbus_attach __P((struct device *, struct device *, void *));
int  mainbus_match __P((struct device *, void *, void *));

struct cfdriver mainbuscd = {
	NULL, "mainbus", mainbus_match, mainbus_attach,
	DV_DULL, sizeof(struct device), 0
};

int
mainbus_match(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	return (1);
}

int
mainbus_print(args, bus)
	void *args;
	char *bus;
{
	struct confargs *ca = args;

	if (ca->ca_paddr != (void *)-1)
		printf(" addr 0x%x", ca->ca_paddr);
	return (UNCONF);
}

int
mainbus_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	struct cfdata *cf = child;
	struct confargs oca;

	bzero(&oca, sizeof oca);
	oca.ca_paddr = (caddr_t)cf->cf_loc[0];
	oca.ca_vaddr = (caddr_t)-1;
	oca.ca_ipl = -1;
	oca.ca_bustype = BUS_MAIN;
	oca.ca_name = cf->cf_driver->cd_name;
	if ((*cf->cf_driver->cd_match)(parent, cf, &oca) == 0)
		return (0);
	config_attach(parent, cf, &oca, mainbus_print);
	return (1);
}

void
mainbus_attach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	printf("\n");

	/* XXX
	 * should have a please-attach-first list for mainbus,
	 * to ensure that the pcc/vme2/mcc chips are attached
	 * first.
	 */

	(void)config_search(mainbus_scan, self, args);
}

/*
 * Determine mass storage and memory configuration for a machine.
 */
configure()
{
	init_sir();

	rminit(extiomap, (long)EIOMAPSIZE, (long)1, "extio", EIOMAPSIZE/16);

	if (!config_rootfound("mainbus", NULL))
		panic("autoconfig failed, no root");

	setroot();
	swapconf();
	cold = 0;
}

/*
 * Allocate/deallocate a cache-inhibited range of kernel virtual address
 * space mapping the indicated physical address range [pa - pa+size)
 */
caddr_t
mapiodev(pa, size)
	caddr_t pa;
	int size;
{
	int ix, npf, offset;
	caddr_t kva;

	size = roundup(size, NBPG);
	offset = (int)pa & PGOFSET;
	pa = (caddr_t)((int)pa & ~PGOFSET);

#ifdef DEBUG
	if (((int)pa & PGOFSET) || (size & PGOFSET))
	        panic("mapiodev: unaligned");
#endif
	npf = btoc(size);
	ix = rmalloc(extiomap, npf);
	if (ix == 0)
	        return (0);
	kva = extiobase + ctob(ix-1);
	physaccess(kva, pa, size, PG_RW|PG_CI);
	return (kva + offset);
}

void
unmapiodev(kva, size)
	caddr_t kva;
	int size;
{
	int ix;

#ifdef DEBUG
	if (((int)kva & PGOFSET) || (size & PGOFSET))
	        panic("unmapiodev: unaligned");
	if (kva < extiobase || kva >= extiobase + ctob(EIOMAPSIZE))
	        panic("unmapiodev: bad address");
#endif
	physunaccess(kva, size);
	ix = btoc(kva - extiobase) + 1;
	rmfree(extiomap, btoc(size), ix);
}

/*
 * Configure swap space and related parameters.
 */
swapconf()
{
	register struct swdevt *swp;
	register int nblks;

	for (swp = swdevt; swp->sw_dev != NODEV; swp++)
		if (bdevsw[major(swp->sw_dev)].d_psize) {
			nblks =
			  (*bdevsw[major(swp->sw_dev)].d_psize)(swp->sw_dev);
			if (nblks != -1 &&
			    (swp->sw_nblks == 0 || swp->sw_nblks > nblks))
				swp->sw_nblks = nblks;
		}
	dumpconf();
}

u_long	bootdev;

#define	PARTITIONMASK	(MAXPARTITIONS-1)

struct bdevnam {
	char *name;
	int maj;
} bdevnam[] = {
	{ "sd", 4 },
	{ "cd", 6 },
	{ "xd", 10 },
};

char *
blktonam(blk)
	int blk;
{
	int i;

	for (i = 0; i < sizeof(bdevnam)/sizeof(bdevnam[0]); i++)
		if (bdevnam[i].maj == blk)
			return (bdevnam[i].name);
	return ("??");
}

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
setroot()
{
	int  majdev, mindev, nswapdev;
	extern int (*mountroot)();
#if defined(NFSCLIENT)
	extern char *nfsbootdevname;
	extern int nfs_mountroot();
#endif
#if defined(FFS)
	extern int ffs_mountroot();
#endif
	int boottype = DV_DISK;
	int tmp;

#ifdef DEBUG
	printf("bootdev 0x%08x boothowto 0x%08x\n", bootdev, boothowto);
#endif

	/*
	 * ignore DFLTROOT in the `swap generic' case.
	 */
	if (boothowto & RB_DFLTROOT ||
	    (bootdev & B_MAGICMASK) != (u_long)B_DEVMAGIC)
		if (mountroot)
			return;

	if (boothowto & RB_ASKNAME) {
#if 0
		char *devname;

		majdev = B_TYPE(bootdev);
		mindev = B_UNIT(bootdev);
		name = searchname(majdev);
		while (mindev == 0 
			if (bootdv && bootdv->dv_class == DV_DISK)
				printf("root device (default %sa)? ",
				    name);
			else if (bootdv)
				printf("root device (default %s)? ",
				    name);
			else
				printf("root device ? ");
			len = getstr(buf, sizeof(buf));
			if (len == 0) {
				if (!bootdv)
					continue;
				strcpy(buf, bootdv->dv_xname);
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
		/*XXXX remember to set boottype if we are a network device!! */

		/*
		 * because swap must be on same device as root, for
		 * network devices this is easy.
		 * XXX: IS THIS STILL TRUE?
		 */
		if (bootdv->dv_class == DV_IFNET) {
			goto gotswap;
		}
		for (;;) {
			if (bootdv && bootdv->dv_class == DV_DISK)
				printf("swap device (default %sb)? ",
					bootdv->dv_xname);
			else if (bootdv)
				printf("swap device (default %s)? ",
					bootdv->dv_xname);
			else
				printf("swap device ? ");
			len = getstr(buf, sizeof(buf));
			if (len == 0) {
				if (!bootdv)
					continue;
				switch (bootdv->dv_class) {
				case DV_IFNET:
					nswapdev = NODEV;
					break;
				case DV_DISK:
					nswapdev = makedev(major(nrootdev),
					    (minor(nrootdev) & ~ PARTITIONMASK) | 1);
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
		/* swdevt[1].sw_dev = NODEV; */
		/* XXX should ask for devices to boot from */
#else
		panic("RB_ASKNAME not implimented");
#endif
	} else if (mountroot == NULL) {
		/*
		 * `swap generic': Use the device the boot program
		 * told us to use. This is a bit messy, since the ROM
		 * doesn't give us a standard dev_t.
		 *    B_TYPE: 0 = disk, 1 = net
		 *    B_ADAPTER: major # of disk device driver XXX
		 *    B_UNIT: disk unit number
		 */
		if (B_TYPE(bootdev) == 0) {
			/*
			 * Root and swap are on a disk.
			 * Assume that we are supposed to put root on
			 * partition a, and swap on partition b.
			 */
			switch (B_ADAPTOR(bootdev)) {
			case 0:
				majdev = 4;
				break;
			}
			mindev = B_UNIT(bootdev) << PARTITIONSHIFT;
			rootdev = makedev(majdev, mindev);
			nswapdev = dumpdev = makedev(major(rootdev),
			    (minor(rootdev) & ~ PARTITIONMASK) | 1);
		} else {
			/*
			 * Root and swap are on a net.
			 * XXX we don't know which network device...
			 */
			nswapdev = dumpdev = NODEV;
			boottype = DV_IFNET;
		}
		swdevt[0].sw_dev = nswapdev;
		/* swdevt[1].sw_dev = NODEV; */
	} else {
		/*
		 * `root DEV swap DEV': honour rootdev/swdevt.
		 * rootdev/swdevt/mountroot already properly set.
		 */
		return;
	}

	switch (boottype) {
#if defined(NFSCLIENT)
	case DV_IFNET:
		mountroot = nfs_mountroot;
		nfsbootdevname = NULL;		/* XXX don't know it */
		break;
#endif
#if defined(FFS)
	case DV_DISK:
		mountroot = ffs_mountroot;
		majdev = major(rootdev);
		mindev = minor(rootdev);
		printf("root on %s%d%c\n", blktonam(majdev),
		    mindev >> PARTITIONSHIFT,
		    (mindev & PARTITIONMASK) + 'a');
		break;
#endif
	default:
		printf("can't figure root, hope your kernel is right\n");
		break;
	}
}
