/*	$OpenBSD: kvm_sparc.c,v 1.10 2004/06/15 03:52:59 deraadt Exp $ */
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
 * 3. Neither the name of the University nor the names of its contributors
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
static char *rcsid = "$OpenBSD: kvm_sparc.c,v 1.10 2004/06/15 03:52:59 deraadt Exp $";
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
#include <sys/core.h>
#include <sys/kcore.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#include <uvm/uvm_extern.h>
#include <machine/vmparam.h>
#include <machine/pmap.h>
#include <machine/autoconf.h>
#include <machine/kcore.h>

#include <limits.h>
#include <db.h>

#include "kvm_private.h"


static int cputyp = -1;
static int pgshift;
static int nptesg;	/* [sun4/sun4c] only */

#define VA_VPG(va)	((cputyp == CPU_SUN4C || cputyp == CPU_SUN4M) \
				? VA_SUN4C_VPG(va) \
				: VA_SUN4_VPG(va))

#define VA_OFF(va) (va & (kd->nbpg - 1))


void
_kvm_freevtop(kvm_t *kd)
{
	if (kd->vmst != NULL) {
		_kvm_err(kd, kd->program, "_kvm_freevtop: internal error");
		kd->vmst = NULL;
	}
}

/*
 * Prepare for translation of kernel virtual addresses into offsets
 * into crash dump files. We use the MMU specific goop written at the
 * front of the crash dump by pmap_dumpmmu().
 */
int
_kvm_initvtop(kvm_t *kd)
{
	cpu_kcore_hdr_t *cpup = kd->cpu_data;

	switch (cputyp = cpup->cputype) {
	case CPU_SUN4:
		kd->nbpg = 8196;
		pgshift = 13;
		break;
	case CPU_SUN4C:
	case CPU_SUN4M:
		kd->nbpg = 4096;
		pgshift = 12;
		break;
	default:
		_kvm_err(kd, kd->program, "Unsupported CPU type");
		return (-1);
	}
	nptesg = NBPSG / kd->nbpg;
	return (0);
}

/*
 * Translate a kernel virtual address to a physical address using the
 * mapping information in kd->vm.  Returns the result in pa, and returns
 * the number of bytes that are contiguously available from this
 * physical address.  This routine is used only for crashdumps.
 */
int
_kvm_kvatop(kvm_t *kd, u_long va, u_long *pa)
{
	if (cputyp == -1)
		if (_kvm_initvtop(kd) != 0)
			return (-1);

	return ((cputyp == CPU_SUN4M) ? _kvm_kvatop4m(kd, va, pa) :
	    _kvm_kvatop44c(kd, va, pa));
}

/*
 * (note: sun4 3-level MMU not yet supported)
 */
int
_kvm_kvatop44c(kvm_t *kd, u_long va, u_long *pa)
{
	cpu_kcore_hdr_t *cpup = kd->cpu_data;
	int vr, vs, pte, *ptes;
	struct regmap *rp;
	struct segmap *sp;

	if (va < KERNBASE)
		goto err;

	/*
	 * Layout of CPU segment:
	 *	cpu_kcore_hdr_t;
	 *	[alignment]
	 *	phys_ram_seg_t[cpup->nmemseg];
	 *	ptes[cpup->npmegs];
	 */
	ptes = (int *)((int)kd->cpu_data + cpup->pmegoffset);

	vr = VA_VREG(va);
	vs = VA_VSEG(va);

	sp = &cpup->segmap_store[(vr-NUREG)*NSEGRG + vs];
	if (sp->sg_npte == 0)
		goto err;
	if (sp->sg_pmeg == cpup->npmeg - 1) /* =seginval */
		goto err;
	pte = ptes[sp->sg_pmeg * nptesg + VA_VPG(va)];
	if ((pte & PG_V) != 0) {
		long p, off = VA_OFF(va);

		p = (pte & PG_PFNUM) << pgshift;
		*pa = p + off;
		return (kd->nbpg - off);
	}
err:
	_kvm_err(kd, 0, "invalid address (%x)", va);
	return (0);
}

int
_kvm_kvatop4m(kvm_t *kd, u_long va, u_long *pa)
{
	cpu_kcore_hdr_t *cpup = kd->cpu_data;
	struct regmap *rp;
	struct segmap *sp;
	int vr, vs, pte;
	off_t foff;

	if (va < KERNBASE)
		goto err;

	/*
	 * Layout of CPU segment:
	 *	cpu_kcore_hdr_t;
	 *	[alignment]
	 *	phys_ram_seg_t[cpup->nmemseg];
	 */
	vr = VA_VREG(va);
	vs = VA_VSEG(va);

	sp = &cpup->segmap_store[(vr-NUREG)*NSEGRG + vs];
	if (sp->sg_npte == 0)
		goto err;

	/* XXX - assume page tables in initial kernel DATA or BSS. */
	foff = _kvm_pa2off(kd, (u_long)&sp->sg_pte[VA_VPG(va)] - KERNBASE);
	if (foff == (off_t)-1)
		return (0);

	if (_kvm_pread(kd, kd->pmfd, (void *)&pte, sizeof(pte), foff) < 0) {
		_kvm_err(kd, kd->program, "cannot read pte for %x", va);
		return (0);
	}

	if ((pte & SRMMU_TETYPE) == SRMMU_TEPTE) {
		long p, off = VA_OFF(va);

		p = (pte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT;
		*pa = p + off;
		return (kd->nbpg - off);
	}
err:
	_kvm_err(kd, 0, "invalid address (%x)", va);
	return (0);
}

/*
 * Translate a physical address to a file-offset in the crash-dump.
 */
off_t
_kvm_pa2off(kvm_t *kd, u_long pa)
{
	cpu_kcore_hdr_t *cpup = kd->cpu_data;
	phys_ram_seg_t *mp;
	off_t off;
	int nmem;

	/*
	 * Layout of CPU segment:
	 *	cpu_kcore_hdr_t;
	 *	[alignment]
	 *	phys_ram_seg_t[cpup->nmemseg];
	 */
	mp = (phys_ram_seg_t *)((int)kd->cpu_data + cpup->memsegoffset);
	off = 0;

	/* Translate (sparse) pfnum to (packed) dump offset */
	for (nmem = cpup->nmemseg; --nmem >= 0; mp++) {
		if (mp->start <= pa && pa < mp->start + mp->size)
			break;
		off += mp->size;
	}
	if (nmem < 0) {
		_kvm_err(kd, 0, "invalid address (%x)", pa);
		return (-1);
	}

	return (kd->dump_off + off + pa - mp->start);
}
