/* $arla: process.rios.S,v 1.1 2000/01/02 02:11:19 lha Exp $ */

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

/* lws 92.11.18    I don't know if we have to save the TOC (R2) or not...
 *		   Note that stack-frame is supposed to be aligned on 
 *		   a double-word boundary.
 *		   For details about RIOS calling conventions
 *		   see the Assembler manual and /usr/include/sys/asdef.s
 */


/*
 * savecontext(f, area1, newsp)
 *     int (*f)(); struct savearea *area1; char *newsp;
 */
	.set	topstack, 0
	.set	cr0, 0
	.set	toc, 2
	.set	r0, 0
	.set	r1, 1
	.set	r2, 2
	.set	r3, 3
	.set	r4, 4
	.set	r5, 5
	.set	r6, 6
	.set	r7, 7
	.set	r12, 12
	.set	a_f, r3
	.set	a_area1, r4
	.set	a_newsp, r5

	.set	argarea,  32
	.set	linkarea, 24
	.set    nfprs,    18
	.set    ngprs,    20
	.set    szdsa,	  8*nfprs+4*ngprs+linkarea+argarea

	.csect .savecontext[PR]
	.globl .savecontext[PR]

	mflr	r0			# save link register
		
/*
 *  save floating point registers.  Interleave some other stuff for
 *  timing reasons.  Set up conditions and registers for branches
 *  early, so that processor can prefetch instructions.
 */
	stfd  14, -144(1)
	stfd  15, -136(1)

	mfcr	r12			# save CR

	stfd  16, -128(1)
	stfd  17, -120(1)

	l	11, 0(a_f)		# r11 <- *(a_f)

   	stfd  18, -112(1)
	stfd  19, -104(1)

	cmpi	cr0, a_newsp, 0		# cr0 <- (a_newsp :: 0)

	stfd  20, -96(1)
	stfd  21, -88(1)
	stfd  22, -80(1)

	mtlr	11			# set up lr early so prefetch works

	stfd  23, -72(1)
	stfd  24, -64(1)
	stfd  25, -56(1)

	st	r0, 8(r1)		# save return addr

	stfd  26, -48(1)
	stfd  27, -40(1)
	stfd  28, -32(1)

	st	12, 4(r1)		# save CR

	stfd  29, -24(1)
	stfd  30, -16(1)
	stfd  31, -8(1)

/*
 *  save general-purpose registers
 */
	stm	12, -8*nfprs-4*ngprs(r1)# save the general-purpose regs
	stu	r1, -szdsa(r1)		# dec SP and save back chain

	l	r7,  PRE_Block.S(toc)	# r7 <- &PRE_Block
	cal	r6, 1(r0)		# r6 <- #1
	stb	r6, 0(r7)		# r6 -> PRE_Block

	st	r1, topstack(a_area1)	# save old SP
	
	beq    L1			# if (a_newsp == 0) goto L1

	mr	r1, r5			# r1 <- a_newsp	-- load new SP

L1:	brl				# pc <- lr	-- (*a_f)()

/*
 * returnto(area2)   This is a little jumbled, I tried to interleave 
 * memory accesses with simple instructions for speed, and I tried to 
 * set up the link register and condition register reasonably early
 * so that processor instruction prefetching might help us out a little.
 */
	.set	a_area2, r3

	.csect  .returnto[PR]
	.globl  .returnto[PR]

	l	r1, topstack(a_area2)	# r1 <- a_area2->topstack
	cal	r1,  szdsa(r1)		# pop off frame
	l	r7, PRE_Block.S(toc)	# r7 <- &PRE_Block

	l	8, 8(1)			# restore lr
	mtlr    8			# do it early so prefetch works

	lm	12,  -8*nfprs-4*ngprs(r1)
	cal	r6, 0(r0)		# r6 <- #0
	mtcrf	0x38, 12		# put back cr
	stb	r6, 0(r7)		# r6 -> PRE_Block

/*
 * restore FPRs here!
 */
	lfd  14, -144(1)
	lfd  15, -136(1)
	lfd  16, -128(1)
	lfd  17, -120(1)
	lfd  18, -112(1)
	lfd  19, -104(1)
	lfd  20, -96(1)
	lfd  21, -88(1)
	lfd  22, -80(1)
	lfd  23, -72(1)
	lfd  24, -64(1)
	lfd  25, -56(1)
	lfd  26, -48(1)
	lfd  27, -40(1)
	lfd  28, -32(1)
	lfd  29, -24(1)
	lfd  30, -16(1)
	lfd  31, -8(1)

        brl				# pc <- lr	-- return

	.toc

PRE_Block.S:
	.tc	PRE_Block[tc], PRE_Block[ua]
	.extern	PRE_Block[ua]

