/*	$OpenBSD: obio.c,v 1.23 2015/01/14 21:35:43 miod Exp $	*/
/*	$NetBSD: obio.c,v 1.37 1997/07/29 09:58:11 fair Exp $	*/

/*
 * Copyright (c) 1993, 1994 Theo de Raadt
 * Copyright (c) 1995, 1997 Paul Kranenburg
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
 * 3. The name of the author may not be used to endorse or promote products
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

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/pmap.h>
#include <machine/oldmon.h>
#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <sparc/sparc/asm.h>
#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/dev/sbusvar.h>
#include <sparc/dev/vmereg.h>

struct vmebus_softc { 
	struct device	 sc_dev;	/* base device */
	struct vmebusreg *sc_reg; 	/* VME control registers */
	struct vmebusvec *sc_vec;	/* VME interrupt vector */
	struct rom_range *sc_range;	/* ROM range property */
	int		 sc_nrange;
};
struct  vmebus_softc *vmebus_sc;/*XXX*/

struct bus_softc {
	union {
		struct	device scu_dev;		/* base device */
		struct	sbus_softc scu_sbus;	/* obio is another sbus slot */
		struct	vmebus_softc scu_vme;
	} bu;
};


/* autoconfiguration driver */
static int	busmatch(struct device *, void *, void *);
static void	obioattach(struct device *, struct device *, void *);
static void	vmesattach(struct device *, struct device *, void *);
static void	vmelattach(struct device *, struct device *, void *);
static void	vmeattach(struct device *, struct device *, void *);

int		busprint(void *, const char *);
int		vmeprint(void *, const char *);
static int	busattach(struct device *, void *, void *, int);
int		obio_scan(struct device *, void *, void *);
int 		vmes_scan(struct device *, void *, void *);
int 		vmel_scan(struct device *, void *, void *);
void		vmebus_translate(struct device *, struct confargs *, int);
int 		vmeintr(void *);

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

struct cfattach vme_ca = {
	sizeof(struct bus_softc), busmatch, vmeattach
};

struct cfdriver vme_cd = {
	NULL, "vme", DV_DULL
};

struct intrhand **vmeints;

/*
 * 4/110 comment: the 4/110 chops off the top 4 bits of an OBIO address.
 *	this confuses autoconf.  for example, if you try and map
 *	0xfe000000 in obio space on a 4/110 it actually maps 0x0e000000.
 *	this is easy to verify with the PROM.   this causes problems
 *	with devices like "esp0 at obio0 addr 0xfa000000" because the
 *	4/110 treats it as esp0 at obio0 addr 0x0a000000" which is the
 *	address of the 4/110's "sw0" scsi chip.   the same thing happens
 *	between zs1 and zs2.    since the sun4 line is "closed" and
 *	we know all the "obio" devices that will ever be on it we just
 *	put in some special case "if"'s in the match routines of esp,
 *	dma, and zs.
 */

int
busmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	register struct cfdata *cf = vcf;
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
		printf("%s at %s", ca->ca_ra.ra_name, obio);

	printf(" addr %p", ca->ca_ra.ra_paddr);

	if (CPU_ISSUN4 && ca->ca_ra.ra_intr[0].int_vec != -1)
		printf(" vec 0x%x", ca->ca_ra.ra_intr[0].int_vec);

	return (UNCONF);
}

int
vmeprint(args, name)
	void *args;
	const char *name;
{
	register struct confargs *ca = args;

	if (name)
		printf("%s at %s", ca->ca_ra.ra_name, name);
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
	int rlen;
	extern int autoconf_nzs;

	static const char *const special4m[] = {
		/* find these first */
		"eeprom",
		"counter",
#if 0 /* Not all sun4m's have an `auxio' */
		"auxio",
#endif
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
	 * There is only one obio bus (it is in fact one of the SBus slots)
	 * How about VME?
	 */
	if (self->dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}

	printf("\n");

	if (ra->ra_bp != NULL && strcmp(ra->ra_bp->name, "obio") == 0)
		oca.ca_ra.ra_bp = ra->ra_bp + 1;
	else
		oca.ca_ra.ra_bp = NULL;

	node = ra->ra_node;
	rlen = getproplen(node, "ranges");
	if (rlen > 0) {
		sc->bu.scu_sbus.sc_nrange = rlen / sizeof(struct rom_range);
		sc->bu.scu_sbus.sc_range =
			(struct rom_range *)malloc(rlen, M_DEVBUF, M_NOWAIT);
		if (sc->bu.scu_sbus.sc_range == 0)
			panic("obio: PROM ranges too large: %d", rlen);
		(void)getprop(node, "ranges", sc->bu.scu_sbus.sc_range, rlen);
	}

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
		(void) config_found(self, (void *)&oca, busprint);
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
		(void) config_found(self, (void *)&oca, busprint);
	}
#endif
}

void
vmesattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	if (self->dv_unit > 0 ||
	    (CPU_ISSUN4M && strcmp(parent->dv_cfdata->cf_driver->cd_name, "vme") != 0)) {
		printf(" unsupported\n");
		return;
	}
	printf("\n");

	if (vmeints == NULL) {
		vmeints = malloc(256 * sizeof(struct intrhand *), M_TEMP,
		    M_NOWAIT | M_ZERO);
		if (vmeints == NULL)
			panic("vmesattach: can't allocate intrhand");
	}
	(void)config_search(vmes_scan, self, args);
	bus_untmp();
}

void
vmelattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	if (self->dv_unit > 0 ||
	    (CPU_ISSUN4M && strcmp(parent->dv_cfdata->cf_driver->cd_name, "vme") != 0)) {
		printf(" unsupported\n");
		return;
	}
	printf("\n");

	if (vmeints == NULL) {
		vmeints = malloc(256 * sizeof(struct intrhand *), M_TEMP,
		    M_NOWAIT | M_ZERO);
		if (vmeints == NULL)
			panic("vmelattach: can't allocate intrhand");
	}
	(void)config_search(vmel_scan, self, args);
	bus_untmp();
}

void
vmeattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct vmebus_softc *sc = (struct vmebus_softc *)self;
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;
	int node, rlen;
	struct confargs oca;

	if (!CPU_ISSUN4M || self->dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}

	node = ra->ra_node;

	sc->sc_reg = (struct vmebusreg *)
		mapiodev(&ra->ra_reg[0], 0, ra->ra_reg[0].rr_len);
	sc->sc_vec = (struct vmebusvec *)
		mapiodev(&ra->ra_reg[1], 0, ra->ra_reg[1].rr_len);

	/*
	 * Get "range" property, though we don't do anything with it yet.
	 */
	rlen = getproplen(node, "ranges");
	if (rlen > 0) {
		sc->sc_nrange = rlen / sizeof(struct rom_range);
		sc->sc_range =
			(struct rom_range *)malloc(rlen, M_DEVBUF, M_NOWAIT);
		if (sc->sc_range == 0)  
			panic("vme: PROM ranges too large: %d", rlen);
		(void)getprop(node, "ranges", sc->sc_range, rlen);
	}

	vmebus_sc = sc;
	printf(": version 0x%x\n",
	       sc->sc_reg->vmebus_cr & VMEBUS_CR_IMPL);

	if (ra->ra_bp != NULL && strcmp(ra->ra_bp->name, "vme") == 0)
		oca.ca_ra.ra_bp = ra->ra_bp + 1;
	else
		oca.ca_ra.ra_bp = NULL;

	oca.ca_ra.ra_name = "vmes";
	oca.ca_bustype = BUS_MAIN;
	(void)config_found(self, (void *)&oca, vmeprint);

	oca.ca_ra.ra_name = "vmel";
	oca.ca_bustype = BUS_MAIN;
	(void)config_found(self, (void *)&oca, vmeprint);
}

void
vmebus_translate(dev, ca, bustype)
	struct device *dev;
	struct confargs *ca;
	int bustype;
{
	struct vmebus_softc *sc = (struct vmebus_softc *)dev;
	register int j;
	int cspace;

	if (sc->sc_nrange == 0)
		panic("vmebus: no ranges");

	/*
	 * Find VMEbus modifier based on address space.
	 * XXX - should not be encoded in `ra_paddr'
	 */
	if (((u_long)ca->ca_ra.ra_paddr & 0xffff0000) == 0xffff0000)
		cspace = VMEMOD_A16_D_S;
	else if (((u_long)ca->ca_ra.ra_paddr & 0xff000000) == 0xff000000)
		cspace = VMEMOD_A24_D_S;
	else
		cspace = VMEMOD_A32_D_S;

	cspace |= (bustype == BUS_VME32) ? VMEMOD_D32 : 0;

	/* Translate into parent address spaces */
	for (j = 0; j < sc->sc_nrange; j++) {
		if (sc->sc_range[j].cspace == cspace) {
#if notyet
			ca->ca_ra.ra_paddr +=
				sc->sc_range[j].poffset;
#endif
			ca->ca_ra.ra_iospace =
				sc->sc_range[j].pspace;
			break;
		}
	}
}

int bt2pmt[] = {
	PMAP_OBIO,
	PMAP_OBIO,
	PMAP_VME16,
	PMAP_VME32,
	PMAP_OBIO 
}; 

int
busattach(parent, vcf, args, bustype)
	struct device *parent;
	void *vcf, *args;
	int bustype;
{
#if defined(SUN4) || defined(SUN4M)
	register struct cfdata *cf = vcf;
	register struct confargs *ca = args;
	struct confargs oca;
	paddr_t pa = (paddr_t)cf->cf_loc[0];
	caddr_t tmp;

	if (bustype == BUS_OBIO && CPU_ISSUN4) {
		/*
		 * avoid sun4m entries which don't have valid PA's.
		 * no point in even probing them. 
		 */
		if (cf->cf_loc[0] == -1) return 0;

		/*
		 * On the 4/100 obio addresses must be mapped at
		 * 0x0YYYYYYY, but alias higher up (the kernel
		 * configuration file uses addresses with the top
		 * four bits set). We ignore 4/[23]00 devices here.
		 */
		if (cpuinfo.cpu_type == CPUTYP_4_100) {
			if ((pa & 0xf0000000) != 0xf0000000)
				return 0;
		}
	}

	oca.ca_ra.ra_paddr = (void *)pa;
	oca.ca_ra.ra_len = 0;
	oca.ca_ra.ra_nreg = 1;
	if (CPU_ISSUN4M)
		vmebus_translate(parent->dv_parent, &oca, bustype);
	else
		oca.ca_ra.ra_iospace = bt2pmt[bustype];

	if (oca.ca_ra.ra_paddr)
		tmp = (caddr_t)mapdev(oca.ca_ra.ra_reg, TMPMAP_VA, 0, NBPG);
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
		    bus_map(oca.ca_ra.ra_reg, oca.ca_ra.ra_len);

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
	int pil = (int)arg, level, vec;
	struct intrhand *ih;
	int r, i = 0;

	level = (pil_to_vme[pil] << 1) | 1;

	if (CPU_ISSUN4) {
		vec = ldcontrolb((caddr_t)(AC_VMEINTVEC | level));
	} else if (CPU_ISSUN4M) {
		vec = vmebus_sc->sc_vec->vmebusvec[level];
	} else
		panic("vme: spurious interrupt");

	if (vec == -1) {
		printf("vme: spurious interrupt\n");
		return 0;
	}

	for (ih = vmeints[vec]; ih; ih = ih->ih_next)
		if (ih->ih_fun) {
			r = (ih->ih_fun)(ih->ih_arg);
			if (r > 0) {
				ih->ih_count.ec_count++;
				return (r);
			}
			i |= r;
		}
	return (i);
}

void
vmeintr_establish(vec, level, ih, ipl_block, name)
	int vec, level;
	struct intrhand *ih;
	int ipl_block;
	const char *name;
{
	struct intrhand *ihs;

	if (vmeints == NULL)
		panic("vmeintr_establish: interrupt vector not allocated");

	if (vec == -1)
		panic("vmeintr_establish: uninitialized vec");

	if (vmeints[vec] == NULL)
		vmeints[vec] = ih;
	else {
		for (ihs = vmeints[vec]; ihs->ih_next; ihs = ihs->ih_next)
			;
		ihs->ih_next = ih;
	}

	if (name != NULL) {
		ih->ih_vec = vec;
		evcount_attach(&ih->ih_count, name, &ih->ih_vec);
	}

	/* ensure the interrupt subsystem will call us at this level */
	for (ihs = intrhand[level]; ihs; ihs = ihs->ih_next)
		if (ihs->ih_fun == vmeintr)
			return;

	ihs = malloc(sizeof(*ihs), M_TEMP, M_NOWAIT | M_ZERO);
	if (ihs == NULL)
		panic("vme_addirq");
	ihs->ih_fun = vmeintr;
	ihs->ih_arg = (void *)level;
	intr_establish(level, ihs, ipl_block, NULL);
}

#define	getpte(va)		lda(va, ASI_PTE)

/*
 * If we can find a mapping that was established by the rom, use it.
 * Else, create a new mapping.
 */
void *
bus_map(pa, len)
	struct rom_reg *pa;
	int len;
{
#ifdef SUN4
	if (CPU_ISSUN4 && len <= NBPG) {
		paddr_t paddr;
		vaddr_t va;
		u_long	pf, pte;
		int pgtype = PMAP_T2PTE_4(pa->rr_iospace);

		if (cpuinfo.cpu_type == CPUTYP_4_100)
			paddr = ((paddr_t)pa->rr_paddr) & 0x0fffffff;
		else
			paddr = (paddr_t)pa->rr_paddr;
		pf = paddr >> PGSHIFT;

		for (va = OLDMON_STARTVADDR; va < OLDMON_ENDVADDR; va += NBPG) {
			pte = getpte(va);
			if ((pte & PG_V) != 0 && (pte & PG_TYPE) == pgtype &&
			    (pte & PG_PFNUM) == pf)
				return ((void *)
				    (va | ((u_long)pa->rr_paddr & PGOFSET)) );
					/* note: preserve page offset */
		}
	}
#endif

	return mapiodev(pa, 0, len);
}

void
bus_untmp()
{
	pmap_remove(pmap_kernel(), TMPMAP_VA, TMPMAP_VA+NBPG);
	pmap_update(pmap_kernel());
}
