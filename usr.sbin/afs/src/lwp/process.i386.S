/* $arla: process.i386.S,v 1.1 2000/01/02 02:11:16 lha Exp $ */

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

/* i386 Assembly
 *
 * Written by Derek Atkins <warlord@MIT.EDU>
 * (debugging help by Chris Provenzano <proven@mit.edu>)
 * 11/1991
 * +  * "ojala que sea correcto!"
 */
	
#ifdef HAVE_MACHINE_ASM_H
#include <machine/asm.h>
#endif

#include <lwp_elf.h>

	
	.file "process.s"
	
	.data
	
	.text

/*
 * struct savearea {
 *    char    *topstack;
 * }
 */

	.set    topstack,0

/*
 * savecontext(f, area1, newsp)
 *    int (*f)(); struct savearea *area1; char *newsp;
 */

/* offsets, to make my life easier! */
	.set    f,8
	.set    area1,12
	.set    newsp,16

.globl		_C_LABEL(PRE_Block)
.globl		_C_LABEL(savecontext)

ENTRY(savecontext)
	pushl   %ebp                    /* New Frame! */
	movl    %esp,%ebp
	pushal                           /* Push all registers */
	movl    $1,_C_LABEL(PRE_Block)        /* Do not allow any interrupt finagling */
	movl    area1(%ebp),%eax        /* eax = base of savearea */
	movl    %esp,topstack(%eax)	/* area->topstack = esp */
	movl    newsp(%ebp),%eax        /* get new sp into eax */
	cmpl    $0,%eax
	je      L1                      /* if new sp is 0 then dont change esp */
	movl    %eax,%esp               /* go ahead.  make my day! */
L1:
	jmp     *f(%ebp)                        /* ebx = &f */

/* Shouldnt be here....*/

	call    _C_LABEL(abort)

/*
 * returnto(area2)
 *    struct savearea *area2;
 */

/* stack offset */
	.set    area2,8

.globl		_C_LABEL(returnto)

ENTRY(returnto)
	pushl   %ebp
	movl    %esp, %ebp              /* New frame, to get correct pointer */
	movl    area2(%ebp),%eax        /* eax = area2 */
	movl    topstack(%eax),%esp	/* restore esp */
	popal
	movl    $0,_C_LABEL(PRE_Block)            /* clear it up... */
	popl    %ebp
	ret
	
/* I see, said the blind man, as he picked up his hammer and saw! */
	pushl   $1234
	call    _C_LABEL(abort)

