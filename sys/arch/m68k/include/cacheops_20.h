/*	$OpenBSD: cacheops_20.h,v 1.1 1997/07/06 07:46:23 downsj Exp $	*/
/*	$NetBSD: cacheops_20.h,v 1.1 1997/06/02 20:26:39 leo Exp $	*/

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
void TBIA_20 __P((void));
extern inline void
TBIA_20()
{
	__asm __volatile (" pflusha");
}

/*
 * Invalidate any TLB entry for given VA (TB Invalidate Single)
 */
void TBIS_20 __P((void *));
extern inline void
TBIS_20(va)
	void	*va;
{

	__asm __volatile (" pflushs	#0,#0,%0@" : : "a" (va) );
}

/*
 * Invalidate supervisor side of TLB
 */
void TBIAS_20 __P((void));
extern inline void
TBIAS_20()
{
	__asm __volatile (" pflushs #4,#4");
}

/*
 * Invalidate user side of TLB
 */
void TBIAU_20 __P((void));
extern inline void
TBIAU_20()
{
	__asm __volatile (" pflushs #0,#4;");
}

/*
 * Invalidate instruction cache
 */
void ICIA_20 __P((void));
extern inline void
ICIA_20()
{
	__asm __volatile (" movc %0,cacr;" : : "d" (IC_CLEAR));
}

void ICPA_20 __P((void));
extern inline void
ICPA_20()
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
#define	DCIA_20()
#define	DCIS_20()
#define	DCIU_20()
#define	DCIAS_20()

void PCIA_20 __P((void));
extern inline void
PCIA_20()
{
	__asm __volatile (" movc %0,cacr;" : : "d" (DC_CLEAR));
}
