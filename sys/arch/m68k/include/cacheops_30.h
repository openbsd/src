/*	$OpenBSD: cacheops_30.h,v 1.5 2002/08/09 21:26:15 mickey Exp $	*/
/*	$NetBSD: cacheops_30.h,v 1.1 1997/06/02 20:26:40 leo Exp $	*/

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
void TBIA_30(void);
extern __inline__ void
TBIA_30()
{
	int tmp = DC_CLEAR;

	__asm __volatile (" pflusha;"
			  " movc %0,cacr" : : "d" (tmp));
}
	
/*
 * Invalidate any TLB entry for given VA (TB Invalidate Single)
 */
void TBIS_30(vaddr_t);
extern __inline__ void
TBIS_30(va)
	vaddr_t	va;
{
	__asm __volatile (" pflush #0,#0,%0@;"
			  " movc   %1,cacr" : : "a" (va), "d" (DC_CLEAR));
}

/*
 * Invalidate supervisor side of TLB
 */
void TBIAS_30(void);
extern __inline__ void
TBIAS_30()
{
	__asm __volatile (" pflush #4,#4;"
			  " movc   %0,cacr;" :: "d" (DC_CLEAR));
}

/*
 * Invalidate user side of TLB
 */
void TBIAU_30(void);
extern __inline__ void
TBIAU_30()
{
	__asm __volatile (" pflush #0,#4;"
			  " movc   %0,cacr;" :: "d" (DC_CLEAR));
}

/*
 * Invalidate instruction cache
 */
void ICIA_30(void);
extern __inline__ void
ICIA_30()
{
	__asm __volatile (" movc %0,cacr;" : : "d" (IC_CLEAR));
}

void ICPA_30(void);
extern __inline__ void
ICPA_30()
{
	__asm __volatile (" movc %0,cacr;" : : "d" (IC_CLEAR));
}

/*
 * Invalidate data cache.
 * NOTE: we do not flush 68030/20 on-chip cache as there are no aliasing
 * problems with DC_WA.  The only cases we have to worry about are context
 * switch and TLB changes, both of which are handled "in-line" in resume
 * and TBI*.
 */
#define	DCIA_30()
#define	DCIS_30()
#define	DCIU_30()
#define	DCIAS_30(va)
#define	DCFA_30()
#define	DCPA_30()


void PCIA_30(void);
extern __inline__ void
PCIA_30()
{
	__asm __volatile (" movc %0,cacr;" : : "d" (DC_CLEAR));
}
