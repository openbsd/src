/*	$OpenBSD: autoconf.c,v 1.17 2001/11/06 19:53:16 miod Exp $	*/
/*	$NetBSD: autoconf.c,v 1.37 1996/11/20 18:57:22 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
#include <sys/dkstat.h>
#include <sys/dmap.h>
#include <sys/map.h>
#include <sys/reboot.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/control.h>
#include <machine/disklabel.h>
#include <machine/cpu.h>
#include <machine/machdep.h>
#include <machine/mon.h>
#include <machine/pte.h>
#include <machine/pmap.h>

#include <dev/cons.h>

/* Want compile-time initialization here. */
int cold = 1;

void setroot __P((void));
void swapconf __P((void));
int findblkmajor __P((struct device *));
struct device *getdisk __P((char *, int, int, dev_t *));
struct device *parsedisk __P((char *, int, int, dev_t *));
int getstr __P((char *, int));

void
cpu_configure()
{
	struct device *mainbus;

	/* General device autoconfiguration. */
	mainbus = config_rootfound("mainbus", NULL);
	if (mainbus == NULL)
		panic("cpu_configure: mainbus not found");

	/*
	 * Now that device autoconfiguration is finished,
	 * we can safely enable interrupts.
	 */
	printf("enabling interrupts\n");
	(void)spl0();

	/*
	 * Configure swap area and related system
	 * parameters based on device(s) used.
	 */
	setroot();
	swapconf();
	
	cold = 0;
}

/*
 * Generic "bus" support functions.
 *
 * bus_scan:
 * This function is passed to config_search() by the attach function
 * for each of the "bus" drivers (obctl, obio, obmem, vmes, vmel).
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
	ca->ca_paddr  = cf->cf_loc[0];
	ca->ca_intpri = cf->cf_loc[1];
	ca->ca_intvec = -1;

	if ((ca->ca_bustype == BUS_VME16) ||
	    (ca->ca_bustype == BUS_VME32)) {
		ca->ca_intvec = cf->cf_loc[2];
	}

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
 * Print out the confargs.  The parent name is non-NULL
 * when there was no match found by config_found().
 */
int
bus_print(args, name)
	void *args;
	const char *name;
{
	struct confargs *ca = args;

	if (name)
		printf("%s:", name);

	if (ca->ca_paddr != -1)
		printf(" addr 0x%x", ca->ca_paddr);
	if (ca->ca_intpri != -1)
		printf(" level %d", ca->ca_intpri);
	if (ca->ca_intvec != -1)
		printf(" vector 0x%x", ca->ca_intvec);

	return(UNCONF);
}

extern vm_offset_t tmp_vpages[];
static const int bustype_to_ptetype[4] = {
	PGT_OBMEM,
	PGT_OBIO,
	PGT_VME_D16,
	PGT_VME_D32,
};

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
	int bustype, paddr, sz;
{
	int off, pte, rv;
	vm_offset_t pgva;
	caddr_t va;

	if (bustype & ~3)
		return -1;

	off = paddr & PGOFSET;
	paddr -= off;
	pte = PA_PGNUM(paddr);
	pte |= bustype_to_ptetype[bustype];
	pte |= (PG_VALID | PG_WRITE | PG_SYSTEM | PG_NC);

	pgva = tmp_vpages[0];
	va = (caddr_t)pgva + off;

	/* All mappings in tmp_vpages are non-cached, so no flush. */
	set_pte(pgva, pte);

	/*
	 * OK, try the access using one of the assembly routines
	 * that will set pcb_onfault and catch any bus errors.
	 */
	switch (sz) {
	case 1:
		rv = peek_byte(va);
		break;
	case 2:
		rv = peek_word(va);
		break;
	default:
		printf(" bus_peek: invalid size=%d\n", sz);
		rv = -1;
	}

	/* All mappings in tmp_vpages are non-cached, so no flush. */
	set_pte(pgva, PG_INVAL);

	return rv;
}

static const int bustype_to_pmaptype[4] = {
	0,
	PMAP_OBIO,
	PMAP_VME16,
	PMAP_VME32,
};

char *
bus_mapin(bustype, paddr, sz)
	int bustype, paddr, sz;
{
	int off, pa, pmt;
	vm_offset_t va, retval;

	if (bustype & ~3)
		return (NULL);

	off = paddr & PGOFSET;
	pa = paddr - off;
	sz += off;
	sz = round_page(sz);

	pmt = bustype_to_pmaptype[bustype];
	pmt |= PMAP_NC;	/* non-cached */

	/* Get some kernel virtual address space. */
	va = uvm_km_valloc_wait(kernel_map, sz);
	if (va == 0)
		panic("bus_mapin");
	retval = va + off;

	/* Map it to the specified bus. */
#if 0	/* XXX */
	/* This has a problem with wrap-around... */
	pmap_map((int)va, pa | pmt, pa + sz, VM_PROT_ALL);
#else
	do {
		pmap_enter(pmap_kernel(), va, pa | pmt, VM_PROT_ALL, 0);
		va += NBPG;
		pa += NBPG;
	} while ((sz -= NBPG) > 0);
#endif

	return ((char*)retval);
}

/* from hp300: badaddr() */
int
peek_word(addr)
	register caddr_t addr;
{
	label_t		faultbuf;
	register int	x;

	nofault = &faultbuf;
	if (setjmp(&faultbuf)) {
		nofault = NULL;
		return(-1);
	}
	x = *(volatile u_short *)addr;
	nofault = NULL;
	return(x);
}

/* from hp300: badbaddr() */
int
peek_byte(addr)
	register caddr_t addr;
{
	label_t		faultbuf;
	register int	x;

	nofault = &faultbuf;
	if (setjmp(&faultbuf)) {
		nofault = NULL;
		return(-1);
	}
	x = *(volatile u_char *)addr;
	nofault = NULL;
	return(x);
}

/****************************************************************/

/*
 * Configure swap space and related parameters.
 */
void
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
			swp->sw_nblks = ctod(dtoc(swp->sw_nblks));
		}
	dumpconf();
}

struct nam2blk {
	char *name;
	int maj;
} nam2blk[] = {
	{ "xy",		3 },
	{ "sd",		7 },
	{ "xd",		10 },
	{ "st",		11 },
	{ "rd",		17 },
	{ "cd",		18 },
};

/* This takes the args: name, ctlr, unit */
typedef struct device *(*findfunc_t) __P((char *, int, int));

struct device *find_dev_byname __P((char *));
struct device *net_find __P((char *, int, int));
struct device *scsi_find __P((char *, int, int));
struct device *xx_find __P((char *, int, int));

struct prom_n2f {
	char name[4];
	findfunc_t func;
} prom_dev_table[] = {
	{ "ie",	net_find },
	{ "le",	net_find },
	{ "sd",	scsi_find },
	{ "xy",	xx_find },
	{ "xd",	xx_find },
};

int
findblkmajor(dv)
	struct device *dv;
{
	char *name = dv->dv_xname;
	register int i;

	for (i = 0; i < sizeof(nam2blk)/sizeof(nam2blk[0]); ++i)
		if (strncmp(name, nam2blk[i].name, strlen(nam2blk[0].name)) == 0)
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
		for (dv = alldevs.tqh_first; dv != NULL;
		    dv = dv->dv_list.tqe_next) {
			if (dv->dv_class == DV_DISK)
				printf(" %s[a-%c]", dv->dv_xname,
				    'a' + MAXPARTITIONS - 1);
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
	int majdev, unit, part;

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
			unit = dv->dv_unit;
			if (majdev < 0)
				panic("parsedisk");
			*devp = MAKEDISKDEV(majdev, unit, part);
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
	MachMonBootParam *bp;
	struct prom_n2f *nf;
	char promname[3];
	findfunc_t find;
	struct swdevt *swp;
	struct device *bootdv;
	int len, majdev, unit, part;
	dev_t nrootdev, nswapdev = NODEV;
	char buf[128];
	dev_t temp;
#if defined(NFSCLIENT)
	extern char *nfsbootdevname;
#endif

	/* PROM boot parameters. */
	bp = *romp->bootParam;

	/*
	 * Copy PROM boot device name (two letters)
	 * to a normal, null terminated string.
	 * (No terminating null in bp->devName)
	 */
	promname[0] = bp->devName[0];
	promname[1] = bp->devName[1];
	promname[2] = '\0';

	/* Default to "unknown" */
	bootdv = NULL;
	part = 0;
	find = NULL;

	/* Do we know anything about the PROM boot device? */
	for (nf = prom_dev_table; nf->func; nf++)
		if (strcmp(nf->name, promname) == 0) {
			find = nf->func;
			break;
		}
	if (find != NULL)
		bootdv = (*find)(promname, bp->ctlrNum, bp->unitNum);
	if (bootdv != NULL) {
		if (bootdv->dv_class == DV_DISK)
			part = bp->partNum;
	}

	/*
	 * If `swap generic' and we couldn't determine boot device,
	 * ask the user.
	 */
	if (mountroot == NULL && bootdv == NULL)
		boothowto |= RB_ASKNAME;

	if (boothowto & RB_ASKNAME) {
		for (;;) {
			printf("root device ");
			if (bootdv != NULL)
				printf("(default %s%c)",
					bootdv->dv_xname,
					bootdv->dv_class == DV_DISK
						? part + 'a' : ' ');
			printf(": ");
			len = getstr(buf, sizeof(buf));
			if (len == 0 && bootdv != NULL) {
				strcpy(buf, bootdv->dv_xname);
				len = strlen(buf);
			}
			if (len > 0 && buf[len - 1] == '*') {
				buf[--len] = '\0';
				bootdv = getdisk(buf, len, 1, &nrootdev);
				if (bootdv != NULL) {
					nswapdev = nrootdev;
					goto gotswap;
				}
			}
			bootdv = getdisk(buf, len, part, &nrootdev);
			if (bootdv != NULL) {
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
					bootdv->dv_class == DV_DISK ? 'b' : ' ');
			printf(": ");
			len = getstr(buf, sizeof(buf));
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
			bootdv = getdisk(buf, len, 1, &nswapdev);
			if (bootdv != NULL) {
				if (bootdv->dv_class == DV_IFNET)
					nswapdev = NODEV;
				break;
			}
		}
gotswap:
		rootdev = nrootdev;
		dumpdev = nswapdev;
		swdevt[0].sw_dev = nswapdev;
		/* swdevt[1].sw_dev = NODEV; */

	} else if (mountroot == NULL) {

		/*
		 * `swap generic': Use the device the PROM told us to use.
		 */
		majdev = findblkmajor(bootdv);
		if (majdev >= 0) {
			/*
			 * Root and swap are on a disk.
			 * part is the partition number.
			 * Assume swap is on partition b.
			 */
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
		/* swdevt[1].sw_dev = NODEV; */

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
		unit = DISKUNIT(rootdev);
		part = DISKPART(rootdev);
		printf("root on %s%c\n", bootdv->dv_xname,
		    part + 'a');
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
	if (swp->sw_dev != NODEV) {
		/*
		 * If dumpdev was the same as the old primary swap device,
		 * move it to the new primary swap device.
		 */
		if (temp == dumpdev)
			dumpdev = swdevt[0].sw_dev;
	}
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

/*
 * Functions to find devices using PROM boot parameters.
 */

/*
 * Network device:  Just use controller number.
 */
struct device *
net_find(name, ctlr, unit)
	char *name;
	int ctlr, unit;
{
	char tname[16];

	sprintf(tname, "%s%d", name, ctlr);
	return (find_dev_byname(tname));
}

/*
 * SCSI device:  The controller number corresponds to the
 * scsibus number, and the unit number is (targ*8 + LUN).
 */
struct device *
scsi_find(name, ctlr, unit)
	char *name;
	int ctlr, unit;
{
	struct device *scsibus;
	struct scsibus_softc *sbsc;
	struct scsi_link *sc_link;
	int target, lun;
	char tname[16];

	sprintf(tname, "scsibus%d", ctlr);
	scsibus = find_dev_byname(tname);
	if (scsibus == NULL)
		return (NULL);

	/* Compute SCSI target/LUN from PROM unit. */
	target = (unit >> 3) & 7;
	lun = unit & 7;

	/* Find the device at this target/LUN */
	sbsc = (struct scsibus_softc *)scsibus;
	sc_link = sbsc->sc_link[target][lun];
	if (sc_link == NULL)
		return (NULL);

	return (sc_link->device_softc);
}

/*
 * Xylogics SMD disk: (xy, xd)
 * Assume wired-in unit numbers for now...
 */
struct device *
xx_find(name, ctlr, unit)
	char *name;
	int ctlr, unit;
{
	int diskunit;
	char tname[16];

	diskunit = (ctlr * 2) + unit;
	sprintf(tname, "%s%d", name, diskunit);
	return (find_dev_byname(tname));
}

/*
 * Given a device name, find its struct device
 * XXX - Move this to some common file?
 */
struct device *
find_dev_byname(name)
	char *name;
{
	struct device *dv;

	for (dv = alldevs.tqh_first; dv != NULL;
	    dv = dv->dv_list.tqe_next) {
		if (strcmp(dv->dv_xname, name) == 0) {
			return(dv);
		}
	}
	return (NULL);
}
