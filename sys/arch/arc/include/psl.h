/*      $OpenBSD: psl.h,v 1.2 1998/01/28 13:46:25 pefo Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *
 *	from: @(#)psl.h	8.1 (Berkeley) 6/10/93
 */

#include <machine/cpu.h>

#define	PSL_LOWIPL	(INT_MASK | SR_INT_ENAB)

#define	PSL_USERSET (	\
	SR_KSU_USER |	\
	SR_INT_ENAB |	\
	SR_EXL      |	\
	INT_MASK)

#define	PSL_USERCLR (	\
	SR_COP_USABILITY |	\
	SR_BOOT_EXC_VEC |	\
	SR_TLB_SHUTDOWN |	\
	SR_PARITY_ERR |	\
	SR_CACHE_MISS |	\
	SR_PARITY_ZERO |	\
	SR_SWAP_CACHES |	\
	SR_ISOL_CACHES |	\
	SR_KU_CUR |	\
	SR_INT_ENA_CUR |	\
	SR_MBZ)

/*
 * Macros to decode processor status word.
 */
#define	USERMODE(ps)	(((ps) & SR_KSU_MASK) == SR_KSU_USER)
#define	BASEPRI(ps)	(((ps) & (INT_MASK | SR_INT_ENA_PREV)) \
			== (INT_MASK | SR_INT_ENA_PREV))

#ifdef _KERNEL
#include <machine/intr.h>
#endif
