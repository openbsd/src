/*	$OpenBSD: cpu.c,v 1.11 2003/02/12 06:33:00 jason Exp $	*/
/*	$NetBSD: cpu.c,v 1.13 2001/05/26 21:27:15 chs Exp $ */

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
#include <machine/trap.h>
#include <machine/pmap.h>

#include <sparc64/sparc64/cache.h>

/* This is declared here so that you must include a CPU for the cache code. */
struct cacheinfo cacheinfo;

/* Our exported CPU info; we have only one for now. */  
struct cpu_info cpu_info_store;

/* Linked list of all CPUs in system. */
struct cpu_info *cpus = NULL;

/* The following are used externally (sysctl_hw). */
char	machine[] = MACHINE;		/* from <machine/param.h> */
char	machine_arch[] = MACHINE_ARCH;	/* from <machine/param.h> */
char	cpu_model[100];

struct	proc *fpproc;
int	foundfpu;
int	want_ast, want_resched;

/* The CPU configuration driver. */
static void cpu_attach(struct device *, struct device *, void *);
int  cpu_match(struct device *, void *, void *);

struct cfattach cpu_ca = {
	sizeof(struct device), cpu_match, cpu_attach
};

extern struct cfdriver cpu_cd;

static char *fsrtoname(int, int, int, char *);

#define	IU_IMPL(v)	((((u_int64_t)(v))&VER_IMPL) >> VER_IMPL_SHIFT)
#define	IU_VERS(v)	((((u_int64_t)(v))&VER_MASK) >> VER_MASK_SHIFT)

int
cpu_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;
	struct cfdata *cf = (struct cfdata *)vcf;

	return (strcmp(cf->cf_driver->cd_name, ma->ma_name) == 0);
}

/*
 * Attach the CPU.
 * Discover interesting goop about the virtual address cache
 * (slightly funny place to do it, but this is where it is to be found).
 */
static void
cpu_attach(parent, dev, aux)
	struct device *parent;
	struct device *dev;
	void *aux;
{
	int node;
	long clk;
	int impl, vers, fver;
	char *fpuname;
	struct mainbus_attach_args *ma = aux;
	struct fpstate64 *fpstate;
	struct fpstate64 fps[2];
	char *sep;
	char fpbuf[40];
	register int i, l;
	u_int64_t ver;
	extern u_int64_t cpu_clockrate[];

	/* This needs to be 64-bit aligned */
	fpstate = ALIGNFPSTATE(&fps[1]);
	/*
	 * Get the FSR and clear any exceptions.  If we do not unload
	 * the queue here and it is left over from a previous crash, we
	 * will panic in the first loadfpstate(), due to a sequence error,
	 * so we need to dump the whole state anyway.
	 *
	 * If there is no FPU, trap.c will advance over all the stores,
	 * so we initialize fs_fsr here.
	 */
	fpstate->fs_fsr = 7 << FSR_VER_SHIFT;	/* 7 is reserved for "none" */
	savefpstate(fpstate);
	fver = (fpstate->fs_fsr >> FSR_VER_SHIFT) & (FSR_VER >> FSR_VER_SHIFT);
	ver = getver();
	impl = IU_IMPL(ver);
	vers = IU_VERS(ver);
	if (fver != 7) {
		foundfpu = 1;
		fpuname = fsrtoname(impl, vers, fver, fpbuf);
	} else
		fpuname = "no";

	/* tell them what we have */
	node = ma->ma_node;

	clk = getpropint(node, "clock-frequency", 0);
	if (clk == 0) {
		/*
		 * Try to find it in the OpenPROM root...
		 */
		clk = getpropint(findroot(), "clock-frequency", 0);
	}
	if (clk) {
		cpu_clockrate[0] = clk; /* Tell OS what frequency we run on */
		cpu_clockrate[1] = clk/1000000;
	}
	sprintf(cpu_model, "%s @ %s MHz, %s FPU",
		getpropstring(node, "name"),
		clockfreq(clk), fpuname);
	printf(": %s\n", cpu_model);

	cacheinfo.c_physical = 1; /* Dunno... */
	cacheinfo.c_split = 1;
	cacheinfo.ic_linesize = l = getpropint(node, "icache-line-size", 0);
	for (i = 0; (1 << i) < l && l; i++)
		/* void */;
	if ((1 << i) != l && l)
		panic("bad icache line size %d", l);
	cacheinfo.ic_l2linesize = i;
	cacheinfo.ic_totalsize =
	    getpropint(node, "icache-size", 0) *
	    getpropint(node, "icache-associativity", 1);
	if (cacheinfo.ic_totalsize == 0)
		cacheinfo.ic_totalsize = l *
		    getpropint(node, "icache-nlines", 64) *
		    getpropint(node, "icache-associativity", 1);

	cacheinfo.dc_linesize = l =
		getpropint(node, "dcache-line-size",0);
	for (i = 0; (1 << i) < l && l; i++)
		/* void */;
	if ((1 << i) != l && l)
		panic("bad dcache line size %d", l);
	cacheinfo.dc_l2linesize = i;
	cacheinfo.dc_totalsize =
	    getpropint(node, "dcache-size", 0) *
	    getpropint(node, "dcache-associativity", 1);
	if (cacheinfo.dc_totalsize == 0)
		cacheinfo.dc_totalsize = l *
		    getpropint(node, "dcache-nlines", 128) *
		    getpropint(node, "dcache-associativity", 1);
	
	cacheinfo.ec_linesize = l =
		getpropint(node, "ecache-line-size", 0);
	for (i = 0; (1 << i) < l && l; i++)
		/* void */;
	if ((1 << i) != l && l)
		panic("bad ecache line size %d", l);
	cacheinfo.ec_l2linesize = i;
	cacheinfo.ec_totalsize =
		getpropint(node, "ecache-size", 0) *
		getpropint(node, "ecache-associativity", 1);
	if (cacheinfo.ec_totalsize == 0)
		cacheinfo.ec_totalsize = l *
		    getpropint(node, "ecache-nlines", 32768) *
		    getpropint(node, "ecache-associativity", 1);
	
	/*
	 * XXX - The following will have to do until
	 * we have per-cpu cache handling.
	 */
	cacheinfo.c_l2linesize =
		min(cacheinfo.ic_l2linesize,
		    cacheinfo.dc_l2linesize);
	cacheinfo.c_linesize =
		min(cacheinfo.ic_linesize,
		    cacheinfo.dc_linesize);
	cacheinfo.c_totalsize =
		cacheinfo.ic_totalsize +
		cacheinfo.dc_totalsize;

	if (cacheinfo.c_totalsize == 0)
		return;
	
	sep = " ";
	printf("%s: physical", dev->dv_xname);
	if (cacheinfo.ic_totalsize > 0) {
		printf("%s%ldK instruction (%ld b/l)", sep,
		       (long)cacheinfo.ic_totalsize/1024,
		       (long)cacheinfo.ic_linesize);
		sep = ", ";
	}
	if (cacheinfo.dc_totalsize > 0) {
		printf("%s%ldK data (%ld b/l)", sep,
		       (long)cacheinfo.dc_totalsize/1024,
		       (long)cacheinfo.dc_linesize);
		sep = ", ";
	}
	if (cacheinfo.ec_totalsize > 0) {
		printf("%s%ldK external (%ld b/l)", sep,
		       (long)cacheinfo.ec_totalsize/1024,
		       (long)cacheinfo.ec_linesize);
	}
	printf("\n");
	cache_enable();
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
	u_char	valid;
	u_char	iu_impl;
	u_char	iu_vers;
	u_char	fpu_vers;
	char	*name;
};

#define	ANY	0xff	/* match any FPU version (or, later, IU version) */


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
	{ 1, 0x0, ANY, 4, "L64804" },

	/*
	 * Vendor 1, IU ROSS0/1 or Pinnacle.
	 */
	{ 1, 0x1, 0xf, 0, "on-chip" },		/* Pinnacle */
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

static char *
fsrtoname(impl, vers, fver, buf)
	register int impl, vers, fver;
	char *buf;
{
	register struct info *p;

	for (p = fpu_types; p->valid; p++)
		if (p->iu_impl == impl &&
		    (p->iu_vers == vers || p->iu_vers == ANY) &&
		    (p->fpu_vers == fver))
			return (p->name);
	sprintf(buf, "version %x", fver);
	return (buf);
}

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};
