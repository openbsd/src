/*	$NetBSD: autoconf.c,v 1.29 1996/05/15 02:51:00 briggs Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
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
 */
/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * from: Utah $Hdr: autoconf.c 1.31 91/01/21$
 *
 *	@(#)autoconf.c	7.5 (Berkeley) 5/7/91
 */

/*
   ALICE 
      05/23/92 BG
      I've started to re-write this procedure to use our devices and strip 
      out all the useless HP stuff, but I only got to line 120 or so 
      before I had a really bad attack of kompernelphobia and blacked out.
*/

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/conf.h>
#include <sys/dmap.h>
#include <sys/reboot.h>

#include <sys/disklabel.h>
#include <sys/disk.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>

#include <machine/adbsys.h>
#include <machine/autoconf.h>
#include <machine/vmparam.h>
#include <machine/param.h>
#include <machine/cpu.h>
#include <machine/pte.h>
#include <machine/viareg.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	cold;		    /* if 1 (locore.s), still working on cold-start */

#ifdef DEBUG
int	acdebug = 0;
#endif

static void	findbootdev __P((void));
static int	mainbus_match __P((struct device *, void *, void *));
static void	mainbus_attach __P((struct device *parent,
					struct device *self, void *aux));
static void	setroot __P((void));
static void	swapconf __P((void));

/*
 * Determine mass storage and memory configuration for a machine.
 */
void
configure(void)
{
	VIA_initialize();

	mrg_init();		/* Init Mac ROM Glue */

	startrtclock();		/* start before adb_init() */
	
	adb_init();		/* ADB device subsystem & driver */

	if (config_rootfound("mainbus", "mainbus") == NULL)
		panic("No main device!");

#if GENERIC
	if ((boothowto & RB_ASKNAME) == 0)
		setroot();
	setconf();
#else
	setroot();
#endif

	/*
	 * Configure swap area and related system
	 * parameter based on device(s) used.
	 */
	swapconf();
	dumpconf();
	cold = 0;
}

/*
 * Configure swap space and related parameters.
 */
static void
swapconf(void)
{
	register struct swdevt *swp;
	register int nblks;

	for (swp = swdevt; swp->sw_dev != NODEV ; swp++) {
		int maj = major(swp->sw_dev);

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

u_long	bootdev;		/* should be dev_t, but not until 32 bits */
struct	device *bootdv = NULL;

#define	PARTITIONMASK	0x7
#define	UNITSHIFT	3

/*
 * Map a SCSI bus, target, lun to a device number.
 * This could be tape, disk, CD.  The calling routine, though,
 * assumes DISK.  It would be nice to allow CD, too...
 */
static int
target_to_unit(u_long bus, u_long target, u_long lun)
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

/* swiped from sparc/sparc/autoconf.c */
static int
findblkmajor(register struct disk *dv)
{
	register int	i;

	for (i=0 ; i<nblkdev ; i++) {
		if ((void (*)(struct buf *))bdevsw[i].d_strategy ==
		    dv->dk_driver->d_strategy)
			return i;
	}
	return -1;
}

/*
 * Yanked from i386/i386/autoconf.c
 */
static void
findbootdev(void)
{
	register struct device *dv;
	register struct disk *diskp;
	register int unit;
	int major;

	major = B_TYPE(bootdev);
	if (major < 0 || major >= nblkdev)
		return;

	unit = B_UNIT(bootdev);

	bootdev &= ~(B_UNITMASK << B_UNITSHIFT);
	unit = target_to_unit(-1, unit, 0);
	bootdev |= (unit << B_UNITSHIFT);

	if (disk_count <= 0)
		return;

	for (dv = alldevs.tqh_first; dv != NULL; dv = dv->dv_list.tqe_next) {
		if ((dv->dv_class == DV_DISK) && (unit == dv->dv_unit)) {
			/*
			 * Find the disk corresponding to the current
			 * device.
			 */
			if ((diskp = disk_find(dv->dv_xname)) == NULL)
				continue;

			if (major == findblkmajor(diskp)) {
				bootdv = dv;
				return;
			}
		}
	}
}

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
static void
setroot(void)
{
	register struct swdevt	*swp;
	register int		majdev, mindev, part;
	dev_t			nrootdev, temp;

	if (boothowto & RB_DFLTROOT)
		return;
	findbootdev();
	if (bootdv == NULL) {
		printf("ARGH!!  No boot device????");
		delay(10000000);
/*		panic("ARGH!!  No boot device????"); */
	}
	nrootdev = 0;
	switch (bootdv->dv_class) {
		case DV_DISK:
			nrootdev = makedev(B_TYPE(bootdev),
					   (B_UNIT(bootdev) << UNITSHIFT)
					   + B_PARTITION(bootdev));
			break;
		default:
			printf("Only supports DISK device for booting.\n");
			break;
	}

	if (rootdev == nrootdev)
		return;

	majdev = major(nrootdev);
	mindev = minor(nrootdev);
	part = mindev & PARTITIONMASK;
	mindev -= part;

	rootdev = nrootdev;
	printf("Changing root device to %s%c.\n", bootdv->dv_xname, part+'a');

	temp = NODEV;
	for (swp = swdevt ; swp->sw_dev != NODEV ; swp++) {
		if (majdev == major(swp->sw_dev) &&
		    mindev == (minor(swp->sw_dev) & ~PARTITIONMASK)) {
			temp = swdevt[0].sw_dev;
			swdevt[0].sw_dev = swp->sw_dev;
			swp->sw_dev = temp;
			break;
		}
	}
	if (swp->sw_dev == NODEV)
		return;

	if (temp == dumpdev)
		dumpdev = swdevt[0].sw_dev;
}

/*
 * Generic "bus" support functions.  From NetBSD/sun3.
 *
 * bus_scan:
 * This function is passed to config_search() by the attach function
 * for each of the "bus" drivers (obio, nubus).
 * The purpose of this function is to copy the "locators" into our
 * confargs structure, so child drivers may use the confargs both
 * as match parameters and as temporary storage for the defaulted
 * locator values determined in the child_match and preserved for
 * the child_attach function.  If the bus attach functions just
 * used config_found, then we would not have an opportunity to
 * setup the confargs for each child match and attach call.
 *
 * bus_print:
 * Just prints out the final (non-default) locators.
 */
int
bus_scan(parent, child, aux)
	struct device *parent;
	void *child, *aux;
{
	struct cfdata *cf = child;
	struct confargs *ca = aux;
	cfmatch_t mf;

#ifdef	DIAGNOSTIC
	if (parent->dv_cfdata->cf_driver->cd_indirect)
		panic("bus_scan: indirect?");
	if (cf->cf_fstate == FSTATE_STAR)
		panic("bus_scan: FSTATE_STAR");
#endif

	/* ca->ca_bustype set by parent */

	/*
	 * Note that this allows the match function to save
	 * defaulted locators in the confargs that will be
	 * preserved for the related attach call.
	 */
	mf = cf->cf_attach->ca_match;
	if ((*mf)(parent, cf, ca) > 0) {
		config_attach(parent, cf, ca, bus_print);
	}
	return (0);
}

/*
 * From NetBSD/sun3.
 * Print out the confargs.  The parent name is non-NULL
 * when there was no match found by config_found().
 */
int
bus_print(args, name)
	void *args;
	char *name;
{
/*	struct confargs *ca = args; */

	if (name)
		printf("%s:", name);

	return(UNCONF);
}

vm_offset_t tmp_vpages[1];

/*
 * Read addr with size len (1,2,4) into val.
 * If this generates a bus error, return -1
 *
 *	Create a temporary mapping,
 *	Try the access using peek_*
 *	Clean up temp. mapping
 */
int
bus_peek(bustype, paddr, sz)
	int bustype;
	vm_offset_t paddr;
	int sz;
{
	int off, pte, rv;
	vm_offset_t pgva;
	caddr_t va;

	if (bustype != BUS_NUBUS)
		return -1;

	off = paddr & PGOFSET;
	paddr -= off;
	pte = (paddr & PG_FRAME) | (PG_V | PG_W | PG_CI);

	pgva = tmp_vpages[0];
	va = (caddr_t)pgva + off;

	mac68k_set_pte(pgva, pte);
	TBIS(pgva);

	/*
	 * OK, try the access using one of the assembly routines
	 * that will set pcb_onfault and catch any bus errors.
	 */
	rv = -1;
	switch (sz) {
	case 1:
		if (!badbaddr(va))
			rv = *((u_char *) va);
		break;
	case 2:
		if (!badwaddr(va))
			rv = *((u_int16_t *) va);
		break;
	case 4:
		if (!badladdr(va))
			rv = *((u_int32_t *) va);
		break;
	default:
		printf("bus_peek: invalid size=%d\n", sz);
		rv = -1;
	}

	mac68k_set_pte(pgva, PG_NV);
	TBIS(pgva);

	return rv;
}

char *
bus_mapin(bustype, paddr, sz)
	int bustype, paddr, sz;
{
	int off, pa, pmt;
	vm_offset_t va, retval;

	if (bustype != BUS_NUBUS)
		return (NULL);

	off = paddr & PGOFSET;
	pa = paddr - off;
	sz += off;
	sz = mac68k_round_page(sz);

	/* Get some kernel virtual address space. */
	va = kmem_alloc_wait(kernel_map, sz);
	if (va == 0)
		panic("bus_mapin");
	retval = va + off;

	/* Map it to the specified bus. */
#if 0	/* XXX */
	/* This has a problem with wrap-around... */
	pmap_map((int)va, pa | pmt, pa + sz, VM_PROT_ALL);
#else
	do {
		pmap_enter(pmap_kernel(), va, pa | pmt, VM_PROT_ALL, FALSE);
		va += NBPG;
		pa += NBPG;
	} while ((sz -= NBPG) > 0);
#endif

	return ((char*)retval);
}	

static int
mainbus_match(parent, match, aux)
	struct device	*parent;
	void		*match, *aux;
{
	return 1;
}

static int bus_order[] = {
	BUS_OBIO,	/* For On-board I/O */
	BUS_NUBUS
};
#define BUS_ORDER_SZ (sizeof(bus_order)/sizeof(bus_order[0]))

static void
mainbus_attach(parent, self, aux)
	struct device	*parent, *self;
	void		*aux;
{
	struct confargs	ca;
	int	i;

	printf("\n");

	for (i = 0; i < BUS_ORDER_SZ; i++) {
		ca.ca_bustype = bus_order[i];
		(void) config_found(self, &ca, NULL);
	}
}

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};
