/*	$OpenBSD: bus_space_notimpl.S,v 1.1 2004/02/01 05:09:48 drahn Exp $	*/
/*	$NetBSD: bus_space_notimpl.S,v 1.2 2001/09/10 02:20:19 reinoud Exp $	*/

/*
 * Copyright (c) 1997 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>

/*
 * BUS_SPACE - name of this bus space
 */

#define BUS_SPACE	bs_notimpl

#define __C(x,y)	__CONCAT(x,y)
#define __S(s)		__STRING(s)
#define NAME(func)	__C(BUS_SPACE,__C(_bs_,func))
#define LNAME(func)	__C(L,NAME(func))

#define	__L(x)		_C_LABEL(x)
#define GLOBAL(func)	.global	__L(NAME(func))
#define LABEL(func)	__L(NAME(func)):
#define LLABEL(func)	LNAME(func):

#define FTEXT(func,text)	__S(__C(NAME(func),text))


#define NOT_IMPL(func)							\
	GLOBAL(func)						;	\
LABEL(func)							;	\
	stmfd	sp!, {r0-r3}					;	\
	adr	r0, LNAME(__C(func,_text))			;	\
	mov	r1, sp						;	\
	b	_C_LABEL(panic)					;	\
								;	\
LLABEL(__C(func,_text))						;	\
	.asciz	FTEXT(func,: args at 0x%08x\n)			;	\
	.align	0						;


/*
 * misc functions
 */

NOT_IMPL(mmap)


/*
 * Generic bus_space I/O functions
 */

/*
 * read single
 */

NOT_IMPL(r_1)
NOT_IMPL(r_2)
NOT_IMPL(r_4)
NOT_IMPL(r_8)

/*
 * write single
 */

NOT_IMPL(w_1)
NOT_IMPL(w_2)
NOT_IMPL(w_4)
NOT_IMPL(w_8)

/*
 * read multiple
 */

NOT_IMPL(rm_1)
NOT_IMPL(rm_2)
NOT_IMPL(rm_4)
NOT_IMPL(rm_8)

/*
 * write multiple
 */

NOT_IMPL(wm_1)
NOT_IMPL(wm_2)
NOT_IMPL(wm_4)
NOT_IMPL(wm_8)

/*
 * read region
 */

NOT_IMPL(rr_1)
NOT_IMPL(rr_2)
NOT_IMPL(rr_4)
NOT_IMPL(rr_8)

/*
 * write region
 */

NOT_IMPL(wr_1)
NOT_IMPL(wr_2)
NOT_IMPL(wr_4)
NOT_IMPL(wr_8)

/*
 * set multiple
 */

NOT_IMPL(sm_1)
NOT_IMPL(sm_2)
NOT_IMPL(sm_4)
NOT_IMPL(sm_8)

/*
 * set region
 */

NOT_IMPL(sr_1)
NOT_IMPL(sr_2)
NOT_IMPL(sr_4)
NOT_IMPL(sr_8)

/*
 * copy
 */

NOT_IMPL(c_1)
NOT_IMPL(c_2)
NOT_IMPL(c_4)
NOT_IMPL(c_8)
