/*	$NetBSD: obio.c,v 1.24 1996/05/18 12:22:49 mrg Exp $	*/

/*
 * Copyright (c) 1993, 1994 Theo de Raadt
 * Copyright (c) 1995 Paul Kranenburg
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
#include <sys/systm.h>
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
#include <sparc/dev/sbusvar.h>

struct bus_softc {
	union {
		struct	device scu_dev;		/* base device */
		struct	sbus_softc scu_sbus;	/* obio is another sbus slot */
	} bu;
#define sc_dev	bu.scu_dev
};

/* autoconfiguration driver */
static int	busmatch __P((struct device *, void *, void *));
static void	obioattach __P((struct device *, struct device *, void *));
static void	vmesattach __P((struct device *, struct device *, void *));
static void	vmelattach __P((struct device *, struct device *, void *));

int		busprint __P((void *, const char *));
static int	busattach __P((struct device *, void *, void *, int));
void *		bus_map __P((struct rom_reg *, int, int));
int		obio_scan __P((struct device *, void *, void *));
int 		vmes_scan __P((struct device *, void *, void *));
int 		vmel_scan __P((struct device *, void *, void *));
int 		vmeintr __P((void *));

struct cfattach obio_ca = {
	sizeof(struct bus_softc), busmatch, obioattach
};

struct cfdriver obio_cd = {
	NULL, "obio", DV_DULL
};

struct cfattach vmel_ca = {
	sizeof(struct bus_softc), busmatch, vmelattach
};

struct cfdriver vmel_cd = {
	NULL, "vmel", DV_DULL
};

struct cfattach vmes_ca = {
	sizeof(struct bus_softc), busmatch, vmesattach
};

struct cfdriver vmes_cd = {
	NULL, "vmes", DV_DULL
};

struct intrhand **vmeints;


int
busmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (CPU_ISSUN4M)
		return (strcmp(cf->cf_driver->cd_name, ra->ra_name) == 0);

	if (!CPU_ISSUN4)
		return (0);

	return (strcmp(cf->cf_driver->cd_name, ra->ra_name) == 0);
}

int
busprint(args, obio)
	void *args;
	const char *obio;
{
	register struct confargs *ca = args;

	if (ca->ca_ra.ra_name == NULL)
		ca->ca_ra.ra_name = "<unknown>";

	if (obio)
		printf("[%s at %s]", ca->ca_ra.ra_name, obio);

	printf(" addr %p", ca->ca_ra.ra_paddr);

	if (CPU_ISSUN4 && ca->ca_ra.ra_intr[0].int_vec != -1)
		printf(" vec 0x%x", ca->ca_ra.ra_intr[0].int_vec);

	return (UNCONF);
}


void
obioattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
#if defined(SUN4M)
	register struct bus_softc *sc = (struct bus_softc *)self;
	struct confargs oca, *ca = args;
	register struct romaux *ra = &ca->ca_ra;
	register int node0, node;
	register char *name;
	register const char *sp;
	const char *const *ssp;
	extern int autoconf_nzs;

	static const char *const special4m[] = {
		/* find these first */
		"eeprom",
		"counter",
		"auxio",
		"",
		/* place device to ignore here */
		"interrupt",
		NULL
	};
#endif

	if (CPU_ISSUN4) {
		if (self->dv_unit > 0) {
			printf(" unsupported\n");
			return;
		}
		printf("\n");

		(void)config_search(obio_scan, self, args);
		bus_untmp();
	}

#if defined(SUN4M)
	if (!CPU_ISSUN4M)
		return;

	/*
	 * There is only one obio bus (it is in fact one of the Sbus slots)
	 * How about VME?
	 */
	if (sc->sc_dev.dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}

	printf("\n");

	if (ra->ra_bp != NULL && strcmp(ra->ra_bp->name, "obio") == 0)
		oca.ca_ra.ra_bp = ra->ra_bp + 1;
	else
		oca.ca_ra.ra_bp = NULL;

	sc->bu.scu_sbus.sc_range = ra->ra_range;
	sc->bu.scu_sbus.sc_nrange = ra->ra_nrange;

	/*
	 * Loop through ROM children, fixing any relative addresses
	 * and then configuring each device.
	 * We first do the crucial ones, such as eeprom, etc.
	 */
	node0 = firstchild(ra->ra_node);
	for (ssp = special4m ; *(sp = *ssp) != 0; ssp++) {
		if ((node = findnode(node0, sp)) == 0) {
			printf("could not find %s amongst obio devices\n", sp);
			panic(sp);
		}
		if (!romprop(&oca.ca_ra, sp, node))
			continue;

		sbus_translate(self, &oca);
		oca.ca_bustype = BUS_OBIO;
		(void) config_found(&sc->sc_dev, (void *)&oca, busprint);
	}

	for (node = node0; node; node = nextsibling(node)) {
		name = getpropstring(node, "name");
		for (ssp = special4m ; (sp = *ssp) != NULL; ssp++)
			if (strcmp(name, sp) == 0)
				break;

		if (sp != NULL || !romprop(&oca.ca_ra, name, node))
			continue;

		if (strcmp(name, "zs") == 0)
			/* XXX - see autoconf.c for this hack */
			autoconf_nzs++;

		/* Translate into parent address spaces */
		sbus_translate(self, &oca);
		oca.ca_bustype = BUS_OBIO;
		(void) config_found(&sc->sc_dev, (void *)&oca, busprint);
	}
#endif
}

void
vmesattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	if (CPU_ISSUN4M || self->dv_unit > 0) {
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

void
vmelattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	if (CPU_ISSUN4M || self->dv_unit > 0) {
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

int
busattach(parent, child, args, bustype)
	struct device *parent;
	void *args, *child;
	int bustype;
{
#if defined(SUN4)
	struct cfdata *cf = child;
	register struct confargs *ca = args;
	struct confargs oca;
	caddr_t tmp;

	if (bustype == BUS_OBIO && CPU_ISSUN4) {

		/*
		 * avoid sun4m entries which don't have valid PA's.
		 * no point in even probing them. 
		 */
		if (cf->cf_loc[0] == -1) return 0;

		/*
		 * On the 4/100 obio addresses must be mapped at
		 * 0x0YYYYYYY, but alias higher up (we avoid the
		 * alias condition because it causes pmap difficulties)
		 * XXX: We also assume that 4/[23]00 obio addresses
		 * must be 0xZYYYYYYY, where (Z != 0)
		 */
		if (cpumod == SUN4_100 && (cf->cf_loc[0] & 0xf0000000))
			return 0;
		if (cpumod != SUN4_100 && !(cf->cf_loc[0] & 0xf0000000))
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
	if (oca.ca_ra.ra_paddr)
		tmp = (caddr_t)bus_tmp(oca.ca_ra.ra_paddr,
		    bustype);
	else
		tmp = NULL;
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

	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return 0;

	/*
	 * check if XXmatch routine replaced the temporary mapping with
	 * a real mapping.   If not, then make sure we don't pass the
	 * tmp mapping to the attach routine.
	 */
	if (oca.ca_ra.ra_vaddr == tmp)
		oca.ca_ra.ra_vaddr = NULL; /* wipe out tmp address */
	/*
	 * the match routine will set "ra_len" if it wants us to
	 * establish a mapping for it.
	 * (which won't be seen on future XXmatch calls,
	 * so not as useful as it seems.)
	 */
	if (oca.ca_ra.ra_len)
		oca.ca_ra.ra_vaddr =
		    bus_map(oca.ca_ra.ra_reg,
		    oca.ca_ra.ra_len, oca.ca_bustype);

	config_attach(parent, cf, &oca, busprint);
	return 1;
#else
	return 0;
#endif
}

int
obio_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	return busattach(parent, child, args, BUS_OBIO);
}

int
vmes_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	return busattach(parent, child, args, BUS_VME16);
}

int
vmel_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	return busattach(parent, child, args, BUS_VME32);
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

#ifdef DIAGNOSTIC
	if (!CPU_ISSUN4) {
		panic("vme: spurious interrupt");
	}
#endif

	vec = ldcontrolb((caddr_t)
	    (AC_VMEINTVEC | (pil_to_vme[level] << 1) | 1));
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

	if (!CPU_ISSUN4) {
		panic("vmeintr_establish: not supported on cpu-type %d",
		      cputyp);
	}

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
	struct rom_reg *pa;
	int len;
	int bustype;
{
	u_long	pf = (u_long)(pa->rr_paddr) >> PGSHIFT;
	u_long	va, pte;
	int pgtype = -1;

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
				return ((void *)
				    (va | ((u_long)pa->rr_paddr & PGOFSET)) );
					/* note: preserve page offset */
		}
	}
	return mapiodev(pa, 0, len, bustype);
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
	return ((void *)(TMPMAP_VA | ((u_long) pa & PGOFSET)) );
}

void
bus_untmp()
{
	pmap_remove(pmap_kernel(), TMPMAP_VA, TMPMAP_VA+NBPG);
}
