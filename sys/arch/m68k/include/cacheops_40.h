/*	$OpenBSD: cacheops_40.h,v 1.4 2002/03/14 01:26:34 millert Exp $	*/
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
void TBIA_40(void);
extern __inline__ void
TBIA_40()
{
	__asm __volatile (" .word 0xf518" ); /*  pflusha */
}

/*
 * Invalidate any TLB entry for given VA (TB Invalidate Single)
 */
void TBIS_40(vaddr_t);
extern __inline__ void
TBIS_40(va)
	vaddr_t	va;
{
	register vaddr_t	r_va __asm("a0") = va;
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
void TBIAS_40(void);
extern __inline__ void
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
void TBIAU_40(void);
extern __inline__ void
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
void ICIA_40(void);
extern __inline__ void
ICIA_40()
{
	__asm __volatile (" .word 0xf498;"); /* cinva ic */
}

void ICPA_40(void);
extern __inline__ void
ICPA_40()
{
	__asm __volatile (" .word 0xf498;"); /* cinva ic */
}

/*
 * Invalidate data cache.
 */
void DCIA_40(void);
extern __inline__ void
DCIA_40()
{
	__asm __volatile (" .word 0xf478;"); /* cpusha dc */
}

void DCIS_40(void);
extern __inline__ void
DCIS_40()
{
	__asm __volatile (" .word 0xf478;"); /* cpusha dc */
}

void DCIU_40(void);
extern __inline__ void
DCIU_40()
{
	__asm __volatile (" .word 0xf478;"); /* cpusha dc */
}

void DCIAS_40(paddr_t);
extern __inline__ void
DCIAS_40(pa)
	paddr_t	pa;
{
	register paddr_t	r_pa __asm("a0") = pa;

	__asm __volatile (" .word 0xf468;" : : "a" (r_pa)); /* cpushl dc,a0@ */
}

void PCIA_40(void);
extern __inline__ void
PCIA_40()
{
	__asm __volatile (" .word 0xf478;"); /* cpusha dc */
}

void DCFA_40(void);
extern __inline__ void
DCFA_40()
{
	__asm __volatile (" .word 0xf478;"); /* cpusha dc */
}

/* invalidate instruction physical cache line */
void ICPL_40(paddr_t);
extern __inline__ void
ICPL_40(pa)
	paddr_t	pa;
{
	register paddr_t	r_pa __asm("a0") = pa;

	__asm __volatile (" .word 0xf488;" : : "a" (r_pa)); /* cinvl ic,a0@ */
}

/* invalidate instruction physical cache page */
void ICPP_40(paddr_t);
extern __inline__ void
ICPP_40(pa)
	paddr_t	pa;
{
	register paddr_t	r_pa __asm("a0") = pa;

	__asm __volatile (" .word 0xf490;" : : "a" (r_pa)); /* cinvp ic,a0@ */
}

/* invalidate data physical cache line */
void DCPL_40(paddr_t);
extern __inline__ void
DCPL_40(pa)
	paddr_t	pa;
{
	register paddr_t	r_pa __asm("a0") = pa;

	__asm __volatile (" .word 0xf448;" : : "a" (r_pa)); /* cinvl dc,a0@ */
}

/* invalidate data physical cache page */
void DCPP_40(paddr_t);
extern __inline__ void
DCPP_40(pa)
	paddr_t	pa;
{
	register paddr_t	r_pa __asm("a0") = pa;

	__asm __volatile (" .word 0xf450;" : : "a" (r_pa)); /* cinvp dc,a0@ */
}

/* invalidate data physical all */
void DCPA_40(void);
extern __inline__ void
DCPA_40()
{
	__asm __volatile (" .word 0xf458;"); /* cinva dc */
}

/* data cache flush line */
void DCFL_40(paddr_t);
extern __inline__ void
DCFL_40(pa)
	paddr_t	pa;
{
	register paddr_t	r_pa __asm("a0") = pa;

	__asm __volatile (" .word 0xf468;" : : "a" (r_pa)); /* cpushl dc,a0@ */
}

/* data cache flush page */
void DCFP_40(paddr_t);
extern __inline__ void
DCFP_40(pa)
	paddr_t	pa;
{
	register paddr_t	r_pa __asm("a0") = pa;

	__asm __volatile (" .word 0xf470;" : : "a" (r_pa)); /* cpushp dc,a0@ */
}
