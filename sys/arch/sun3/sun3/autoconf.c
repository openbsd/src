/*	$NetBSD: autoconf.c,v 1.38 1996/12/17 21:11:14 gwr Exp $	*/

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
 * Configure() is called at boot time.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/map.h>
#include <sys/dkstat.h>
#include <sys/dmap.h>
#include <sys/reboot.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/control.h>
#include <machine/pte.h>
#include <machine/pmap.h>

#include "machdep.h"

int cold;

void configure()
{
	struct device *mainbus;

	/* General device autoconfiguration. */
	mainbus = config_rootfound("mainbus", NULL);
	if (mainbus == NULL)
		panic("configure: mainbus not found");

	/* Choose root and swap devices. */
	swapgeneric();
	swapconf();
	dumpconf();
	cold = 0;
}

/*
 * Configure swap space and related parameters.
 */
void
swapconf()
{
	struct swdevt *swp;
	u_int maj;
	int nblks;

	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {

		maj = major(swp->sw_dev);
		if (maj > nblkdev) /* paranoid? */
			break;

		if (bdevsw[maj].d_psize) {
			nblks = (*bdevsw[maj].d_psize)(swp->sw_dev);
			if (nblks > 0 &&
				(swp->sw_nblks == 0 || swp->sw_nblks > nblks))
				swp->sw_nblks = nblks;
			swp->sw_nblks = ctod(dtoc(swp->sw_nblks));
		}
	}
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
int bus_scan(parent, child, aux)
	struct device *parent;
	void *child, *aux;
{
	struct cfdata *cf = child;
	struct confargs *ca = aux;
	cfmatch_t mf;

#ifdef	DIAGNOSTIC
	if (cf->cf_fstate == FSTATE_STAR)
		panic("bus_scan: FSTATE_STAR");
#endif

	/* ca->ca_bustype set by parent */
	ca->ca_paddr  = cf->cf_loc[0];
	ca->ca_intpri = cf->cf_loc[1];
	ca->ca_intvec = -1;

	if ((ca->ca_bustype == BUS_VME16) ||
		(ca->ca_bustype == BUS_VME32))
	{
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
int bus_peek(bustype, paddr, sz)
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
	sz = sun3_round_page(sz);

	pmt = bustype_to_pmaptype[bustype];
	pmt |= PMAP_NC;	/* non-cached */

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

int
peek_word(addr)
	register caddr_t addr;
{
	label_t		faultbuf;
	register int x;

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
	label_t 	faultbuf;
	register int x;

	nofault = &faultbuf;
	if (setjmp(&faultbuf)) {
		nofault = NULL;
		return(-1);
	}
	x = *(volatile u_char *)addr;
	nofault = NULL;
	return(x);
}
