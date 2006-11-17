/* $arla: process.m68k.S,v 1.7 2000/08/25 14:49:04 lha Exp $ */

/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

#include <config.h>

#undef RCSID

#include <lwp_elf.h>

#if !defined(__ELF__) && !defined(sun3)
#define reg(x) x
#else /* __ELF__ || sun3 */
#ifdef __STDC__
#define reg(x) %##x
#else
#define reg(x) %/**/x
#endif /* __STDC__ */
#endif /* __ELF__ || sun3 */

/*
#
#	Information Technology Center
#	Carnegie-Mellon University
#
#
*/
	.data

/*
#
#	Process assembly language assist for Suns.
#
*/

	.text
	.even

/*
#
# struct savearea {
#	char	*topstack;
# }
#
*/

	.globl	_C_LABEL(PRE_Block)
	.globl	_C_LABEL(savecontext)
	.globl	_C_LABEL(returnto)
	
topstack =	0

/* Stuff to allow saving/restoring registers */
nregs	=	13
regs	=	0x3ffe			| d1-d7 & a0-a5

/*
# savecontext(f, area1, newsp)
#     int (*f)(); struct savearea *area1; char *newsp;
*/

/* Stack offsets of arguments */
f	=	8
area1	=	12
newsp	=	16

ENTRY(savecontext)
	movl	#1,_C_LABEL(PRE_Block)	| Dont allow any interrupt finagling
	link	reg(a6),#-(nregs*4)	| Save frame pointer & ...
					| ... allocate space for nregs registers
/* Save registers */
	moveml	#regs,reg(sp)@

	movl	reg(a6)@(area1),reg(a0)		| a0 = base of savearea
	movl	reg(sp),reg(a0)@(topstack)	| area->topstack = sp
	movl	reg(a6)@(newsp),reg(d0)		| Get new sp
	jeq	forw1			| If newsp == 0, no stack switch
	movl	reg(d0),reg(sp)			| Switch to new stack
forw1:
	movl	reg(a6)@(f),reg(a0)		| a0 = f
	jbsr	reg(a0)@			| f()

/* It is impossible to be here, so abort() */

	jbsr	_C_LABEL(abort)

/*
# returnto(area2)
#     struct savearea *area2;
*/

/* Stack offset of argument */
area2	=	8

ENTRY(returnto)
	link	reg(a6),#0
	movl	reg(a6)@(area2),reg(a0)		| Base of savearea
	movl	reg(a0)@(topstack),reg(sp)	| Restore sp
/* Restore registers */
	moveml	reg(sp)@,#regs

	addl	#(nregs*4),reg(sp)
	movl	reg(sp),reg(a6)			| Argghh...be careful here
	unlk	reg(a6)
	clrl	_C_LABEL(PRE_Block)
	rts					| Return to previous process
