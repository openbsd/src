/*	$NetBSD: obio.c,v 1.16 1995/08/18 08:20:26 pk Exp $	*/

/*
 * Copyright (c) 1993, 1994 Theo de Raadt
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
 *	This product includes software developed by Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>

#ifdef DEBUG
#include <sys/proc.h>
#include <sys/syslog.h>
#endif

#include <vm/vm.h>

#include <machine/autoconf.h>
#include <machine/pmap.h>
#include <machine/oldmon.h>
#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <sparc/sparc/asm.h>
#include <sparc/sparc/vaddrs.h>

struct bus_softc {
	struct	device sc_dev;		/* base device */
	int	nothing;
};

/* autoconfiguration driver */
static int	busmatch __P((struct device *, void *, void *));
static void	obioattach __P((struct device *, struct device *, void *));
static void	vmesattach __P((struct device *, struct device *, void *));
static void	vmelattach __P((struct device *, struct device *, void *));

struct cfdriver obiocd = { NULL, "obio", busmatch, obioattach,
	DV_DULL, sizeof(struct bus_softc)
};
struct cfdriver vmelcd = { NULL, "vmel", busmatch, vmelattach,
	DV_DULL, sizeof(struct bus_softc)
};
struct cfdriver vmescd = { NULL, "vmes", busmatch, vmesattach,
	DV_DULL, sizeof(struct bus_softc)
};

static int	busattach __P((struct device *, void *, void *, int));

void *		bus_map __P((void *, int, int));
void *		bus_tmp __P((void *, int));
void		bus_untmp __P((void));

int
busmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (cputyp != CPU_SUN4)
		return (0);
	return (strcmp(cf->cf_driver->cd_name, ra->ra_name) == 0);
}

int
busprint(args, obio)
	void *args;
	char *obio;
{
	register struct confargs *ca = args;

	if (ca->ca_ra.ra_name == NULL)
		ca->ca_ra.ra_name = "<unknown>";
	if (obio)
		printf("[%s at %s]", ca->ca_ra.ra_name, obio);
	printf(" addr %x", ca->ca_ra.ra_paddr);
	if (ca->ca_ra.ra_intr[0].int_vec != -1)
		printf(" vec 0x%x", ca->ca_ra.ra_intr[0].int_vec);
	return (UNCONF);
}


int
busattach(parent, child, args, bustype)
	struct device *parent;
	void *args, *child;
	int bustype;
{
	struct cfdata *cf = child;
	register struct bus_softc *sc = (struct bus_softc *)parent;
	register struct confargs *ca = args;
	struct confargs oca;
	caddr_t tmp;

	if (bustype == BUS_OBIO && cputyp == CPU_SUN4) {
		/*
		 * On the 4/100 obio addresses must be mapped at
		 * 0x0YYYYYYY, but alias higher up (we avoid the
		 * alias condition because it causes pmap difficulties)
		 * XXX: We also assume that 4/[23]00 obio addresses
		 * must be 0xZYYYYYYY, where (Z != 0)
		 */
		if (cpumod==SUN4_100 && (cf->cf_loc[0] & 0xf0000000))
			return 0;
		if (cpumod!=SUN4_100 && !(cf->cf_loc[0] & 0xf0000000))
			return 0;
	}

	if (parent->dv_cfdata->cf_driver->cd_indirect) {
		printf(" indirect devices not supported\n");
		return 0;
	}

	oca.ca_ra.ra_iospace = -1;
	oca.ca_ra.ra_paddr = (void *)cf->cf_loc[0];
	oca.ca_ra.ra_len = 0;
	oca.ca_ra.ra_nreg = 1;
	tmp = NULL;
	if (oca.ca_ra.ra_paddr)
		tmp = bus_tmp(oca.ca_ra.ra_paddr,
		    bustype);
	oca.ca_ra.ra_vaddr = tmp;
	oca.ca_ra.ra_intr[0].int_pri = cf->cf_loc[1];
	if (bustype == BUS_VME16 || bustype == BUS_VME32)
		oca.ca_ra.ra_intr[0].int_vec = cf->cf_loc[2];
	else
		oca.ca_ra.ra_intr[0].int_vec = -1;
	oca.ca_ra.ra_nintr = 1;
	oca.ca_ra.ra_name = cf->cf_driver->cd_name;
	if (ca->ca_ra.ra_bp != NULL &&
	  ((bustype == BUS_VME16 && strcmp(ca->ca_ra.ra_bp->name,"vmes") ==0) ||
	   (bustype == BUS_VME32 && strcmp(ca->ca_ra.ra_bp->name,"vmel") ==0) ||
	   (bustype == BUS_OBIO && strcmp(ca->ca_ra.ra_bp->name,"obio") == 0)))
		oca.ca_ra.ra_bp = ca->ca_ra.ra_bp + 1;
	else
		oca.ca_ra.ra_bp = NULL;
	oca.ca_bustype = bustype;

	if ((*cf->cf_driver->cd_match)(parent, cf, &oca) == 0)
		return 0;

	/*
	 * check if XXmatch routine replaced the
	 * temporary mapping with a real mapping.
	 */
	if (tmp == oca.ca_ra.ra_vaddr)
		oca.ca_ra.ra_vaddr = NULL;
	/*
	 * or if it has asked us to create a mapping..
	 * (which won't be seen on future XXmatch calls,
	 * so not as useful as it seems.)
	 */
	if (oca.ca_ra.ra_len)
		oca.ca_ra.ra_vaddr =
		    bus_map(oca.ca_ra.ra_paddr,
		    oca.ca_ra.ra_len, oca.ca_bustype);

	config_attach(parent, cf, &oca, busprint);
	return 1;
}

int
obio_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	return busattach(parent, child, args, BUS_OBIO);
}

void
obioattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	if (self->dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}
	printf("\n");

	(void)config_search(obio_scan, self, args);
	bus_untmp();
}

struct intrhand **vmeints;

int
vmes_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	return busattach(parent, child, args, BUS_VME16);
}

void
vmesattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	if (self->dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}
	printf("\n");

	if (vmeints == NULL) {
		vmeints = (struct intrhand **)malloc(256 *
		    sizeof(struct intrhand *), M_TEMP, M_NOWAIT);
		bzero(vmeints, 256 * sizeof(struct intrhand *));
	}
	(void)config_search(vmes_scan, self, args);
	bus_untmp();
}

int
vmel_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	return busattach(parent, child, args, BUS_VME32);
}

void
vmelattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	if (self->dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}
	printf("\n");

	if (vmeints == NULL) {
		vmeints = (struct intrhand **)malloc(256 *
		    sizeof(struct intrhand *), M_TEMP, M_NOWAIT);
		bzero(vmeints, 256 * sizeof(struct intrhand *));
	}
	(void)config_search(vmel_scan, self, args);
	bus_untmp();
}

int pil_to_vme[] = {
	-1,	/* pil 0 */
	-1,	/* pil 1 */
	1,	/* pil 2 */
	2,	/* pil 3 */
	-1,	/* pil 4 */
	3,	/* pil 5 */
	-1,	/* pil 6 */
	4,	/* pil 7 */
	-1,	/* pil 8 */
	5,	/* pil 9 */
	-1,	/* pil 10 */
	6,	/* pil 11 */
	-1,	/* pil 12 */
	7,	/* pil 13 */
	-1,	/* pil 14 */
	-1,	/* pil 15 */
};

int
vmeintr(arg)
	void *arg;
{
	int level = (int)arg, vec;
	struct intrhand *ih;
	int i = 0;

	vec = ldcontrolb(AC_VMEINTVEC | (pil_to_vme[level] << 1) | 1);
	if (vec == -1) {
		printf("vme: spurious interrupt\n");
		return 0;
	}

	for (ih = vmeints[vec]; ih; ih = ih->ih_next)
		if (ih->ih_fun)
			i += (ih->ih_fun)(ih->ih_arg);
	return (i);
}

void
vmeintr_establish(vec, level, ih)
	int vec, level;
	struct intrhand *ih;
{	
	struct intrhand *ihs;

	if (vec == -1)
		panic("vmeintr_establish: uninitialized vec\n");

	if (vmeints[vec] == NULL)
		vmeints[vec] = ih;
	else {
		for (ihs = vmeints[vec]; ihs->ih_next; ihs = ihs->ih_next)
			;
		ihs->ih_next = ih;
	}

	/* ensure the interrupt subsystem will call us at this level */
	for (ihs = intrhand[level]; ihs; ihs = ihs->ih_next)
		if (ihs->ih_fun == vmeintr)
			return;

	ihs = (struct intrhand *)malloc(sizeof(struct intrhand),
	    M_TEMP, M_NOWAIT);
	if (ihs == NULL)
		panic("vme_addirq");
	bzero(ihs, sizeof *ihs);
	ihs->ih_fun = vmeintr;
	ihs->ih_arg = (void *)level;
	intr_establish(level, ihs);
}

#define	getpte(va)		lda(va, ASI_PTE)

/*
 * If we can find a mapping that was established by the rom, use it.
 * Else, create a new mapping.
 */
void *
bus_map(pa, len, bustype)
	void *pa;
	int len;
	int bustype;
{
	u_long	pf = (u_long)pa >> PGSHIFT;
	u_long	va, pte;
	int pgtype;

	switch (bt2pmt[bustype]) {
	case PMAP_OBIO:
		pgtype = PG_OBIO;
		break;
	case PMAP_VME32:
		pgtype = PG_VME32;
		break;
	case PMAP_VME16:
		pgtype = PG_VME16;
		break;
	}

	if (len <= NBPG) {
		for (va = OLDMON_STARTVADDR; va < OLDMON_ENDVADDR; va += NBPG) {
			pte = getpte(va);
			if ((pte & PG_V) != 0 && (pte & PG_TYPE) == pgtype &&
			    (pte & PG_PFNUM) == pf)
				return ((void *)va);
		}
	}
	return mapiodev(pa, len, bustype);
}

void *
bus_tmp(pa, bustype)
	void *pa;
	int bustype;
{
	vm_offset_t addr = (vm_offset_t)pa & ~PGOFSET;
	int pmtype = bt2pmt[bustype];

	pmap_enter(pmap_kernel(), TMPMAP_VA,
	    addr | pmtype | PMAP_NC,
	    VM_PROT_READ | VM_PROT_WRITE, 1);
	return ((void *)TMPMAP_VA);
}

void
bus_untmp()
{
	pmap_remove(pmap_kernel(), TMPMAP_VA, TMPMAP_VA+NBPG);
}
