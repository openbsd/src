/*	$OpenBSD: kvm_sparc.c,v 1.2 1996/05/05 14:57:50 deraadt Exp $ */
/*	$NetBSD: kvm_sparc.c,v 1.9 1996/04/01 19:23:03 cgd Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
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

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm_sparc.c	8.1 (Berkeley) 6/4/93";
#else
static char *rcsid = "$OpenBSD: kvm_sparc.c,v 1.2 1996/05/05 14:57:50 deraadt Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * Sparc machine dependent routines for kvm.  Hopefully, the forthcoming 
 * vm code will one day obsolete this module.
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/device.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <machine/autoconf.h>

#include <limits.h>
#include <db.h>

#include "kvm_private.h"

#define MA_SIZE 32 /* XXX */
struct vmstate {
	struct {
		int x_seginval;		/* [sun4/sun4c] only */
		int x_npmemarr;
		struct memarr x_pmemarr[MA_SIZE];
		struct segmap x_segmap_store[NKREG*NSEGRG];
	} x;
#define seginval x.x_seginval
#define npmemarr x.x_npmemarr
#define pmemarr x.x_pmemarr
#define segmap_store x.x_segmap_store
	int *pte;			/* [sun4/sun4c] only */
};
#define NPMEG(vm) ((vm)->seginval+1)

static int cputyp = -1;

static int pgshift, nptesg;

#define VA_VPG(va)	((cputyp == CPU_SUN4C || cputyp == CPU_SUN4M) \
				? VA_SUN4C_VPG(va) \
				: VA_SUN4_VPG(va))

static int	_kvm_mustinit __P((kvm_t *));

#if 0
static int
getcputyp()
{
	int mib[2];
	size_t size;

	mib[0] = CTL_HW;
	mib[1] = HW_CLASS;
	size = sizeof cputyp;
	if (sysctl(mib, 2, &cputyp, &size, NULL, 0) == -1)
		return (-1);
}
#endif

static int
_kvm_mustinit(kd)
	kvm_t *kd;
{
static	struct nlist nlist[2] = {
#		define X_CPUTYP	0
		{ "_cputyp" },
		{ NULL },
	};
	off_t foff;

	if (cputyp != -1)
		return 0;

	for (pgshift = 12; (1 << pgshift) != kd->nbpg; pgshift++)
		;
	nptesg = NBPSG / kd->nbpg;

	if (kvm_nlist(kd, nlist) != 0) {
		_kvm_err(kd, kd->program, "cannot find `cputyp' symbol");
		return (-1);
	}
	/* Assume kernel mappings are all within first memory bank. */
	foff = nlist[X_CPUTYP].n_value - KERNBASE;
	if (lseek(kd->pmfd, foff, 0) == -1 || 
	    read(kd->pmfd, &cputyp, sizeof(cputyp)) < 0) {
		_kvm_err(kd, kd->program, "cannot read `cputyp");
		return (-1);
	}
	if (cputyp != CPU_SUN4 &&
	    cputyp != CPU_SUN4C &&
	    cputyp != CPU_SUN4M)
		return (-1);

	return (0);
}

void
_kvm_freevtop(kd)
	kvm_t *kd;
{
	if (kd->vmst != 0) {
		if (kd->vmst->pte != 0)
			free(kd->vmst->pte);
		free(kd->vmst);
		kd->vmst  = 0;
	}
}

/*
 * Translate a kernel virtual address to a physical address using the
 * mapping information in kd->vm.  Returns the result in pa, and returns
 * the number of bytes that are contiguously available from this 
 * physical address.  This routine is used only for crashdumps.
 */
int
_kvm_kvatop(kd, va, pa)
	kvm_t *kd;
	u_long va;
	u_long *pa;
{
	if (_kvm_mustinit(kd) != 0)
		return (-1);

	return ((cputyp == CPU_SUN4M)
		? _kvm_kvatop4m(kd, va, pa)
		: _kvm_kvatop44c(kd, va, pa));
}

/*
 * Prepare for translation of kernel virtual addresses into offsets
 * into crash dump files. We use the MMU specific goop written at the
 * and of crash dump by pmap_dumpmmu().
 * (note: sun4/sun4c 2-level MMU specific)
 */
int
_kvm_initvtop(kd)
	kvm_t *kd;
{
	if (_kvm_mustinit(kd) != 0)
		return (-1);

	return ((cputyp == CPU_SUN4M)
		? _kvm_initvtop4m(kd)
		: _kvm_initvtop44c(kd));
}

#define VA_OFF(va) (va & (kd->nbpg - 1))


/*
 * We use the MMU specific goop written at the end of crash dump
 * by pmap_dumpmmu().
 * (note: sun4 3-level MMU not yet supported)
 */
int
_kvm_initvtop44c(kd)
	kvm_t *kd;
{
	register struct vmstate *vm;
	register int i;
	off_t foff;
	struct stat st;

	if ((vm = kd->vmst) == 0) {
		kd->vmst = vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
		if (vm == 0)
			return (-1);
	}

	if (fstat(kd->pmfd, &st) < 0)
		return (-1);
	/*
	 * Read segment table.
	 */

	foff = st.st_size - roundup(sizeof(vm->x), kd->nbpg);
	errno = 0;
	if (lseek(kd->pmfd, (off_t)foff, 0) == -1 && errno != 0 || 
	    read(kd->pmfd, (char *)&vm->x, sizeof(vm->x)) < 0) {
		_kvm_err(kd, kd->program, "cannot read segment map");
		return (-1);
	}

	vm->pte = (int *)_kvm_malloc(kd, NPMEG(vm) * nptesg * sizeof(int));
	if (vm->pte == 0) {
		free(kd->vmst);
		kd->vmst = 0;
		return (-1);
	}

	/*
	 * Read PMEGs.
	 */
	foff = st.st_size - roundup(sizeof(vm->x), kd->nbpg) -
	      roundup(NPMEG(vm) * nptesg * sizeof(int), kd->nbpg);

	errno = 0;
	if (lseek(kd->pmfd, foff, 0) == -1 && errno != 0 || 
	    read(kd->pmfd, (char *)vm->pte, NPMEG(vm) * nptesg * sizeof(int)) < 0) {
		_kvm_err(kd, kd->program, "cannot read PMEG table");
		return (-1);
	}

	return (0);
}

int
_kvm_kvatop44c(kd, va, pa)
	kvm_t *kd;
	u_long va;
	u_long *pa;
{
	register int vr, vs, pte, off, nmem;
	register struct vmstate *vm = kd->vmst;
	struct regmap *rp;
	struct segmap *sp;
	struct memarr *mp;

	if (va < KERNBASE)
		goto err;

	vr = VA_VREG(va);
	vs = VA_VSEG(va);

	sp = &vm->segmap_store[(vr-NUREG)*NSEGRG + vs];
	if (sp->sg_npte == 0)
		goto err;
	if (sp->sg_pmeg == vm->seginval)
		goto err;
	pte = vm->pte[sp->sg_pmeg * nptesg + VA_VPG(va)];
	if ((pte & PG_V) != 0) {
		register long p, dumpoff = 0;

		off = VA_OFF(va);
		p = (pte & PG_PFNUM) << pgshift;
		/* Translate (sparse) pfnum to (packed) dump offset */
		for (mp = vm->pmemarr, nmem = vm->npmemarr; --nmem >= 0; mp++) {
			if (mp->addr <= p && p < mp->addr + mp->len)
				break;
			dumpoff += mp->len;
		}
		if (nmem < 0)
			goto err;
		*pa = (dumpoff + p - mp->addr) | off;
		return (kd->nbpg - off);
	}
err:
	_kvm_err(kd, 0, "invalid address (%x)", va);
	return (0);
}

/*
 * Prepare for translation of kernel virtual addresses into offsets
 * into crash dump files. Since the sun4m pagetables are all in memory,
 * we use nlist to bootstrap the translation tables. This assumes that
 * the kernel mappings all reside in the first physical memory bank.
 */
int
_kvm_initvtop4m(kd)
	kvm_t *kd;
{
	register int i;
	register off_t foff;
	register struct vmstate *vm;
	struct stat st;
static	struct nlist nlist[4] = {
#		define X_KSEGSTORE	0
		{ "_kernel_segmap_store" },
#		define X_PMEMARR	1
		{ "_pmemarr" },
#		define X_NPMEMARR	2
		{ "_npmemarr" },
		{ NULL },
	};

	if ((vm = kd->vmst) == 0) {
		kd->vmst = vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
		if (vm == 0)
			return (-1);
	}

	if (kvm_nlist(kd, nlist) != 0) {
		_kvm_err(kd, kd->program, "cannot read symbols");
		return (-1);
	}

	/* Assume kernel mappings are all within first memory bank. */
	foff = nlist[X_KSEGSTORE].n_value - KERNBASE;
	if (lseek(kd->pmfd, foff, 0) == -1 || 
	    read(kd->pmfd, vm->segmap_store, sizeof(vm->segmap_store)) < 0) {
		_kvm_err(kd, kd->program, "cannot read segment map");
		return (-1);
	}

	foff = nlist[X_PMEMARR].n_value - KERNBASE;
	if (lseek(kd->pmfd, foff, 0) == -1 || 
	    read(kd->pmfd, vm->pmemarr, sizeof(vm->pmemarr)) < 0) {
		_kvm_err(kd, kd->program, "cannot read pmemarr");
		return (-1);
	}

	foff = nlist[X_NPMEMARR].n_value - KERNBASE;
	if (lseek(kd->pmfd, foff, 0) == -1 || 
	    read(kd->pmfd, &vm->npmemarr, sizeof(vm->npmemarr)) < 0) {
		_kvm_err(kd, kd->program, "cannot read npmemarr");
		return (-1);
	}

	return (0);
}

int
_kvm_kvatop4m(kd, va, pa)
	kvm_t *kd;
	u_long va;
	u_long *pa;
{
	register struct vmstate *vm = kd->vmst;
	register int vr, vs, nmem, off;
	int pte;
	off_t foff;
	struct regmap *rp;
	struct segmap *sp;
	struct memarr *mp;

	if (va < KERNBASE)
		goto err;

	vr = VA_VREG(va);
	vs = VA_VSEG(va);

	sp = &vm->segmap_store[(vr-NUREG)*NSEGRG + vs];
	if (sp->sg_npte == 0)
		goto err;

	/* Assume kernel mappings are all within first memory bank. */
	foff = (long)&sp->sg_pte[VA_VPG(va)] - KERNBASE;
	if (lseek(kd->pmfd, foff, 0) == -1 || 
	    read(kd->pmfd, (void *)&pte, sizeof(pte)) < 0) {
		_kvm_err(kd, kd->program, "cannot read pte");
		goto err;
	}

	if ((pte & SRMMU_TETYPE) == SRMMU_TEPTE) {
		register long p, dumpoff = 0;

		off = VA_OFF(va);
		p = (pte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT;
		/* Translate (sparse) pfnum to (packed) dump offset */
		for (mp = vm->pmemarr, nmem = vm->npmemarr; --nmem >= 0; mp++) {
			if (mp->addr <= p && p < mp->addr + mp->len)
				break;
			dumpoff += mp->len;
		}
		if (nmem < 0)
			goto err;
		*pa = (dumpoff + p - mp->addr) | off;
		return (kd->nbpg - off);
	}
err:
	_kvm_err(kd, 0, "invalid address (%x)", va);
	return (0);
}
