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
static char sccsid[] = "@(#)kvm_sparc.c	8.1 (Berkeley) 6/4/93";
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
		int x_seginval;
		int x_npmemarr;
		struct memarr x_pmemarr[MA_SIZE];
		struct segmap x_segmap_store[NKREG*NSEGRG];
	} x;
#define seginval x.x_seginval
#define npmemarr x.x_npmemarr
#define pmemarr x.x_pmemarr
#define segmap_store x.x_segmap_store
	int *pte;
};
#define NPMEG(vm) ((vm)->seginval+1)

static int cputyp = -1;

static int pgshift, nptesg;

#define VA_VPG(va)	(cputyp==CPU_SUN4C ? VA_SUN4C_VPG(va) : VA_SUN4_VPG(va))

static void
_kvm_mustinit(kd)
	kvm_t *kd;
{
	if (cputyp != -1)
		return;
	for (pgshift = 12; (1 << pgshift) != kd->nbpg; pgshift++)
		;
	nptesg = NBPSG / kd->nbpg;

#if 1
	if (cputyp == -1) {
		if (kd->nbpg == 8192)
			cputyp = CPU_SUN4;
		else
			cputyp = CPU_SUN4C;
	}
#endif
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
 * Prepare for translation of kernel virtual addresses into offsets
 * into crash dump files. We use the MMU specific goop written at the
 * and of crash dump by pmap_dumpmmu().
 * (note: sun4/sun4c 2-level MMU specific)
 */
int
_kvm_initvtop(kd)
	kvm_t *kd;
{
	register int i;
	register int off;
	register struct vmstate *vm;
	struct stat st;
	struct nlist nlist[5];

	_kvm_mustinit(kd);

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

	off = st.st_size - roundup(sizeof(vm->x), kd->nbpg);
	errno = 0;
	if (lseek(kd->pmfd, (off_t)off, 0) == -1 && errno != 0 || 
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
	off = st.st_size - roundup(sizeof(vm->x), kd->nbpg) -
	      roundup(NPMEG(vm) * nptesg * sizeof(int), kd->nbpg);

	errno = 0;
	if (lseek(kd->pmfd, (off_t)off, 0) == -1 && errno != 0 || 
	    read(kd->pmfd, (char *)vm->pte, NPMEG(vm) * nptesg * sizeof(int)) < 0) {
		_kvm_err(kd, kd->program, "cannot read PMEG table");
		return (-1);
	}

	return (0);
}

#define VA_OFF(va) (va & (kd->nbpg - 1))

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
	register int vr, vs, pte, off, nmem;
	register struct vmstate *vm = kd->vmst;
	struct regmap *rp;
	struct segmap *sp;
	struct memarr *mp;

	_kvm_mustinit(kd);

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
