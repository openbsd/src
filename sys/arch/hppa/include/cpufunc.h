/*	$OpenBSD: cpufunc.h,v 1.5 1998/12/29 21:47:13 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
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
/*
 *  (c) Copyright 1988 HEWLETT-PACKARD COMPANY
 *
 *  To anyone who acknowledges that this file is provided "AS IS"
 *  without any express or implied warranty:
 *      permission to use, copy, modify, and distribute this file
 *  for any purpose is hereby granted without fee, provided that
 *  the above copyright notice and this notice appears in all
 *  copies, and that the name of Hewlett-Packard Company not be
 *  used in advertising or publicity pertaining to distribution
 *  of the software without specific, written prior permission.
 *  Hewlett-Packard Company makes no representations about the
 *  suitability of this software for any purpose.
 */
/*
 * Copyright (c) 1990,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * THE UNIVERSITY OF UTAH AND CSL PROVIDE THIS SOFTWARE IN ITS "AS IS"
 * CONDITION, AND DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES
 * WHATSOEVER RESULTING FROM ITS USE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: c_support.s 1.8 94/12/14$
 *	Author: Bob Wheeler, University of Utah CSL
 */

#ifndef _MACHINE_CPUFUNC_H_
#define _MACHINE_CPUFUNC_H_

#include <machine/psl.h>
#include <machine/pte.h>

#define tlbbtop(b) ((b) >> (PGSHIFT - 5))
#define tlbptob(p) ((p) << (PGSHIFT - 5))

#define hptbtop(b) ((b) >> 17)

/* Get space register for an address */
static __inline u_int ldsid(vm_offset_t p) {
	register u_int ret;
	__asm __volatile("ldsid (%1),%0" : "=r" (ret) : "r" (p));
	return ret;
}

#define mtctl(v,r) __asm __volatile("mtctl %0,%1":: "r" (v), "i" (r))
#define mfctl(r,v) __asm __volatile("mfctl %1,%0": "=r" (v): "i" (r))

#define mtsp(v,r) __asm __volatile("mtsp %0,%1":: "r" (v), "i" (r))
#define mfsp(r,v) __asm __volatile("mfsp %1,%0": "=r" (v): "i" (r))

#define ssm(v,r) __asm __volatile("ssm %1,%0": "=r" (r): "i" (v))
#define rsm(v,r) __asm __volatile("rsm %1,%0": "=r" (r): "i" (v))

/* Move to system mask. Old value of system mask is returned. */
static __inline u_int mtsm(u_int mask) {
	register u_int ret;
	__asm __volatile("ssm 0,%0\n\t"
			 "mtsm %1": "=r" (ret) : "r" (mask));
	return ret;
}

#if 0
static __inline void set_psw(u_int psw) {
	__asm __volatile("mtctl %0, %%cr22\n\t"
			 "mtctl %%r0, %%cr17\n\t"
			 "mtctl %%r0, %%cr17\n\t"
			 "ldil L%%., %0\n\t"
			 "ldo R%%.+24(%0), %0\n\t"
			 "mtctl %0, %%cr18\n\t"
			 "ldo 4(%0), %0\n\t"
			 "mtctl %0, %%cr18\n\t"
			 "rfi\n\tnop\n\tnop"
			 :: "r" (psw));
}
#else
void set_psw __P((u_int psw));
#endif

#define	fdce(sp,off) __asm __volatile("fdce 0(%0,%1)":: "i" (sp), "r" (off))
#define	fice(sp,off) __asm __volatile("fdce 0(%0,%1)":: "i" (sp), "r" (off))
#define sync_caches() \
    __asm __volatile("sync\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop")

static __inline void
ficache(pa_space_t space, vm_offset_t off, vm_size_t size)
{

}

static __inline void
fdcache(pa_space_t space, vm_offset_t off, vm_size_t size)
{

}

static __inline void
iitlba(u_int pg, pa_space_t sp, vm_offset_t off)
{
	mtsp(1, sp);
	__asm volatile("iitlba %0,(%%sr1, %1)":: "r" (pg), "r" (off));
}

static __inline void
idtlba(u_int pg, pa_space_t sp, vm_offset_t off)
{
	mtsp(sp, 1);
	__asm volatile("idtlba %0,(%%sr1, %1)":: "r" (pg), "r" (off));
}

static __inline void
iitlbp(u_int prot, pa_space_t sp, vm_offset_t off)
{
	mtsp(sp, 1);
	__asm volatile("iitlbp %0,(%%sr1, %1)":: "r" (prot), "r" (off));
}

static __inline void
idtlbp(u_int prot, pa_space_t sp, vm_offset_t off)
{
	mtsp(sp, 1);
	__asm volatile("idtlbp %0,(%%sr1, %1)":: "r" (prot), "r" (off));
}

static __inline void
pitlb(pa_space_t sp, vm_offset_t off)
{
	mtsp(sp, 1);
	__asm volatile("pitlb %%r0(%%sr1, %0)":: "r" (off));
}

static __inline void
pdtlb(pa_space_t sp, vm_offset_t off)
{
	mtsp(sp, 1);
	__asm volatile("pdtlb %%r0(%%sr1, %0)":: "r" (off));
}

static __inline void
pitlbe(pa_space_t sp, vm_offset_t off)
{
	mtsp(sp, 1);
	__asm volatile("pitlbe %%r0(%%sr1, %0)":: "r" (off));
}

static __inline void
pdtlbe(pa_space_t sp, vm_offset_t off)
{
	mtsp(sp, 1);
	__asm volatile("pdtlbe %%r0(%%sr1, %0)":: "r" (off));
}

#ifdef _KERNEL
void fcacheall __P((void));
void ptlball __P((void));
int btlb_insert __P((pa_space_t space, vm_offset_t va, vm_offset_t pa,
		     vm_size_t *lenp, u_int prot));
hppa_hpa_t cpu_gethpa __P((int n));
void heartbeat __P((int on));
#endif

#endif /* _MACHINE_CPUFUNC_H_ */
