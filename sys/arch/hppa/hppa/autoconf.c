/*	$OpenBSD: autoconf.c,v 1.9 2000/03/30 16:33:27 mickey Exp $	*/

/*
 * Copyright (c) 1998-2000 Michael Shalayeff
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
 *	@(#)autoconf.c	8.4 (Berkeley) 10/1/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/timeout.h>

#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <dev/cons.h>

#include <hppa/dev/cpudevs.h>
#include <hppa/dev/cpudevs_data.h>

void	setroot __P((void));
void	swapconf __P((void));
void	dumpconf __P((void));

static int findblkmajor __P((struct device *dv));

void (*cold_hook) __P((void)); /* see below */
register_t	kpsw = PSW_Q | PSW_P | PSW_C | PSW_D;

/*
 * LED blinking thing
 */
#ifdef USELEDS
struct timeout heartbeat_tmo;
void heartbeat __P((void *));
extern int hz;
#endif

/*
 * configure:
 * called at boot time, configure all devices on system
 */
void
configure()
{
	extern int cold;

	splhigh();
	if (config_rootfound("mainbus", "mainbus") == NULL)
		panic("no mainbus found");

	/* in spl*() we trust */
	__asm __volatile("ssm %0, %%r0" :: "i" (PSW_I));
	kpsw |= PSW_I;
	spl0();

	setroot();
	swapconf();
	dumpconf();
	cold = 0;
	if (cold_hook)
		(*cold_hook)();

#ifdef USELEDS
	timeout_set(&heartbeat_tmo, heartbeat, NULL);
	heatbeat(NULL);
#endif
}

#ifdef USELEDS
/*
 * turn the heartbeat alive.
 * right thing would be to pass counter to each subsequent timeout
 * as an argument to heartbeat() incrementing every turn,
 * i.e. avoiding the static hbcnt, but doing timeout_set() on each
 * timeout_add() sounds ugly, guts of struct timeout looks ugly
 * to ponder in even more.
 */
void
heartbeat(v)
	void *v;
{
	static u_int hbcnt = 0;

	/*
	 * do this:
	 *
	 *   |~| |~|
	 *  _| |_| |_,_,_,_
	 *   0 1 2 3 4 6 7
	 */
	if (hbcnt < 4) {
		ledctl(0, 0, PALED_HEARTBEAT);
		hbcnt++;
		timeout_add(&heartbeat_tmo, hz / 8);
	} else {
		hbcnt = 0;
		timeout_add(&heartbeat_tmo, hz / 2);
	}
}
#endif

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
}

/*
 * This is called by configure to set dumplo and dumpsize.
 * Dumps always skip the first CLBYTES of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
dumpconf()
{
	extern int dumpsize;
	int nblks, dumpblks;	/* size of dump area */
	int maj;

	if (dumpdev == NODEV)
		goto bad;
	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		goto bad;
	nblks = (*bdevsw[maj].d_psize)(dumpdev);
	if (nblks <= ctod(1))
		goto bad;
	dumpblks = cpu_dumpsize();
	if (dumpblks < 0)
		goto bad;
	dumpblks += ctod(physmem);

	/* If dump won't fit (incl. room for possible label), punt. */
	if (dumpblks > (nblks - ctod(1)))
		goto bad;

	/* Put dump at end of partition */
	dumplo = nblks - dumpblks;

	/* dumpsize is in page units, and doesn't include headers. */
	dumpsize = physmem;
	return;

bad:
	dumpsize = 0;
	return;
}

struct nam2blk {
	char *name;
	int maj;
} nam2blk[] = {
	{ "st",		2 },
	{ "cd",		3 },
	{ "rd",		6 },
	{ "sd",		8 },
#if 0
	{ "acd",	4 },
	{ "wd",		0 },
	{ "fd",		XXX },
#endif
};

#ifdef RAMDISK_HOOKS
/*static struct device fakerdrootdev = { DV_DISK, {}, NULL, 0, "rd0", NULL };*/
#endif

static int
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
	dev_t temp, nswapdev;
	struct device *bootdv;
	int majdev, unit, part;
#ifdef NFSCLIENT
	extern char *nfsbootdevname;
#endif

	/*
	 * If 'swap generic' and we couldn't determine root device,
	 * ask the user.
	 */
	if (mountroot == NULL && bootdv == NULL)
		boothowto |= RB_ASKNAME;

	if (boothowto & RB_ASKNAME) {
		for (;;) {
			printf("root device? ");

		}
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
			/* part = bp->val[2]; */
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
		swdevt[1].sw_dev = NODEV;

	} else {

		/*
		 * `root DEV swap DEV': honour rootdev/swdevt.
		 * rootdev/swdevt/mountroot already properly set.
		 */
		majdev = major(rootdev);
		unit = DISKUNIT(rootdev);
		part = DISKPART(rootdev);
		return;
	}

	switch (bootdv->dv_class) {
#ifdef NFSCLIENT
	case DV_IFNET:
		mountroot = nfs_mountroot;
		nfsbootdevname = bootdv->dv_xname;
		return;
#endif
#ifndef DISKLESS
	case DV_DISK:
		mountroot = dk_mountroot;
		majdev = major(rootdev);
		unit = DISKUNIT(rootdev);
		part = DISKPART(rootdev);
		printf("root on %s%c\n", bootdv->dv_xname,
		    part + 'a');
		break;
#endif
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
pdc_scanbus(self, ca, bus, maxmod)
	struct device *self;
	struct confargs *ca;
	int bus, maxmod;
{
	struct pdc_memmap pdc_memmap;
	struct device_path dp;
	register int i;

	for (i = maxmod; i--; ) {
		struct confargs nca;
		struct pdc_iodc_read pdc_iodc_read;

		dp.dp_bc[0] = dp.dp_bc[1] = dp.dp_bc[2] = dp.dp_bc[3] = -1;
		dp.dp_bc[4] = bus;
		dp.dp_bc[5] = bus < 0? -1 : 0;
		dp.dp_mod = i;

		if (pdc_call((iodcio_t)pdc, 0, PDC_MEMMAP,
			     PDC_MEMMAP_HPA, &pdc_memmap, &dp) < 0)
			continue;

		nca = *ca;
		if (pdc_call((iodcio_t)pdc, 0, PDC_IODC, PDC_IODC_READ,
			     &pdc_iodc_read, pdc_memmap.hpa, IODC_DATA,
			     &nca.ca_type, sizeof(nca.ca_type)) < 0)
			continue;

		nca.ca_mod = i;
		nca.ca_hpa = pdc_memmap.hpa;
		nca.ca_pdc_iodc_read = &pdc_iodc_read;
		nca.ca_name = hppa_mod_info(nca.ca_type.iodc_type,
					    nca.ca_type.iodc_sv_model);

		config_found_sm(self, &nca, mbprint, mbsubmatch);
	}
}

const char *
hppa_mod_info(type, sv)
	int type, sv;
{
	register const struct hppa_mod_info *mi;
	static char fakeid[32];

	for (mi = hppa_knownmods; mi->mi_type >= 0 &&
	     (mi->mi_type != type || mi->mi_sv != sv); mi++);

	if (mi->mi_type < 0) {
		sprintf(fakeid, "type %x, sv %x", type, sv);
		return fakeid;
	} else
		return mi->mi_name;
}

