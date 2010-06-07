/*	$OpenBSD: cpu.c,v 1.45 2010/06/07 19:44:54 miod Exp $	*/
/*	$NetBSD: cpu.c,v 1.56 1997/09/15 20:52:36 pk Exp $ */

/*
 * Copyright (c) 1996
 *	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
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
 *	@(#)cpu.c	8.5 (Berkeley) 11/23/93
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/ctlreg.h>
#include <machine/trap.h>
#include <machine/pmap.h>

#include <machine/oldmon.h>
#include <machine/idprom.h>

#include <sparc/sparc/cache.h>
#include <sparc/sparc/asm.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/sparc/memreg.h>

#ifdef solbourne
#include <machine/idt.h>
#include <machine/kap.h>
#include <machine/prom.h>
#endif

/* The following are used externally (sysctl_hw). */
char	machine[] = MACHINE;		/* from <machine/param.h> */
char	*cpu_class = "sun4";
char	cpu_model[130];
char	cpu_hotfix[40];
extern char mainbus_model[];		/* from autoconf.c */

int	foundfpu;			/* from machine/cpu.h */

/* The CPU configuration driver. */
void cpu_attach(struct device *, struct device *, void *);
int  cpu_match(struct device *, void *, void *);

struct cfattach cpu_ca = {
	sizeof(struct cpu_softc), cpu_match, cpu_attach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_CPU
};

char *fsrtoname(int, int, int, char *, size_t);
void cache_print(struct cpu_softc *);
void cpu_spinup(struct cpu_softc *);
void fpu_init(struct cpu_softc *);
void replacemul(void);

#define	IU_IMPL(psr)	((u_int)(psr) >> 28)
#define	IU_VERS(psr)	(((psr) >> 24) & 0xf)

#define SRMMU_IMPL(mmusr)	((u_int)(mmusr) >> 28)
#define SRMMU_VERS(mmusr)	(((mmusr) >> 24) & 0xf)


#ifdef notdef
/*
 * IU implementations are parceled out to vendors (with some slight
 * glitches).  Printing these is cute but takes too much space.
 */
static char *iu_vendor[16] = {
	"Fujitsu",	/* and also LSI Logic */
	"ROSS",		/* ROSS (ex-Cypress) */
	"BIT",
	"LSIL",		/* LSI Logic finally got their own */
	"TI",		/* Texas Instruments */
	"Matsushita",
	"Philips",
	"Harvest",	/* Harvest VLSI Design Center */
	"SPEC",		/* Systems and Processes Engineering Corporation */
	"Weitek",
	"vendor#10",
	"vendor#11",
	"vendor#12",
	"vendor#13",
	"vendor#14",
	"vendor#15"
};
#endif

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
cpu_match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	register struct cfdata *cf = vcf;
	register struct confargs *ca = aux;

	return (strcmp(cf->cf_driver->cd_name, ca->ca_ra.ra_name) == 0);
}

/*
 * Attach the CPU.
 * Discover interesting goop about the virtual address cache
 * (slightly funny place to do it, but this is where it is to be found).
 */
void
cpu_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct cpu_softc *sc = (struct cpu_softc *)self;
	register int node;
	register char *fpuname;
	struct confargs *ca = aux;
	char fpbuf[40];
	char model[100];

	sc->node = node = ca->ca_ra.ra_node;

	/*
	 * First, find out if we're attaching the boot CPU.
	 */
	if (node == 0)
		sc->master = 1;
	else {
		sc->mid = getpropint(node, "mid", 0);
		if (sc->mid == 0 || sc->mid == getmid() + 8 /*XXX*/)
			sc->master = 1;
	}

	if (sc->master) {
		/*
		 * Gross, but some things in cpuinfo may already have
		 * been setup by early routines like pmap_bootstrap().
		 */
		bcopy(&sc->dv, &cpuinfo, sizeof(sc->dv));
		bcopy(&cpuinfo, sc, sizeof(cpuinfo));
	}

#if defined(SUN4C) || defined(SUN4M)
	switch (cputyp) {
#if defined(SUN4C)
	case CPU_SUN4C:
		cpu_class = "sun4c";
		break;
#endif /* defined(SUN4C) */
#if defined(SUN4M)
	case CPU_SUN4M:
		cpu_class = "sun4m";
		break;
#endif /* defined(SUN4M) */
	}
#endif /* defined(SUN4C) || defined(SUN4M) */

	getcpuinfo(sc, node);

	fpuname = "no";
	if (sc->master) {
		if (sc->hotfix)
			sc->hotfix(sc);

		fpu_init(sc);
		if (foundfpu)
			fpuname = fsrtoname(sc->cpu_impl, sc->cpu_vers,
			    sc->fpuvers, fpbuf, sizeof fpbuf);
	}
	/* XXX - multi-processor: take care of `cpu_model' and `foundfpu' */

	snprintf(model, sizeof model, "%s @ %s MHz, %s FPU", sc->cpu_name,
	    clockfreq(sc->hz), fpuname);
	printf(": %s", model);
	snprintf(cpu_model, sizeof cpu_model, "%s, %s", mainbus_model, model);

	if (cpu_hotfix[0])
		printf("; %s", cpu_hotfix);
	printf("\n");

	if (sc->cacheinfo.c_totalsize != 0)
		cache_print(sc);

	sc->ci.ci_softc = sc;

	if (sc->master) {
		int s;

		bcopy(sc, &cpuinfo, sizeof(cpuinfo));
		/*
		 * Enable the cache
		 *
		 * Disable all interrupts because we don't want anything
		 * nasty to happen to the pagetables while the cache is
		 * enabled and we haven't uncached them yet.
		 */
		if (sc->cacheinfo.c_totalsize != 0) {
			s = splhigh();
			sc->cache_enable();
			pmap_cache_enable();
			splx(s);
		}
		return;
	}

	/* Now start this CPU */
}

#if 0
void
cpu_(sc)
	struct cpu_softc *sc;
{
	if (sc->hotfix)
		sc->hotfix(sc);

	/* Initialize FPU */
	fpu_init(sc);

	/* Enable the cache */
	sc->cache_enable();
}
#endif

void
cpu_spinup(sc)
	struct cpu_softc *sc;
{
#if 0
	pmap_cpusetup();
#endif
}

void
fpu_init(sc)
	struct cpu_softc *sc;
{
	struct fpstate fpstate;

	/*
	 * Get the FSR and clear any exceptions.  If we do not unload
	 * the queue here and it is left over from a previous crash, we
	 * will panic in the first loadfpstate(), due to a sequence
	 * error, so we need to dump the whole state anyway.
	 *
	 * If there is no FPU, trap.c will advance over all the stores,
	 * so we initialize fs_fsr here.
	 */

	/* 7 is reserved for "none" */
	fpstate.fs_fsr = 7 << FSR_VER_SHIFT;
	savefpstate(&fpstate);
	sc->fpuvers =
	    (fpstate.fs_fsr >> FSR_VER_SHIFT) & (FSR_VER >> FSR_VER_SHIFT);

	if (sc->fpuvers != 7)
		foundfpu = 1;
}

void
cache_print(sc)
	struct cpu_softc *sc;
{
	struct cacheinfo *ci = &sc->cacheinfo;
	char *sep = "";

	printf("%s: ", sc->dv.dv_xname);

	if (ci->c_split) {
		printf("%s", (ci->c_physical ? "physical " : ""));
		if (ci->ic_totalsize > 0) {
			printf("%s%dK instruction (%d b/l)", sep,
			    ci->ic_totalsize/1024, ci->ic_linesize);
			sep = ", ";
		}
		if (ci->dc_totalsize > 0) {
			printf("%s%dK data (%d b/l)", sep,
			    ci->dc_totalsize/1024, ci->dc_linesize);
			sep = ", ";
		}
	} else if (ci->c_physical) {
		/* combined, physical */
		printf("physical %dK combined cache (%d bytes/line)",
		    ci->c_totalsize/1024, ci->c_linesize);
		sep = ", ";
	} else {
		/* combined, virtual */
		printf("%dK byte write-%s, %d bytes/line, %cw flush",
		    ci->c_totalsize/1024,
		    (ci->c_vactype == VAC_WRITETHROUGH) ? "through" : "back",
		    ci->c_linesize, ci->c_hwflush ? 'h' : 's');
		sep = ", ";
	}

	if (ci->ec_totalsize > 0) {
		printf("%s%dK external (%d b/l)", sep,
		    ci->ec_totalsize/1024, ci->ec_linesize);
	}
	if (sep)	/* printed at least one field.. */
		printf(" ");
}


/*------------*/


void cpumatch_unknown(struct cpu_softc *, struct module_info *, int);
void cpumatch_sun4(struct cpu_softc *, struct module_info *, int);
void cpumatch_sun4c(struct cpu_softc *, struct module_info *, int);
void cpumatch_ms(struct cpu_softc *, struct module_info *, int);
void cpumatch_viking(struct cpu_softc *, struct module_info *, int);
void cpumatch_hypersparc(struct cpu_softc *, struct module_info *, int);
void cpumatch_turbosparc(struct cpu_softc *, struct module_info *, int);
void cpumatch_kap(struct cpu_softc *, struct module_info *, int);

void getcacheinfo_sun4(struct cpu_softc *, int node);
void getcacheinfo_sun4c(struct cpu_softc *, int node);
void getcacheinfo_obp(struct cpu_softc *, int node);
void getcacheinfo_kap(struct cpu_softc *, int node);

void sun4_hotfix(struct cpu_softc *);
void viking_hotfix(struct cpu_softc *);
void turbosparc_hotfix(struct cpu_softc *);
void swift_hotfix(struct cpu_softc *);

void ms1_mmu_enable(void);
void viking_mmu_enable(void);
void swift_mmu_enable(void);
void hypersparc_mmu_enable(void);

void srmmu_get_syncflt(void);
void ms1_get_syncflt(void);
void viking_get_syncflt(void);
void swift_get_syncflt(void);
void turbosparc_get_syncflt(void);
void hypersparc_get_syncflt(void);
void cypress_get_syncflt(void);

int srmmu_get_asyncflt(u_int *, u_int *);
int hypersparc_get_asyncflt(u_int *, u_int *);
int cypress_get_asyncflt(u_int *, u_int *);
int no_asyncflt_regs(u_int *, u_int *);

struct module_info module_unknown = {
	CPUTYP_UNKNOWN,
	VAC_UNKNOWN,
	cpumatch_unknown
};


void
cpumatch_unknown(sc, mp, node)
	struct cpu_softc *sc;
	struct module_info *mp;
	int	node;
{
	panic("Unknown CPU type: "
	      "cpu: impl %d, vers %d; mmu: impl %d, vers %d",
		sc->cpu_impl, sc->cpu_vers,
		sc->mmu_impl, sc->mmu_vers);
}

#if defined(SUN4)
struct module_info module_sun4 = {
	CPUTYP_UNKNOWN,
	VAC_WRITETHROUGH,
	cpumatch_sun4,
	getcacheinfo_sun4,
	sun4_hotfix,
	0,
	sun4_cache_enable,
	0,			/* ncontext set in `match' function */
	0,			/* get_syncflt(); unused in sun4 */
	0,			/* get_asyncflt(); unused in sun4 */
	sun4_cache_flush,
	sun4_vcache_flush_page,
	sun4_vcache_flush_segment,
	sun4_vcache_flush_region,
	sun4_vcache_flush_context,
	noop_pcache_flush_line,
	noop_pure_vcache_flush,
	noop_cache_flush_all,
	0
};

void
getcacheinfo_sun4(sc, node)
	struct cpu_softc *sc;
	int	node;
{
	struct cacheinfo *ci = &sc->cacheinfo;

	switch (sc->cpu_type) {
	case CPUTYP_4_100:
		ci->c_vactype = VAC_NONE;
		ci->c_totalsize = 0;
		ci->c_hwflush = 0;
		ci->c_linesize = 0;
		ci->c_l2linesize = 0;
		ci->c_split = 0;
		ci->c_nlines = 0;

		/* Override cache flush functions */
		sc->cache_flush = noop_cache_flush;
		sc->vcache_flush_page = noop_vcache_flush_page;
		sc->vcache_flush_segment = noop_vcache_flush_segment;
		sc->vcache_flush_region = noop_vcache_flush_region;
		sc->vcache_flush_context = noop_vcache_flush_context;
		break;
	case CPUTYP_4_200:
		ci->c_vactype = VAC_WRITEBACK;
		ci->c_totalsize = 128*1024;
		ci->c_hwflush = 0;
		ci->c_linesize = 16;
		ci->c_l2linesize = 4;
		ci->c_split = 0;
		ci->c_nlines = ci->c_totalsize << ci->c_l2linesize;
		break;
	case CPUTYP_4_300:
		ci->c_vactype = VAC_WRITEBACK;
		ci->c_totalsize = 128*1024;
		ci->c_hwflush = 0;
		ci->c_linesize = 16;
		ci->c_l2linesize = 4;
		ci->c_split = 0;
		ci->c_nlines = ci->c_totalsize << ci->c_l2linesize;
		sc->flags |= CPUFLG_SUN4CACHEBUG;
		break;
	case CPUTYP_4_400:
		ci->c_vactype = VAC_WRITEBACK;
		ci->c_totalsize = 128 * 1024;
		ci->c_hwflush = 0;
		ci->c_linesize = 32;
		ci->c_l2linesize = 5;
		ci->c_split = 0;
		ci->c_nlines = ci->c_totalsize << ci->c_l2linesize;
		break;
	}
}

struct	idprom idprom;
void	getidprom(struct idprom *, int size);

void
cpumatch_sun4(sc, mp, node)
	struct cpu_softc *sc;
	struct module_info *mp;
	int	node;
{

	getidprom(&idprom, sizeof(idprom));
	switch (idprom.id_machine) {
	/* XXX: don't know about Sun4 types */
	case ID_SUN4_100:
		sc->cpu_type = CPUTYP_4_100;
		sc->classlvl = 100;
		sc->mmu_ncontext = 8;
		sc->mmu_nsegment = 256;
/*XXX*/		sc->hz = 14280000;
		break;
	case ID_SUN4_200:
		sc->cpu_type = CPUTYP_4_200;
		sc->classlvl = 200;
		sc->mmu_nsegment = 512;
		sc->mmu_ncontext = 16;
/*XXX*/		sc->hz = 16670000;
		break;
	case ID_SUN4_300:
		sc->cpu_type = CPUTYP_4_300;
		sc->classlvl = 300;
		sc->mmu_nsegment = 256;
		sc->mmu_ncontext = 16;
/*XXX*/		sc->hz = 25000000;
		break;
	case ID_SUN4_400:
		sc->cpu_type = CPUTYP_4_400;
		sc->classlvl = 400;
		sc->mmu_nsegment = 1024;
		sc->mmu_ncontext = 64;
		sc->mmu_nregion = 256;
/*XXX*/		sc->hz = 33000000;
		sc->sun4_mmu3l = 1;
		break;
	}

}
#endif /* SUN4 */

#if defined(SUN4C)
struct module_info module_sun4c = {
	CPUTYP_UNKNOWN,
	VAC_WRITETHROUGH,
	cpumatch_sun4c,
	getcacheinfo_sun4c,
	sun4_hotfix,
	0,
	sun4_cache_enable,
	0,			/* ncontext set in `match' function */
	0,			/* get_syncflt(); unused in sun4c */
	0,			/* get_asyncflt(); unused in sun4c */
	sun4_cache_flush,
	sun4_vcache_flush_page,
	sun4_vcache_flush_segment,
	sun4_vcache_flush_region,
	sun4_vcache_flush_context,
	noop_pcache_flush_line,
	noop_pure_vcache_flush,
	noop_cache_flush_all,
	0
};

void
cpumatch_sun4c(sc, mp, node)
	struct cpu_softc *sc;
	struct module_info *mp;
	int	node;
{
	int	rnode;

	rnode = findroot();
	sc->mmu_npmeg = sc->mmu_nsegment =
		getpropint(rnode, "mmu-npmg", 128);
	sc->mmu_ncontext = getpropint(rnode, "mmu-nctx", 8);
                              
	/* Get clock frequency */ 
	sc->hz = getpropint(rnode, "clock-frequency", 0);
}

void
getcacheinfo_sun4c(sc, node)
	struct cpu_softc *sc;
	int node;
{
	struct cacheinfo *ci = &sc->cacheinfo;
	int i, l;

	if (node == 0)
		/* Bootstrapping */
		return;

	/* Sun4c's have only virtually-addressed caches */
	ci->c_physical = 0; 
	ci->c_totalsize = getpropint(node, "vac-size", 65536);
	/*
	 * Note: vac-hwflush is spelled with an underscore
	 * on the 4/75s.
	 */
	ci->c_hwflush =
		getpropint(node, "vac_hwflush", 0) |
		getpropint(node, "vac-hwflush", 0);

	ci->c_linesize = l = getpropint(node, "vac-linesize", 16);
	for (i = 0; (1 << i) < l; i++)
		/* void */;
	if ((1 << i) != l)
		panic("bad cache line size %d", l);
	ci->c_l2linesize = i;
	ci->c_associativity = 1;
	ci->c_nlines = ci->c_totalsize << i;

	ci->c_vactype = VAC_WRITETHROUGH;

	/*
	 * Machines with "buserr-type" 1 have a bug in the cache
	 * chip that affects traps.  (I wish I knew more about this
	 * mysterious buserr-type variable....)
	 */
	if (getpropint(node, "buserr-type", 0) == 1)
		sc->flags |= CPUFLG_SUN4CACHEBUG;
}
#endif /* SUN4C */

#if defined(solbourne)
struct module_info module_kap = {
	CPUTYP_UNKNOWN,
	VAC_WRITEBACK,
	cpumatch_kap,
	getcacheinfo_kap,
	NULL,
	0,				/* mmu_enable */
	kap_cache_enable,
	0,				/* ncontext is irrelevant here */
	0,
	0,
	kap_cache_flush,
	kap_vcache_flush_page,
	noop_vcache_flush_segment,	/* unused */
	noop_vcache_flush_region,	/* unused */
	kap_vcache_flush_context,
	noop_pcache_flush_line,
	noop_pure_vcache_flush,
	noop_cache_flush_all,
	0
};

void
cpumatch_kap(sc, mp, node)
	struct cpu_softc *sc;
	struct module_info *mp;
	int	node;
{
	extern int timerblurb;

	sc->mmu_npmeg = sc->mmu_ncontext = 0;	/* do not matter for idt */

	/*
	 * Check for the clock speed in the board diagnostic register.
	 * While there, knowing that there are only two possible values,
	 * fill the delay constant.
	 */
	if ((lda(GLU_DIAG, ASI_PHYS_IO) >> 24) & GD_36MHZ) {
		sc->hz = 36000000;
		timerblurb = 14;	/* about 14.40 */
	} else {
		sc->hz = 33000000;
		timerblurb = 13;	/* about 13.20 */
	}

	if (node != 0) {
		sysmodel = getpropint(node, "cpu", 0);
		switch (sysmodel) {
		case SYS_S4000:
			break;
		case SYS_S4100:
			/* XXX do something about the L2 cache */
			break;
		default:
			panic("cpumatch_kap: unrecognized sysmodel %x",
			    sysmodel);
		}
	}
}

void
getcacheinfo_kap(sc, node)
	struct cpu_softc *sc;
	int node;
{
	struct cacheinfo *ci = &sc->cacheinfo;

	/*
	 * The KAP processor has 3KB icache and 2KB dcache.
	 * It is divided in 3 (icache) or 2 (dcache) banks
	 * of 256 lines, each line being 4 bytes.
	 * Both caches are virtually addressed.
	 */

	ci->ic_linesize = 12;
	ci->ic_l2linesize = 3;	/* XXX */
	ci->ic_nlines = DCACHE_LINES;
	ci->ic_associativity = 1;
	ci->ic_totalsize =
	    ci->ic_nlines * ci->ic_linesize * ci->ic_associativity;
	
	ci->dc_enabled = 1;
	ci->dc_linesize = 8;
	ci->dc_l2linesize = 3;
	ci->dc_nlines = DCACHE_LINES;
	ci->dc_associativity = 1;
	ci->dc_totalsize =
	    ci->dc_nlines * ci->dc_linesize * ci->dc_associativity;

	ci->c_totalsize = ci->ic_totalsize + ci->dc_totalsize;
	/* ci->c_enabled */
	ci->c_hwflush = 0;
	ci->c_linesize = 8;	/* min */
	ci->c_l2linesize = 3;	/* min */
	ci->c_nlines = DCACHE_LINES;
	ci->c_physical = 0;
	ci->c_associativity = 1;
	ci->c_split = 1;

	/* no L2 cache (except on 4100 but we don't handle it yet) */
	
	ci->c_vactype = VAC_WRITEBACK;
}
#endif

void
sun4_hotfix(sc)
	struct cpu_softc *sc;
{
	if ((sc->flags & CPUFLG_SUN4CACHEBUG) != 0) {
		kvm_uncache((caddr_t)trapbase, 1);
		snprintf(cpu_hotfix, sizeof cpu_hotfix,
		    "cache chip bug - trap page uncached");
	}

}

#if defined(SUN4M)
void
getcacheinfo_obp(sc, node)
	struct	cpu_softc *sc;
	int	node;
{
	struct cacheinfo *ci = &sc->cacheinfo;
	int i, l;

	if (node == 0)
		/* Bootstrapping */
		return;

	/*
	 * Determine the Sun4m cache organization.
	 */
	ci->c_physical = node_has_property(node, "cache-physical?");

	if (getpropint(node, "ncaches", 1) == 2)
		ci->c_split = 1;
	else
		ci->c_split = 0;

	/* hwflush is used only by sun4/4c code */
	ci->c_hwflush = 0; 

	if (node_has_property(node, "icache-nlines") &&
	    node_has_property(node, "dcache-nlines") &&
	    ci->c_split) {
		/* Harvard architecture: get I and D cache sizes */
		ci->ic_nlines = getpropint(node, "icache-nlines", 0);
		ci->ic_linesize = l =
			getpropint(node, "icache-line-size", 0);
		for (i = 0; (1 << i) < l && l; i++)
			/* void */;
		if ((1 << i) != l && l)
			panic("bad icache line size %d", l);
		ci->ic_l2linesize = i;
		ci->ic_associativity =
			getpropint(node, "icache-associativity", 1);
		ci->ic_totalsize = l * ci->ic_nlines * ci->ic_associativity;
	
		ci->dc_nlines = getpropint(node, "dcache-nlines", 0);
		ci->dc_linesize = l =
			getpropint(node, "dcache-line-size",0);
		for (i = 0; (1 << i) < l && l; i++)
			/* void */;
		if ((1 << i) != l && l)
			panic("bad dcache line size %d", l);
		ci->dc_l2linesize = i;
		ci->dc_associativity =
			getpropint(node, "dcache-associativity", 1);
		ci->dc_totalsize = l * ci->dc_nlines * ci->dc_associativity;

		ci->c_l2linesize = min(ci->ic_l2linesize, ci->dc_l2linesize);
		ci->c_linesize = min(ci->ic_linesize, ci->dc_linesize);
		ci->c_totalsize = ci->ic_totalsize + ci->dc_totalsize;
	} else {
		/* unified I/D cache */
		ci->c_nlines = getpropint(node, "cache-nlines", 128);
		ci->c_linesize = l = 
			getpropint(node, "cache-line-size", 0);
		for (i = 0; (1 << i) < l && l; i++)
			/* void */;
		if ((1 << i) != l && l)
			panic("bad cache line size %d", l);
		ci->c_l2linesize = i;
		ci->c_totalsize = l *
			ci->c_nlines *
			getpropint(node, "cache-associativity", 1);
	}
	
	if (node_has_property(node, "ecache-nlines")) {
		/* we have a L2 "e"xternal cache */
		ci->ec_nlines = getpropint(node, "ecache-nlines", 32768);
		ci->ec_linesize = l = getpropint(node, "ecache-line-size", 0);
		for (i = 0; (1 << i) < l && l; i++)
			/* void */;
		if ((1 << i) != l && l)
			panic("bad ecache line size %d", l);
		ci->ec_l2linesize = i;
		ci->ec_associativity =
			getpropint(node, "ecache-associativity", 1);
		ci->ec_totalsize = l * ci->ec_nlines * ci->ec_associativity;
	}
	if (ci->c_totalsize == 0)
		printf("warning: couldn't identify cache\n");
}

/*
 * We use the max. number of contexts on the micro and
 * hyper SPARCs. The SuperSPARC would let us use up to 65536
 * contexts (by powers of 2), but we keep it at 4096 since
 * the table must be aligned to #context*4. With 4K contexts,
 * we waste at most 16K of memory. Note that the context
 * table is *always* page-aligned, so there can always be
 * 1024 contexts without sacrificing memory space (given
 * that the chip supports 1024 contexts).
 *
 * Currently known limits: MS1=64, MS2=256, HS=4096, SS=65536
 * 	some old SS's=4096
 */

/* TI Microsparc I */
struct module_info module_ms1 = {
	CPUTYP_MS1,
	VAC_NONE,
	cpumatch_ms,
	getcacheinfo_obp,
	0,
	ms1_mmu_enable,
	ms1_cache_enable,
	64,
	ms1_get_syncflt,
	no_asyncflt_regs,
	ms1_cache_flush,
	noop_vcache_flush_page,
	noop_vcache_flush_segment,
	noop_vcache_flush_region,
	noop_vcache_flush_context,
	noop_pcache_flush_line,
	noop_pure_vcache_flush,
	ms1_cache_flush_all,
	memerr4m
};

void
ms1_mmu_enable()
{
}

/* TI Microsparc II */
struct module_info module_ms2 = {
	CPUTYP_MS2,
	VAC_WRITETHROUGH,
	cpumatch_ms,
	getcacheinfo_obp,
	0, /* was swift_hotfix, */
	0,
	swift_cache_enable,
	256,
	srmmu_get_syncflt,
	srmmu_get_asyncflt,
	srmmu_cache_flush,
	srmmu_vcache_flush_page,
	srmmu_vcache_flush_segment,
	srmmu_vcache_flush_region,
	srmmu_vcache_flush_context,
	noop_pcache_flush_line,
	noop_pure_vcache_flush,
	srmmu_cache_flush_all,
	memerr4m
};


struct module_info module_swift = {
	CPUTYP_MS2,
	VAC_WRITETHROUGH,
	cpumatch_ms,
	getcacheinfo_obp,
	swift_hotfix,
	0,
	swift_cache_enable,
	256,
	swift_get_syncflt,
	no_asyncflt_regs,
	srmmu_cache_flush,
	srmmu_vcache_flush_page,
	srmmu_vcache_flush_segment,
	srmmu_vcache_flush_region,
	srmmu_vcache_flush_context,
	srmmu_pcache_flush_line,
	noop_pure_vcache_flush,
	srmmu_cache_flush_all,
	memerr4m
};

void
cpumatch_ms(sc, mp, node)
	struct cpu_softc *sc;
	struct module_info *mp;
	int	node;
{
	replacemul();
}

void
swift_hotfix(sc)
	struct cpu_softc *sc;
{
	int pcr = lda(SRMMU_PCR, ASI_SRMMU);

	/* Turn on branch prediction */
	pcr |= SWIFT_PCR_BF;
	sta(SRMMU_PCR, ASI_SRMMU, pcr);
}

void
swift_mmu_enable()
{
}

struct module_info module_viking = {
	CPUTYP_UNKNOWN,		/* set in cpumatch() */
	VAC_NONE,
	cpumatch_viking,
	getcacheinfo_obp,
	viking_hotfix,
	viking_mmu_enable,
	viking_cache_enable,
	4096,
	viking_get_syncflt,
	no_asyncflt_regs,
	/* supersparcs use cached DVMA, no need to flush */
	noop_cache_flush,
	noop_vcache_flush_page,
	noop_vcache_flush_segment,
	noop_vcache_flush_region,
	noop_vcache_flush_context,
	viking_pcache_flush_line,
	noop_pure_vcache_flush,
	noop_cache_flush_all,
	viking_memerr
};

void
cpumatch_viking(sc, mp, node)
	struct cpu_softc *sc;
	struct module_info *mp;
	int	node;
{
	replacemul();

	if (node == 0)
		viking_hotfix(sc);
}

void
viking_hotfix(sc)
	struct cpu_softc *sc;
{
	int pcr = lda(SRMMU_PCR, ASI_SRMMU);

	/* Test if we're directly on the MBus */
	if ((pcr & VIKING_PCR_MB) == 0) {
		sc->mxcc = 1;
		sc->flags |= CPUFLG_CACHE_MANDATORY;
		/*
		 * Ok to cache PTEs; set the flag here, so we don't
		 * uncache in pmap_bootstrap().
		 */
		if ((pcr & VIKING_PCR_TC) == 0)
			printf("[viking: PCR_TC is off]");
		else
			sc->flags |= CPUFLG_CACHEPAGETABLES;
	} else {
		sc->cache_flush = viking_cache_flush;
		sc->pcache_flush_line = viking_pcache_flush_line;
	}

	/* XXX! */
	if (sc->mxcc)
		sc->cpu_type = CPUTYP_SS1_MBUS_MXCC;
	else
		sc->cpu_type = CPUTYP_SS1_MBUS_NOMXCC;
}

void
viking_mmu_enable()
{
	int pcr;

	pcr = lda(SRMMU_PCR, ASI_SRMMU);

	if (cpuinfo.mxcc) {
		if ((pcr & VIKING_PCR_TC) == 0) {
			printf("[viking: turn on PCR_TC]");
		}
		pcr |= VIKING_PCR_TC;
	} else
		pcr &= ~VIKING_PCR_TC;
	sta(SRMMU_PCR, ASI_SRMMU, pcr);
}


/* ROSS Hypersparc */
struct module_info module_hypersparc = {
	CPUTYP_UNKNOWN,
	VAC_WRITEBACK,
	cpumatch_hypersparc,
	getcacheinfo_obp,
	0,
	hypersparc_mmu_enable,
	hypersparc_cache_enable,
	4096,
	hypersparc_get_syncflt,
	hypersparc_get_asyncflt,
	srmmu_cache_flush,
	srmmu_vcache_flush_page,
	srmmu_vcache_flush_segment,
	srmmu_vcache_flush_region,
	srmmu_vcache_flush_context,
	srmmu_pcache_flush_line,
	hypersparc_pure_vcache_flush,
	hypersparc_cache_flush_all,
	hypersparc_memerr
};

void
cpumatch_hypersparc(sc, mp, node)
	struct cpu_softc *sc;
	struct module_info *mp;
	int	node;
{
	sc->cpu_type = CPUTYP_HS_MBUS;/*XXX*/

	if (node == 0)
		sta(0, ASI_HICACHECLR, 0);

	replacemul();
}

void
hypersparc_mmu_enable()
{
#if 0
	int pcr;

	pcr = lda(SRMMU_PCR, ASI_SRMMU);
	pcr |= HYPERSPARC_PCR_C;
	pcr &= ~HYPERSPARC_PCR_CE;

	sta(SRMMU_PCR, ASI_SRMMU, pcr);
#endif
}

/* Cypress 605 */
struct module_info module_cypress = {
	CPUTYP_CYPRESS,
	VAC_WRITEBACK,
	0,
	getcacheinfo_obp,
	0,
	0,
	cypress_cache_enable,
	4096,
	cypress_get_syncflt,
	cypress_get_asyncflt,
	srmmu_cache_flush,
	srmmu_vcache_flush_page,
	srmmu_vcache_flush_segment,
	srmmu_vcache_flush_region,
	srmmu_vcache_flush_context,
	srmmu_pcache_flush_line,
	noop_pure_vcache_flush,
	cypress_cache_flush_all,
	memerr4m
};

/* Fujitsu Turbosparc */
struct module_info module_turbosparc = {	/* UNTESTED */
	CPUTYP_MS2,
	VAC_WRITEBACK,
	cpumatch_turbosparc,
	getcacheinfo_obp,
	turbosparc_hotfix,
	0,
	turbosparc_cache_enable,
	256,
	turbosparc_get_syncflt,
	no_asyncflt_regs,
	srmmu_cache_flush,
	srmmu_vcache_flush_page,
	srmmu_vcache_flush_segment,
	srmmu_vcache_flush_region,
	srmmu_vcache_flush_context,
	srmmu_pcache_flush_line,
	noop_pure_vcache_flush,
	srmmu_cache_flush_all,
	memerr4m
};

void
cpumatch_turbosparc(sc, mp, node)
	struct cpu_softc *sc;
	struct module_info *mp;
	int	node;
{
	int i;

	if (node == 0 || sc->master == 0)
		return;

	i = getpsr();
	if (sc->cpu_vers == IU_VERS(i))
		return;

	/*
	 * A cloaked Turbosparc: clear any items in cpuinfo that
	 * might have been set to uS2 versions during bootstrap.
	 */
	sc->cpu_name = 0;
	sc->mmu_ncontext = 0;
	sc->cpu_type = 0;
	sc->cacheinfo.c_vactype = 0;
	sc->hotfix = 0;
	sc->mmu_enable = 0;
	sc->cache_enable = 0;
	sc->get_syncflt = 0;
	sc->cache_flush = 0;
	sc->vcache_flush_page = 0;
	sc->vcache_flush_segment = 0;
	sc->vcache_flush_region = 0;
	sc->vcache_flush_context = 0;
	sc->pcache_flush_line = 0;

	replacemul();
}

void
turbosparc_hotfix(sc)
	struct cpu_softc *sc;
{
	int pcf;

	pcf = lda(SRMMU_PCFG, ASI_SRMMU);
	if (pcf & TURBOSPARC_PCFG_US2) {
		/* Turn off uS2 emulation bit */
		pcf &= ~TURBOSPARC_PCFG_US2;
		sta(SRMMU_PCFG, ASI_SRMMU, pcf);
	}
}
#endif /* SUN4M */


#define	ANY	-1	/* match any version */

struct cpu_conf {
	int	arch;
	int	cpu_impl;
	int	cpu_vers;
	int	mmu_impl;
	int	mmu_vers;
	char	*name;
	struct	module_info *minfo;
} cpu_conf[] = {
#if defined(SUN4)
	{ CPU_SUN4, 0, 0, ANY, ANY, "MB86900/1A or L64801", &module_sun4 },
	{ CPU_SUN4, 1, 0, ANY, ANY, "L64811", &module_sun4 },
	{ CPU_SUN4, 1, 1, ANY, ANY, "CY7C601", &module_sun4 },
#endif

#if defined(SUN4C)
	{ CPU_SUN4C, 0, 0, ANY, ANY, "MB86900/1A or L64801", &module_sun4c },
	{ CPU_SUN4C, 1, 0, ANY, ANY, "L64811", &module_sun4c },
	{ CPU_SUN4C, 1, 1, ANY, ANY, "CY7C601", &module_sun4c },
	{ CPU_SUN4C, 9, 0, ANY, ANY, "W8601/8701 or MB86903", &module_sun4c },
#endif

#if defined(SUN4M)
	{ CPU_SUN4M, 0, 4, 0, 4, "MB86904", &module_swift },
	{ CPU_SUN4M, 0, 5, 0, 5, "MB86907", &module_turbosparc },
	{ CPU_SUN4M, 1, 1, 1, 0, "CY7C601/604", &module_cypress },
	{ CPU_SUN4M, 1, 1, 1, 0xb, "CY7C601/605 (v.b)", &module_cypress },
	{ CPU_SUN4M, 1, 1, 1, 0xc, "CY7C601/605 (v.c)", &module_cypress },
	{ CPU_SUN4M, 1, 1, 1, 0xf, "CY7C601/605 (v.f)", &module_cypress },
	{ CPU_SUN4M, 1, 3, 1, ANY, "CY7C611", &module_cypress },
	{ CPU_SUN4M, 1, 0xe, 1, 7, "RT620/625", &module_hypersparc },
	{ CPU_SUN4M, 1, 0xf, 1, 7, "RT620/625", &module_hypersparc },
	{ CPU_SUN4M, 4, 0, 0, ANY, "TMS390Z50 v0 or TMS390Z55", &module_viking },
	{ CPU_SUN4M, 4, 1, 0, ANY, "TMS390Z50 v1", &module_viking },
	{ CPU_SUN4M, 4, 1, 4, ANY, "TMS390S10", &module_ms1 },
	{ CPU_SUN4M, 4, 2, 0, ANY, "TI_MS2", &module_ms2 },
	{ CPU_SUN4M, 4, 3, ANY, ANY, "TI_4_3", &module_viking },
	{ CPU_SUN4M, 4, 4, ANY, ANY, "TI_4_4", &module_viking },
#endif

#if defined(solbourne)
	{ CPU_KAP, 5, 0, ANY, ANY, "KAP", &module_kap },
#endif

	{ ANY, ANY, ANY, ANY, ANY, "Unknown", &module_unknown }
};

void
getcpuinfo(sc, node)
	struct cpu_softc *sc;
	int	node;
{
	struct cpu_conf *mp;
	int i;
	int cpu_impl, cpu_vers;
	int mmu_impl, mmu_vers;

	/*
	 * Set up main criteria for selection from the CPU configuration
	 * table: the CPU implementation/version fields from the PSR
	 * register, and -- on sun4m machines -- the MMU
	 * implementation/version from the SCR register.
	 */
	if (sc->master) {
		i = getpsr();
		if (node == 0 ||
		    (cpu_impl =
		     getpropint(node, "psr-implementation", -1)) == -1)
			cpu_impl = IU_IMPL(i);

		if (node == 0 ||
		    (cpu_vers = getpropint(node, "psr-version", -1)) == -1)
			cpu_vers = IU_VERS(i);

		if (CPU_ISSUN4M) {
			i = lda(SRMMU_PCR, ASI_SRMMU);
			if (node == 0 ||
			    (mmu_impl =
			     getpropint(node, "implementation", -1)) == -1)
				mmu_impl = SRMMU_IMPL(i);

			if (node == 0 ||
			    (mmu_vers = getpropint(node, "version", -1)) == -1)
				mmu_vers = SRMMU_VERS(i);
		} else {
			mmu_impl = ANY;
			mmu_vers = ANY;
		}
	} else {
		/*
		 * Get CPU version/implementation from ROM. If not
		 * available, assume same as boot CPU.
		 */
		cpu_impl = getpropint(node, "psr-implementation", -1);
		if (cpu_impl == -1)
			cpu_impl = cpuinfo.cpu_impl;
		cpu_vers = getpropint(node, "psr-version", -1);
		if (cpu_vers == -1)
			cpu_vers = cpuinfo.cpu_vers;

		/* Get MMU version/implementation from ROM always */
		mmu_impl = getpropint(node, "implementation", -1);
		mmu_vers = getpropint(node, "version", -1);
	}

	for (mp = cpu_conf; ; mp++) {
		if (mp->arch != cputyp && mp->arch != ANY)
			continue;

#define MATCH(x)	(mp->x == x || mp->x == ANY)
		if (!MATCH(cpu_impl) ||
		    !MATCH(cpu_vers) ||
		    !MATCH(mmu_impl) ||
		    !MATCH(mmu_vers))
			continue;
#undef MATCH

		/*
		 * Got CPU type.
		 */
		sc->cpu_impl = cpu_impl;
		sc->cpu_vers = cpu_vers;
		sc->mmu_impl = mmu_impl;
		sc->mmu_vers = mmu_vers;

		if (mp->minfo->cpu_match) {
			/* Additional fixups */
			mp->minfo->cpu_match(sc, mp->minfo, node);
		}
		if (sc->cpu_name == 0)
			sc->cpu_name = mp->name;

		if (sc->mmu_ncontext == 0)
			sc->mmu_ncontext = mp->minfo->ncontext;

		if (sc->cpu_type == 0)
			sc->cpu_type = mp->minfo->cpu_type;

		if (sc->cacheinfo.c_vactype == VAC_UNKNOWN)
			sc->cacheinfo.c_vactype = mp->minfo->vactype;

		mp->minfo->getcacheinfo(sc, node);

		if (node && sc->hz == 0 && !CPU_ISSUN4/*XXX*/) {
			sc->hz = getpropint(node, "clock-frequency", 0);
			if (sc->hz == 0) {
				/*
				 * Try to find it in the OpenPROM root...
				 */     
				sc->hz = getpropint(findroot(),
						    "clock-frequency", 0);
			}
		}

		/*
		 * Copy CPU/MMU/Cache specific routines into cpu_softc.
		 */
#define MPCOPY(x)	if (sc->x == 0) sc->x = mp->minfo->x;
		MPCOPY(hotfix);
		MPCOPY(mmu_enable);
		MPCOPY(cache_enable);
		MPCOPY(get_syncflt);
		MPCOPY(get_asyncflt);
		MPCOPY(cache_flush);
		MPCOPY(vcache_flush_page);
		MPCOPY(vcache_flush_segment);
		MPCOPY(vcache_flush_region);
		MPCOPY(vcache_flush_context);
		MPCOPY(pcache_flush_line);
		MPCOPY(pure_vcache_flush);
		MPCOPY(cache_flush_all);
		MPCOPY(memerr);
#undef MPCOPY
		return;
	}
	panic("Out of CPUs");
}

/*
 * The following tables convert <IU impl, IU version, FPU version> triples
 * into names for the CPU and FPU chip.  In most cases we do not need to
 * inspect the FPU version to name the IU chip, but there is one exception
 * (for Tsunami), and this makes the tables the same.
 *
 * The table contents (and much of the structure here) are from Guy Harris.
 *
 */
struct info {
	int	valid;
	int	iu_impl;
	int	iu_vers;
	int	fpu_vers;
	char	*name;
};

/* NB: table order matters here; specific numbers must appear before ANY. */
static struct info fpu_types[] = {
	/*
	 * Vendor 0, IU Fujitsu0.
	 */
	{ 1, 0x0, ANY, 0, "MB86910 or WTL1164/5" },
	{ 1, 0x0, ANY, 1, "MB86911 or WTL1164/5" },
	{ 1, 0x0, ANY, 2, "L64802 or ACT8847" },
	{ 1, 0x0, ANY, 3, "WTL3170/2" },
	{ 1, 0x0, 4,   4, "on-chip" },		/* Swift */
	{ 1, 0x0, 5,   5, "on-chip" },		/* TurboSparc */
	{ 1, 0x0, ANY, 4, "L64804" },

	/*
	 * Vendor 1, IU ROSS0/1 or Pinnacle.
	 */
	{ 1, 0x1, 0xf, 0, "on-chip" },		/* Pinnacle */
	{ 1, 0x1, 0xe, 0, "on-chip" },		/* Hypersparc RT 625/626 */
	{ 1, 0x1, ANY, 0, "L64812 or ACT8847" },
	{ 1, 0x1, ANY, 1, "L64814" },
	{ 1, 0x1, ANY, 2, "TMS390C602A" },
	{ 1, 0x1, ANY, 3, "RT602 or WTL3171" },

	/*
	 * Vendor 2, IU BIT0.
	 */
	{ 1, 0x2, ANY, 0, "B5010 or B5110/20 or B5210" },

	/*
	 * Vendor 4, Texas Instruments.
	 */
	{ 1, 0x4, ANY, 0, "on-chip" },		/* Viking */
	{ 1, 0x4, ANY, 4, "on-chip" },		/* Tsunami */

	/*
	 * Vendor 5, IU Matsushita0.
	 */
	{ 1, 0x5, ANY, 0, "on-chip" },

	/*
	 * Vendor 9, Weitek.
	 */
	{ 1, 0x9, ANY, 3, "on-chip" },

	{ 0 }
};

char *
fsrtoname(impl, vers, fver, buf, buflen)
	register int impl, vers, fver;
	char *buf;
	size_t buflen;
{
	register struct info *p;

	for (p = fpu_types; p->valid; p++)
		if (p->iu_impl == impl &&
		    (p->iu_vers == vers || p->iu_vers == ANY) &&
		    (p->fpu_vers == fver))
			return (p->name);
	snprintf(buf, buflen, "version 0x%x", fver);
	return (buf);
}

/*
 * Whack the slow sun4/sun4c {,u}{mul,div,rem} functions with
 * fast V8 ones
 * We are called once before pmap_bootstrap and once after. We only do stuff
 * in the "before" case. We happen to know that the kernel text is not
 * write-protected then.
 * XXX - investigate cache flushing, right now we can't do it because the
 *       flushes try to do va -> pa conversions.
 */
extern int _mulreplace, _mulreplace_end, _mul;
extern int _umulreplace, _umulreplace_end, _umul;
extern int _divreplace, _divreplace_end, _div;
extern int _udivreplace, _udivreplace_end, _udiv;
extern int _remreplace, _remreplace_end, _rem;
extern int _uremreplace, _uremreplace_end, _urem;
int	v8mul;	/* flag whether cpu has hardware mul, div, and rem */

struct replace {
	void *from, *frome, *to;
} ireplace[] = {
	{ &_mulreplace, &_mulreplace_end, &_mul },
	{ &_umulreplace, &_umulreplace_end, &_umul },
	{ &_divreplace, &_divreplace_end, &_div },
	{ &_udivreplace, &_udivreplace_end, &_udiv },
	{ &_remreplace, &_remreplace_end, &_rem },
 	{ &_uremreplace, &_uremreplace_end, &_urem },
};

void
replacemul()
{
	static int replacedone = 0;
	int i, s;

	if (replacedone)
		return;
	replacedone = 1;

	s = splhigh();
	for (i = 0; i < sizeof(ireplace)/sizeof(ireplace[0]); i++)
		bcopy(ireplace[i].from, ireplace[i].to,
		    ireplace[i].frome - ireplace[i].from);
	splx(s);
	v8mul = 1;
}
