/*	$OpenBSD: cacheops_40.h,v 1.1 1997/07/06 07:46:24 downsj Exp $	*/
/*	$NetBSD: cacheops_40.h,v 1.1 1997/06/02 20:26:41 leo Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Leo Weppelman
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Invalidate entire TLB.
 */
void TBIA_40 __P((void));
extern inline void
TBIA_40()
{
	__asm __volatile (" .word 0xf518" ); /*  pflusha */
}

/*
 * Invalidate any TLB entry for given VA (TB Invalidate Single)
 */
void TBIS_40 __P((vm_offset_t));
extern inline void
TBIS_40(va)
	vm_offset_t	va;
{
	register vm_offset_t	r_va __asm("a0") = va;
	int	tmp;

	__asm __volatile (" movc   %1, dfc;"	/* select supervisor	*/
			  " .word 0xf508;"	/* pflush a0@		*/
			  " moveq  %3, %1;"	/* select user		*/
			  " movc   %1, dfc;"
			  " .word 0xf508;" : "=d" (tmp) :
			  "0" (FC_SUPERD), "a" (r_va), "i" (FC_USERD));
}

/*
 * Invalidate supervisor side of TLB
 */
void TBIAS_40 __P((void));
extern inline void
TBIAS_40()
{
	/*
	 * Cannot specify supervisor/user on pflusha, so we flush all
	 */
	__asm __volatile (" .word 0xf518;");
}

/*
 * Invalidate user side of TLB
 */
void TBIAU_40 __P((void));
extern inline void
TBIAU_40()
{
	/*
	 * Cannot specify supervisor/user on pflusha, so we flush all
	 */
	__asm __volatile (" .word 0xf518;");
}

/*
 * Invalidate instruction cache
 */
void ICIA_40 __P((void));
extern inline void
ICIA_40()
{
	__asm __volatile (" .word 0xf498;"); /* cinva ic */
}

void ICPA_40 __P((void));
extern inline void
ICPA_40()
{
	__asm __volatile (" .word 0xf498;"); /* cinva ic */
}

/*
 * Invalidate data cache.
 */
void DCIA_40 __P((void));
extern inline void
DCIA_40()
{
	__asm __volatile (" .word 0xf478;"); /* cpusha dc */
}

void DCIS_40 __P((void));
extern inline void
DCIS_40()
{
	__asm __volatile (" .word 0xf478;"); /* cpusha dc */
}

void DCIU_40 __P((void));
extern inline void
DCIU_40()
{
	__asm __volatile (" .word 0xf478;"); /* cpusha dc */
}

void DCIAS_40 __P((vm_offset_t));
extern inline void
DCIAS_40(va)
	vm_offset_t	va;
{
	register vm_offset_t	r_va __asm("a0") = va;

	__asm __volatile (" .word 0xf468;" : : "a" (r_va)); /* cpushl dc,a0@ */
}

void PCIA_40 __P((void));
extern inline void
PCIA_40()
{
	__asm __volatile (" .word 0xf478;"); /* cpusha dc */
}

void DCFA_40 __P((void));
extern inline void
DCFA_40()
{
	__asm __volatile (" .word 0xf478;"); /* cpusha dc */
}

/* invalidate instruction physical cache line */
void ICPL_40 __P((vm_offset_t));
extern inline void
ICPL_40(va)
	vm_offset_t	va;
{
	register vm_offset_t	r_va __asm("a0") = va;

	__asm __volatile (" .word 0xf488;" : : "a" (r_va)); /* cinvl ic,a0@ */
}

/* invalidate instruction physical cache page */
void ICPP_40 __P((vm_offset_t));
extern inline void
ICPP_40(va)
	vm_offset_t	va;
{
	register vm_offset_t	r_va __asm("a0") = va;

	__asm __volatile (" .word 0xf490;" : : "a" (r_va)); /* cinvp ic,a0@ */
}

/* invalidate data physical cache line */
void DCPL_40 __P((vm_offset_t));
extern inline void
DCPL_40(va)
	vm_offset_t	va;
{
	register vm_offset_t	r_va __asm("a0") = va;

	__asm __volatile (" .word 0xf448;" : : "a" (r_va)); /* cinvl dc,a0@ */
}

/* invalidate data physical cache page */
void DCPP_40 __P((vm_offset_t));
extern inline void
DCPP_40(va)
	vm_offset_t	va;
{
	register vm_offset_t	r_va __asm("a0") = va;

	__asm __volatile (" .word 0xf450;" : : "a" (r_va)); /* cinvp dc,a0@ */
}

/* invalidate data physical all */
void DCPA_40 __P((void));
extern inline void
DCPA_40()
{
	__asm __volatile (" .word 0xf458;"); /* cinva dc */
}

/* data cache flush line */
void DCFL_40 __P((vm_offset_t));
extern inline void
DCFL_40(va)
	vm_offset_t	va;
{
	register vm_offset_t	r_va __asm("a0") = va;

	__asm __volatile (" .word 0xf468;" : : "a" (r_va)); /* cpushl dc,a0@ */
}

/* data cache flush page */
void DCFP_40 __P((vm_offset_t));
extern inline void
DCFP_40(va)
	vm_offset_t	va;
{
	register vm_offset_t	r_va __asm("a0") = va;

	__asm __volatile (" .word 0xf470;" : : "a" (r_va)); /* cpushp dc,a0@ */
}
