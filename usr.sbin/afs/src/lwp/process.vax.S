/* $arla: process.vax.S,v 1.1 2000/01/02 02:11:20 lha Exp $ */

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
#	Transcribed for Vaxen by M. Satyanarayanan, September 1985
#	Algorithm: "Monkey see, monkey do"
#
*/

	.text

/*
#
# struct savearea {
#	char	*topstack;
# }
#
*/

	.set	topstack,0

/* Stuff to allow saving/restoring registers */

/*
# savecontext(f, area1, newsp)
#     int (*f)(); struct savearea *area1; char *newsp;
*/

/* Stack offsets of arguments */
	.set	f,4
	.set	area1,8
	.set	newsp,12

.globl	_PRE_Block
.globl	_savecontext

_savecontext:
	.word 0x0ffc	# Save regs R2-R11
	movb	$1,_PRE_Block		# Critical section for preemption code
   	pushl	ap			# save old ap
	pushl	fp			# save old fp    
	movl	area1(ap),r0		# r0 = base of savearea
	movl	sp,topstack(r0)		# area->topstack = sp
	movl	newsp(ap),r0		# Get new sp
	beql	L1			# if new sp is 0, dont change stacks
	movl	r0,sp			# else switch to new stack
L1:
	movl	f(ap),r1		# r1 = f
	calls	$0,0(r1)		# f()

/* It is impossible to be here, so abort() */

	calls	$0,_abort

/*
# returnto(area2)
#     struct savearea *area2;
*/

/* Stack offset of argument */
	.set	area2,4

	.globl _returnto
_returnto:
	.word	0x0			# Who cares about these regs?
	movl	area2(ap),r0		# r0 = address of area2
	movl	topstack(r0),sp		# Restore sp
	movl	(sp)+,fp		# Restore fp
	movl	(sp)+,ap		# ,,,,
	clrb	_PRE_Block		# End of preemption critical section
	ret

	pushl	$1234			# I will gloat, Kazar
	calls	$0,_abort
