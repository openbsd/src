/*	$OpenBSD: cpufunc.h,v 1.2 1998/08/29 01:56:55 mickey Exp $	*/

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

#ifndef _HPPA_CPUFUNC_H_
#define _HPPA_CPUFUNC_H_

#include <machine/psl.h>
#include <machine/pte.h>

#define tlbbtop(b) (((b) & ~PGOFSET) >> (PGSHIFT - 5))
#define tlbptob(p) ((p) << (PGSHIFT - 5))


/* Get space register for an address */
static __inline u_int ldsid(vm_offset_t p) {
	register u_int ret;
	__asm __volatile("ldsid (%1),%0" : "=r" (ret) : "r" (p));
	return ret;
}

/* Disable SID hashing and flush all caches for S-CHIP */
static __inline u_int disable_S_sid_hashing(void) {
	register u_int t, ret;
	__asm ("mfcpu	(0,%1)\n\t"	/* get cpu diagnosic register */
	       "mfcpu	(0,%1)\n\t"	/* black magic */
	       "copy	%1,%0\n\t"
	       "depi	0,20,3,%1\n\t"	/* clear DHE, domain and IHE bits */
	       "depi	1,16,1,%1\n\t"	/* enable quad-word stores */
	       "depi	0,10,1,%1\n\t"	/* do not clear the DHPMC bit */
	       "depi	0,14,1,%1\n\t"	/* do not clear the ILPMC bit */
	       "mtcpu	(%1,0)\n\t"	/* set the cpu disagnostic register */
	       "mtcpu	(%1,0)\n\t"	/* black magic */
	       : "=r" (ret) : "r" (t));
	return ret;
}

/* Disable SID hashing and flush all caches for T-CHIP */
static __inline u_int disable_T_sid_hashing(void) {
	register u_int t, ret;
	__asm("mfcpu	(0,%1)\n\t"	/* get cpu diagnosic register */
	      "mfcpu	(0,%1)\n\t"	/* black magic */
	      "copy	%1,%0\n\t"
	      "depi	0,18,1,%1\n\t"	/* clear DHE bit */
	      "depi	0,20,1,%1\n\t"	/* clear IHE bit */
	      "depi	0,10,1,%1\n\t"	/* do not clear the DHPMC bit */
	      "depi	0,14,1,%1\n\t"	/* do not clear the ILPMC bit */
	      "mtcpu	(%1,0)\n\t"	/* set the cpu disagnostic register */
	      "mtcpu	(%1,0)\n\t"	/* black magic */
	       : "=r" (ret) : "r" (t));
	return ret;
}

/* Disable SID hashing and flush all caches for L-CHIP */
static __inline u_int disable_L_sid_hashing(void) {
	register u_int t, ret;
	__asm("mfcpu2	(0,%1)\n\t"	/* get cpu diagnosic register  */
/*	      ".word	0x14160600\n\t" */
	      "copy	%1,%0\n\t"
	      "depi	0,27,1,%1\n\t"	/* clear DHE bit */
	      "depi	0,28,1,%1\n\t"	/* clear IHE bit */
	      "depi	0,6,1,%1\n\t"	/* do not clear the L2IHPMC bit */
	      "depi	0,8,1,%1\n\t"	/* do not clear the L2DHPMC bit */
	      "depi	0,10,1,%1\n\t"	/* do not clear the L1IHPMC bit */
	      "mtcpu2	(%1,0)"		/* set the cpu disagnostic register */ 
/*	      ".word	0x14160240\n\t" */
	       : "=r" (ret) : "r" (t));
	return ret;
}

static __inline u_int get_dcpu_reg(void) {
	register u_int ret;
	__asm("mfcpu	(0,%0)\n\t"	/* Get cpu diagnostic register */
	      "mfcpu	(0,%0)": "=r" (ret));	/* black magic */
	return ret;
}

#define mtctl(v,r) __asm __volatile("mtctl %0,%1":: "r" (v), "i" (r))
#define mfctl(r,v) __asm __volatile("mfctl %1,%0": "=r" (v): "i" (r))

#define mtcpu(v,r) __asm __volatile("mtcpu %0,%1":: "r" (v), "i" (r))
#define mfcpu(r,v) __asm __volatile("mfcpu %1,%0": "=r" (v): "i" (r))

#define mtcpu2(v,r) __asm __volatile("mtcpu2 %0,%1":: "r" (v), "i" (r))
#define mfcpu2(r,v) __asm __volatile("mfcpu2 %1,%0": "=r" (v): "i" (r))

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

static __inline void set_psw(u_int psw) {
	__asm __volatile("mtctl %%r0, %%cr17\n\t"
			 "mtctl %%r0, %%cr17\n\t"
			 "ldil L%%.+32, %%r21\n\t"
			 "ldo R%%.+28(%%r21), %%r21\n\t"
			 "mtctl %%r21, %%cr17\n\t"
			 "ldo 4(%%r21), %%r21\n\t"
			 "mtctl %%r21, %%cr17\n\t"
			 "mtctl %0, %%cr22\n\t"
			 "rfi\n\t"
			 "nop\n\tnop\n\tnop\n\tnop"
			 :: "r" (psw): "r21");
}

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

static __inline void
ibitlb(int i, vm_offset_t pa, vm_offset_t va, vm_size_t sz, u_int prot)
{

}

static __inline void
pbitlb(int i)
{
}

static __inline void
ibdtlb(int i, vm_offset_t pa, vm_offset_t va, vm_size_t sz, u_int prot)
{

}

static __inline void
pbdtlb(int i)
{
}

static __inline void
ibctlb(int i, vm_offset_t pa, vm_offset_t va, vm_size_t sz, u_int prot)
{
	register u_int psw, t;

	rsm(PSW_R|PSW_I,psw);

	t = 0x7fc1|((i&15)<<1);	/* index 127, lockin, override, mismatch */
	mtcpu(t,8);		/* move to the dtlb diag reg */
	mtcpu(t,8);		/* black magic */

	prot |= TLB_DIRTY;
	sz = (~sz >> 7) & 0x7f000;
	pa = (tlbbtop(pa) & 0x7f000) | sz;
	va = (va & 0x7f000) | sz;

	idtlba(pa, 0, va);
	idtlbp(prot, 0, va);

	t |= 0x2000;		/* no lockout, PE force-ins disable */
	mtcpu2(t,8);		/* move to the dtlb diagnostic register */

	mtsm(psw);
}

static __inline void
pbctlb(int i)
{
	register u_int psw, t;

	rsm(PSW_R|PSW_I,psw);

	t = 0xffc1|((i&15)<<1);	/* index 127, lockin, override, mismatch */
	mtcpu(t,8);		/* move to the dtlb diag reg */
	mtcpu(t,8);		/* black magic */

	idtlba(0,0,0);		/* address does not matter */
	idtlbp(0,0,0);

	t |= 0x7f << 7;
	mtcpu(t,8);		/* move to the dtlb diagnostic register */
	mtcpu(t,8);		/* black magic */

	mtsm(psw);
}

static __inline void
iLbctlb(int i, vm_offset_t pa, vm_offset_t va, vm_offset_t sz, u_int prot)
{
	register u_int psw, t;

	rsm(PSW_R|PSW_I,psw);

	t = 0x6041| ((i&7)<<1);	/* lockin, PE force-insert disable,
				   PE LRU-ins dis, BE force-ins enable
				   set the block enter select bit */
	mtcpu2(t, 8);		/* move to the dtlb diagnostic register */

	prot |= TLB_DIRTY;
	sz = (~sz >> 7) & 0x7f000;
	pa = (tlbbtop(pa) & 0x7f000) | sz;
	va = (va & 0x7f000) | sz;

	/* we assume correct address/size alignment */
	idtlba(pa, 0, va);
	idtlbp(prot, 0, va);

	t |= 0x2000;		/* no lockin, PE force-ins disable */
	mtcpu2(t, 8);		/* move to the dtlb diagnostic register */

	mtsm(psw);
}

static __inline void
pLbctlb(int i)
{
	register u_int psw, t;

	rsm(PSW_R|PSW_I,psw);

	t = 0xc041| ((i&7)<<1);	/* lockout, PE force-insert disable,
				   PE LRU-ins dis, BE force-ins enable
				   set the block enter select bit */
	mtcpu2(t,8);		/* move to the dtlb diagnostic register */

	idtlba(0,0,0);		/* address does not matter */
	idtlbp(0,0,0);

	t |= 0x2000;		/* no lockout, PE force-ins disable */
	mtcpu2(t,8);		/* move to the dtlb diagnostic register */

	mtsm(psw);
}

#ifdef _KERNEL
struct pdc_cache;
void fcacheall __P((struct pdc_cache *));
void ptlball __P((struct pdc_cache *));
#endif

#endif /* _HPPA_CPUFUNC_H_ */



