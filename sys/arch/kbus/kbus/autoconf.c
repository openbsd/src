/*	$NetBSD: autoconf.c,v 1.16 1995/12/28 19:16:55 thorpej Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)autoconf.c	7.1 (Berkeley) 5/9/91
 */

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time and initializes the vba 
 * device tables and the memory controller monitoring.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */

#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>

#include <machine/cpu.h>
#include <machine/trap.h>
#include <machine/pmap.h>
#include <machine/autoconf.h>
#include <machine/vmparam.h>
#include <machine/kbus.h>

void	setroot __P((void));
void	swapconf __P((void));

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */

extern int cold;	/* cold start flag initialized in locore.s */

/*
 * Determine i/o configuration for a machine.
 */
void
configure()
{
	/* Start the clocks. */
/*	startrtclock(); */

	/* Find out what the hardware configuration looks like! */
	if (config_rootfound("mainbus", NULL) == NULL)
		panic ("No mainbus found!");

	(void)spl0();

	/*
	 * Configure swap area and related system
	 * parameter based on device(s) used.
	 */
#if 0 /* XXX */
	setroot();
#endif
	swapconf();
	cold = 0;

	printf ("******* COLD = 0 ********\n"); /* XXX */
}

/*
 * Configure swap space and related parameters.
 */
void
swapconf()
{
	register struct swdevt *swp;
	register int nblks;

	for (swp = swdevt; swp->sw_dev > 0; swp++)
	{
		unsigned d = major(swp->sw_dev);

		if (d >= nblkdev) break;
		if (bdevsw[d].d_psize) {
			nblks = (*bdevsw[d].d_psize)(swp->sw_dev);
			if (nblks > 0 &&
			    (swp->sw_nblks == 0 || swp->sw_nblks > nblks))
				swp->sw_nblks = nblks;
			else
				swp->sw_nblks = 0;
		}
		swp->sw_nblks = ctod(dtoc(swp->sw_nblks));

	}
}

#if 0
#define	DOSWAP			/* change swdevt and dumpdev */
u_long	bootdev = 0;		/* should be dev_t, but not until 32 bits */

static	char devname[][2] = {
	's','d',	/* 0 = sd */
	's','w',	/* 1 = sw */
	's','t',	/* 2 = st */
	'r','d',	/* 3 = rd */
	'c','d',	/* 4 = cd */
};

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
setroot()
{
	int  majdev, mindev, unit, part, adaptor;
	dev_t temp, orootdev;
	struct swdevt *swp;

	if (boothowto & RB_DFLTROOT ||
	    (bootdev & B_MAGICMASK) != (u_long)B_DEVMAGIC)
		return;
	majdev = (bootdev >> B_TYPESHIFT) & B_TYPEMASK;
	if (majdev > sizeof(devname) / sizeof(devname[0]))
		return;
	adaptor = (bootdev >> B_ADAPTORSHIFT) & B_ADAPTORMASK;
	part = (bootdev >> B_PARTITIONSHIFT) & B_PARTITIONMASK;
	unit = (bootdev >> B_UNITSHIFT) & B_UNITMASK;
	mindev = (unit * MAXPARTITIONS) + part;
	orootdev = rootdev;
	rootdev = makedev(majdev, mindev);
	/*
	 * If the original rootdev is the same as the one
	 * just calculated, don't need to adjust the swap configuration.
	 */
	if (rootdev == orootdev)
		return;
	printf("changing root device to %c%c%d%c\n",
		devname[majdev][0], devname[majdev][1],
		unit, part + 'a');

#ifdef DOSWAP
	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		if (majdev == major(swp->sw_dev) &&
		    (mindev / MAXPARTITIONS)
		    == (minor(swp->sw_dev) / MAXPARTITIONS)) {
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
#endif
}
#endif

#if 0
/*ARGSUSED*/
int
simple_devprint(auxp, pnp)
	void *auxp;
	const char *pnp;
{
	return(QUIET);
}
#endif

/* Defined in ioconf.c */
extern char *locnames[];
extern short locnamp[];

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
	int i;
	short *ind;

#ifdef	DIAGNOSTIC
	if (parent->dv_cfdata->cf_driver->cd_indirect)
		panic("bus_scan: indirect?");
	if (cf->cf_fstate == FSTATE_STAR)
		panic("bus_scan: FSTATE_STAR");
#endif

	/* ca->ca_bustype set by parent */
	ca->ca_intvec = -1;
	ca->ca_intpri = -1;
	ca->ca_paddr = -1;

	for (i = 0, ind = locnamp + cf->cf_locnames; ind[i] != -1; i++)
	  {
	    if (strcmp (locnames[ind[i]], "addr") == 0)
	      ca->ca_paddr  = cf->cf_loc[i];
	    else if (strcmp (locnames[ind[i]], "vect") == 0)
	      ca->ca_intvec = cf->cf_loc[i];
	    else if (strcmp (locnames[ind[i]], "level") == 0)
	      ca->ca_intpri  = cf->cf_loc[i];
	    else
	      printf ("Unknown loc `%s'\n", locnames[ind[i]]);
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


/*
 * Map an I/O device given physical address and size in bytes, e.g.,
 *
 *	mydev = (struct mydev *)mapdev(myioaddr,
 *				       sizeof(struct mydev), pmtype);
 *
 * See also machine/autoconf.h.
 */
static vm_offset_t iobase = VM_MIN_IO_ADDRESS + NBPG;
static vm_offset_t peek_addr = VM_MIN_IO_ADDRESS;

char *
bus_mapin(bustype, paddr, size)
	register int bustype;
	u_long paddr;
	register int size;
{
	register vm_offset_t v;
	register vm_offset_t pa;
	register char *ret;

	size = round_page(size);
	if (size == 0)
	  panic("bus_mapin: zero size");

	switch (bustype)
	  {
	  case BUS_KBUS:
	    break;
	  case BUS_VME16:
	    paddr = VME16_BASE | (paddr & VME16_MASK);
	    break;
	  case BUS_VME24:
	    paddr = VME24_BASE | (paddr & VME24_MASK);
	    break;
	  case BUS_VME32:
	    paddr = VME32_BASE | (paddr & VME32_MASK);
	    break;
	  default:
	    return NULL;
	  }

	v = iobase;
	iobase += size;
	if (iobase > VM_MAX_IO_ADDRESS)	/* unlikely */
	  panic("mapiodev");

	ret = (void *)(v | (paddr & PGOFSET)); /* note: preserve page offset */

	pa = trunc_page(paddr);

	do {
		pmap_enter(pmap_kernel(), v, pa | PG_IO,
			   VM_PROT_READ | VM_PROT_WRITE, 1,
			   VM_PROT_READ | VM_PROT_WRITE);
		v += PAGE_SIZE;
		pa += PAGE_SIZE;
	} while ((size -= PAGE_SIZE) > 0);
	return ret;
}

/*
 * Read addr with size len (1,2,4) into val.
 * If this generates a bus error, return -1
 *
 *	Create a temporary mapping,
 *	Try the access using peek_*
 *	Clean up temp. mapping
 */
int bus_peek(bustype, paddr, sz)
	int bustype, paddr, sz;
{
	int off;

	switch (bustype)
	  {
	  case BUS_KBUS:
	    break;
	  case BUS_VME16:
	    paddr = VME16_BASE | (paddr & VME16_MASK);
	    break;
	  case BUS_VME24:
	    paddr = VME24_BASE | (paddr & VME24_MASK);
	    break;
	  case BUS_VME32:
	    paddr = VME32_BASE | (paddr & VME32_MASK);
	    break;
	  default:
	    return -1;
	  }

	off = paddr & PGOFSET;

	pmap_enter (pmap_kernel(), peek_addr, (paddr & PG_FRAME) | PG_IO,
		    VM_PROT_READ | VM_PROT_WRITE, 1,
		    VM_PROT_READ | VM_PROT_WRITE);

	return probeget ((caddr_t)peek_addr + off, sz);
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
