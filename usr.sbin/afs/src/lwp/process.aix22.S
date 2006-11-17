/* $arla: process.aix22.S,v 1.1 2000/01/02 02:11:15 lha Exp $ */

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
*/
/*
#
#	Process assembly language assist for Sailboats.
#
*/

	.text
	.globl	.savecontext
	.align 1

/*
#
# struct savearea {
#	char	*topstack;
# }
#
*/


/*# Offsets of fields*/
.set topstack,0

/*# Stuff to allow saving/restoring registers*/
.set regspace,64
.set freg,0

/*
#
# savecontext(f, area1, newsp)
#    int (*f)(); struct savearea *area1; char *newsp;
#
*/

.savecontext:
	ai	1,1,-regspace		# Save frame pointer & ...

/*# Save registers*/
	stm	0,0(1)			# Change this if save fewer regs.
	lr	14,0
/*# Set preemption semaphore*/
	lis	6,1
	l	7,4(14)
	sts	6,0(7)
/*# r3 = base of savearea*/
	st	1,topstack(3)		# area1->topstack = sp
/*# New sp is in r4.*/
	ci	4,0
	beq	L1			# If newsp == 0, no stack switch
	cas	1,4,0			# Switch to new stack
L1:
	l	6,0(2)			# r2 = _f
	balrx	15,6			# f()
	cas	0,2,0
	.data	3
	.globl	_savecontext
_savecontext:
	.long	.savecontext
	.long	_PRE_Block
/*
#
# returnto(area2)
#     struct savearea *area2;
#
*/

	.text
	.globl	.returnto
	.align 1
.returnto:
	l	1,topstack(2)
/*
# Now in the context of the savecontext stack to be restored.
# Start with the registers...
# Clear preemption semaphore
*/
	lr	14,0
	lis	6,0
	l	7,4(14)
	sts	6,0(7)
	lm	0,0(1)		# Change if saving fewer regs.
	brx	15		# Return to previous process
	ai	1,1,regspace
	.data	3
	.globl	_returnto
_returnto:
	.long	.returnto
	.long	_PRE_Block
