/*	$NetBSD: mainbus.c,v 1.1 1996/06/19 03:21:05 scottr Exp $	*/

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
 * Copyright (C) 1996 Allen K. Briggs
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
 *	This product includes software developed by Allen Briggs.
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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/dmap.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>

#include <machine/autoconf.h>
#include <machine/vmparam.h>
#include <machine/param.h>
#include <machine/cpu.h>
#include <machine/pte.h>

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
	int off, pa, pmt = 0;
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

static int	mainbus_match __P((struct device *, void *, void *));
static void	mainbus_attach __P((struct device *parent,
					struct device *self, void *aux));

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
