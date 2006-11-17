/* $arla: process.ibm032.S,v 1.1 2000/01/02 02:11:16 lha Exp $ */

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

|
|	Information Technology Center
|	Carnegie-Mellon University
|
|
	.data
	.globl	.oVncs
	.set		.oVncs,0

	.globl	_savecontext
_savecontext:
	.long		_.savecontext

	.globl	_returnto
_returnto:
	.long		_.returnto

|
|	Process assembly language assist for Sailboats.
|

	.text
	.align 2

|
| struct savearea {
|	char	*topstack;
| }
|

| Offsets of fields
.set topstack,0

| Stuff to allow saving/restoring registers
.set regspace,64
.set freg,0

|
| savecontext(f, area1, newsp)
|    int (*f)(); struct savearea *area1; char *newsp;
|

	.globl	_.savecontext
_.savecontext:
	ai	sp,sp,-regspace		| Save frame pointer & ...
					| ... allocate space for 16 registers
| Save registers
	stm	r0,0(sp)			| Change this if save fewer regs.
| Set preemption semaphore
	get	r6,$1
	get	r7,$_PRE_Block
	put	r6,0(r7)			| PRE_Block = 1
| r3 = base of savearea
	put	sp,topstack(r3)		| area1->topstack = sp
| New sp is in r4.
	cis	r4,0
	be	L1			| If newsp == 0, no stack switch
	cas	sp,r4,r0			| Switch to new stack
L1:
	get	r6,0(r2)			| r2 = _f
	balrx	r15,r6			| f()
	cas	r0,r2,r0

|
| returnto(area2)
|     struct savearea *area2;
|

	.globl _.returnto
_.returnto:
	get	sp,topstack(r2)
| Now in the context of the savecontext stack to be restored.
| Start with the registers...
| Clear preemption semaphore
	get	r6,$0
	get	r7,$_PRE_Block
	put	r6,0(r7)			| PRE_Block = 0
	lm	r0,0(sp)		| Change if saving fewer regs.
	brx	r15		| Return to previous process
	ai	sp,sp,regspace
 .data
 .ltorg
