/* $Header: /home/cvs/src/usr.sbin/afs/src/lwp/Attic/process.S,v 1.1.1.1 1998/09/14 21:53:12 art Exp $ */
/* $Source: /home/cvs/src/usr.sbin/afs/src/lwp/Attic/process.S,v $ */

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

#if defined(RIOS)

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

#endif	/* RIOS	*/

#ifdef mc68000
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

	.globl	_PRE_Block

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

	.globl	_savecontext
_savecontext:
	movb	#1,_PRE_Block		| Dont allow any interrupt finagling
	link	a6,#-(nregs*4)		| Save frame pointer & ...
					| ... allocate space for nregs registers
/* Save registers */
	moveml	#regs,sp@

	movl	a6@(area1),a0		| a0 = base of savearea
	movl	sp,a0@(topstack)	| area->topstack = sp
	movl	a6@(newsp),d0		| Get new sp
	jeq	forw1			| If newsp == 0, no stack switch
	movl	d0,sp			| Switch to new stack
forw1:
	movl	a6@(f),a0		| a0 = f
	jbsr	a0@			| f()

/* It is impossible to be here, so abort() */

	jbsr	_abort

/*
# returnto(area2)
#     struct savearea *area2;
*/

/* Stack offset of argument */
area2	=	8

	.globl _returnto
_returnto:
	link	a6,#0
	movl	a6@(area2),a0		| Base of savearea
	movl	a0@(topstack),sp	| Restore sp
/* Restore registers */
	moveml	sp@,#regs

	addl	#(nregs*4),sp
	movl	sp,a6			| Argghh...be careful here
	unlk	a6
	clrb	_PRE_Block
	rts				| Return to previous process
#endif /* mc68000 */
#ifdef	sparc
#ifdef	AFS_SUN5_ENV
#include	<sys/asm_linkage.h>
#include  <sys/trap.h>
#else
#ifdef AFS_NETBSD_ENV
#include <machine/trap.h>
#define ST_FLUSH_WINDOWS ST_FLUSHWIN
#define MINFRAME 92
#define SA(x) (((x)+7)&~7)
#define STACK_ALIGN 8
#else 
#ifdef AFS_LINUX_ENV
#define AFS_SUN5_ENV 1          /* Make believe this is Solaris */ 
#define ST_FLUSH_WINDOWS 0x03   /* XXX:  from asm/traps.h */
#define MINFRAME 92
#define SA(x) (((x)+7)&~7)
#define STACK_ALIGN 8
#else /* SunOS 4: */
#include	<sun4/asm_linkage.h>
#include  <sun4/trap.h>
#endif
#endif
#endif
	.data	
#ifdef	AFS_SUN5_ENV
	.globl	PRE_Block
#else
	.globl	_PRE_Block
#endif
topstack	= 0
globals = 4
/*
# savecontext(f, area1, newsp)
#     int (*f)(); struct savearea *area1; char *newsp;
*/
	.text
#ifdef	AFS_SUN5_ENV
	.globl	savecontext
savecontext:
#else
	.globl	_savecontext
_savecontext:
#endif
	save	%sp, -SA(MINFRAME), %sp	! Get new window
	ta	ST_FLUSH_WINDOWS		! FLush all other active windows

	/* The following 3 lines do the equivalent of: _PRE_Block = 1 */
#ifdef	AFS_SUN5_ENV
	set	PRE_Block, %l0
#else
	set	_PRE_Block, %l0
#endif
	mov	1,%l1
	stb	%l1, [%l0]

	st	%fp,[%i1+topstack]		! area1->topstack = sp
	
	st	%g1, [%i1 + globals + 0]		/* Save all globals just in case */
	st	%g2, [%i1 + globals + 4]
	st	%g3, [%i1 + globals + 8]
	st	%g4, [%i1 + globals + 12]
	st	%g5, [%i1 + globals + 16]
	st	%g6, [%i1 + globals + 20]
	st	%g7, [%i1 + globals + 24]
	mov	%y, %g1
	st	%g1, [%i1 + globals + 28]

#ifdef	save_allregs
	st	%f0, [%i1 + globals + 32 + 0]		! Save all floating point registers 
	st	%f1, [%i1 + globals + 32 + 4]
	st	%f2, [%i1 + globals + 32 + 8]
	st	%f3, [%i1 + globals + 32 + 12]
	st	%f4, [%i1 + globals + 32 + 16]
	st	%f5, [%i1 + globals + 32 + 20]
	st	%f6, [%i1 + globals + 32 + 24]
	st	%f7, [%i1 + globals + 32 + 28]
	st	%f8, [%i1 + globals + 64 + 0]
	st	%f9, [%i1 + globals + 64 + 4]
	st	%f10, [%i1 + globals + 64 + 8]
	st	%f11, [%i1 + globals + 64 + 12]
	st	%f12, [%i1 + globals + 64 + 16]
	st	%f13, [%i1 + globals + 64 + 20]
	st	%f14, [%i1 + globals + 64 + 24]
	st	%f15, [%i1 + globals + 64 + 28]
	st	%f16, [%i1 + globals + 64 + 32]
	st	%f17, [%i1 + globals + 64 + 36]
	st	%f18, [%i1 + globals + 64 + 40]
	st	%f19, [%i1 + globals + 64 + 44]
	st	%f20, [%i1 + globals + 64 + 48]
	st	%f21, [%i1 + globals + 64 + 52]
	st	%f22, [%i1 + globals + 64 + 56]
	st	%f23, [%i1 + globals + 64 + 60]
	st	%f24, [%i1 + globals + 64 + 64]
	st	%f25, [%i1 + globals + 64 + 68]
	st	%f26, [%i1 + globals + 64 + 72]
	st	%f27, [%i1 + globals + 64 + 76]
	st	%f28, [%i1 + globals + 64 + 80]
	st	%f29, [%i1 + globals + 64 + 84]
	st	%f30, [%i1 + globals + 64 + 88]
	st	%f31, [%i1 + globals + 64 + 92]
#ifdef	notdef
	mov	%fsr,%g1
	st	%g1, [%i1 + globals + 64 + 96]
	mov	%fq,%g1
	st	%g1, [%i1 + globals + 64 + 100]
#endif

	st	%c0, [%i1 + globals + 168 + 0]			! Save all coprocessor registers 
	st	%c1, [%i1 + globals + 168 + 4]
	st	%c2, [%i1 + globals + 168 + 8]
	st	%c3, [%i1 + globals + 168 + 12]
	st	%c4, [%i1 + globals + 168 + 16]
	st	%c5, [%i1 + globals + 168 + 20]
	st	%c6, [%i1 + globals + 168 + 24]
	st	%c7, [%i1 + globals + 168 + 28]
	st	%c8, [%i1 + globals + 200 + 0]
	st	%c9, [%i1 + globals + 200 + 4]
	st	%c10, [%i1 + globals + 200 + 8]
	st	%c11, [%i1 + globals + 200 + 12]
	st	%c12, [%i1 + globals + 200 + 16]
	st	%c13, [%i1 + globals + 200 + 20]
	st	%c14, [%i1 + globals + 200 + 24]
	st	%c15, [%i1 + globals + 200 + 28]
	st	%c16, [%i1 + globals + 200 + 32]
	st	%c17, [%i1 + globals + 200 + 36]
	st	%c18, [%i1 + globals + 200 + 40]
	st	%c19, [%i1 + globals + 200 + 44]
	st	%c20, [%i1 + globals + 200 + 48]
	st	%c21, [%i1 + globals + 200 + 52]
	st	%c22, [%i1 + globals + 200 + 56]
	st	%c23, [%i1 + globals + 200 + 60]
	st	%c24, [%i1 + globals + 200 + 64]
	st	%c25, [%i1 + globals + 200 + 68]
	st	%c26, [%i1 + globals + 200 + 72]
	st	%c27, [%i1 + globals + 200 + 76]
	st	%c28, [%i1 + globals + 200 + 80]
	st	%c29, [%i1 + globals + 200 + 84]
	st	%c30, [%i1 + globals + 200 + 88]
	st	%c31, [%i1 + globals + 200 + 92]
#ifdef	notdef
	mov	%csr,%g1
	st	%g1, [%i1 + globals + 200 + 96]
	mov	%cq,%g1
	st	%g1, [%i1 + globals + 200 + 100]
#endif
#endif
	cmp	%i2, 0
	be,a	L1				! if (newsp == 0) no stack switch
	nop
#ifdef	notdef
	add	%i2, STACK_ALIGN - 1, %i2
	and	%i2, ~(STACK_ALIGN - 1), %i2
	sub	%i2, SA(MINFRAME), %fp
	call	%i0
	restore
#else
	! This used to compute a new stack frame base, write it into
	! FP, and restore to enter the new frame. But that left a window
	! in which FP could be written into the backing store for this
	! frame, to be tripped over later by returnto. So instead we do
	! the restore first, then modify SP to enter the new frame. We
	! can still refer to our argument as %02.
	restore
	add	%o2, STACK_ALIGN - 1, %o2
	and	%o2, ~(STACK_ALIGN - 1), %o2	
	call	%o0
	sub	%o2, SA(MINFRAME), %sp
#endif	

L1:	call	%i0			! call f()
	nop


! returnto(area1)
!     struct savearea *area1;
#ifdef	AFS_SUN5_ENV
	.globl returnto
returnto:
#else
	.globl _returnto
_returnto:
#endif
	ta	ST_FLUSH_WINDOWS		! FLush all other active windows
	ld	[%o0+topstack],%g1		! sp = area1->topstack
	sub	%g1, SA(MINFRAME), %fp	! Adjust sp to the right place
	sub	%fp, SA(MINFRAME), %sp

#ifdef	save_allregs
	ld	[%o0 + globals + 32 + 0],%f0		! Restore floating-point registers 
	ld	[%o0 + globals + 32 + 4],%f1
	ld	[%o0 + globals + 32 + 8],%f2
	ld	[%o0 + globals + 32 + 12],%f3
	ld	[%o0 + globals + 32 + 16],%f4
	ld	[%o0 + globals + 32 + 20],%f5
	ld	[%o0 + globals + 32 + 24],%f6
	ld	[%o0 + globals + 32 + 28],%f7
	ld	[%o0 + globals + 64 + 0],%f8
	ld	[%o0 + globals + 64 + 4],%f9
	ld	[%o0 + globals + 64 + 8],%f10
	ld	[%o0 + globals + 64 + 12],%f11
	ld	[%o0 + globals + 64 + 16],%f12
	ld	[%o0 + globals + 64 + 20],%f13
	ld	[%o0 + globals + 64 + 24],%f14
	ld	[%o0 + globals + 64 + 28],%f15
	ld	[%o0 + globals + 64 + 32],%f16
	ld	[%o0 + globals + 64 + 36],%f17
	ld	[%o0 + globals + 64 + 40],%f18
	ld	[%o0 + globals + 64 + 44],%f19
	ld	[%o0 + globals + 64 + 48],%f20
	ld	[%o0 + globals + 64 + 52],%f21
	ld	[%o0 + globals + 64 + 56],%f22
	ld	[%o0 + globals + 64 + 60],%f23
	ld	[%o0 + globals + 64 + 64],%f24
	ld	[%o0 + globals + 64 + 68],%f25
	ld	[%o0 + globals + 64 + 72],%f26
	ld	[%o0 + globals + 64 + 76],%f27
	ld	[%o0 + globals + 64 + 80],%f28
	ld	[%o0 + globals + 64 + 84],%f29
	ld	[%o0 + globals + 64 + 88],%f30
	ld	[%o0 + globals + 64 + 92],%f31
#ifdef	notdef
	ld	[%o0 + globals + 64 + 96],%g1
	mov	%g1, %fsr
	ld	[%o0 + globals + 64 + 100],%g1
	mov	%g1, %fq
#endif

	ld	[%o0 + globals + 168 + 0],%c0		! Restore floating-point registers 
	ld	[%o0 + globals + 168 + 4],%c1
	ld	[%o0 + globals + 168 + 8],%c2
	ld	[%o0 + globals + 168 + 12],%c3
	ld	[%o0 + globals + 168 + 16],%c4
	ld	[%o0 + globals + 168 + 20],%c5
	ld	[%o0 + globals + 168 + 24],%c6
	ld	[%o0 + globals + 168 + 28],%c7
	ld	[%o0 + globals + 200 + 0],%c8
	ld	[%o0 + globals + 200 + 4],%c9
	ld	[%o0 + globals + 200 + 8],%c10
	ld	[%o0 + globals + 200 + 12],%c11
	ld	[%o0 + globals + 200 + 16],%c12
	ld	[%o0 + globals + 200 + 20],%c13
	ld	[%o0 + globals + 200 + 24],%c14
	ld	[%o0 + globals + 200 + 28],%c15
	ld	[%o0 + globals + 200 + 32],%c16
	ld	[%o0 + globals + 200 + 36],%c17
	ld	[%o0 + globals + 200 + 40],%c18
	ld	[%o0 + globals + 200 + 44],%c19
	ld	[%o0 + globals + 200 + 48],%c20
	ld	[%o0 + globals + 200 + 52],%c21
	ld	[%o0 + globals + 200 + 56],%c22
	ld	[%o0 + globals + 200 + 60],%c23
	ld	[%o0 + globals + 200 + 64],%c24
	ld	[%o0 + globals + 200 + 68],%c25
	ld	[%o0 + globals + 200 + 72],%c26
	ld	[%o0 + globals + 200 + 76],%c27
	ld	[%o0 + globals + 200 + 80],%c28
	ld	[%o0 + globals + 200 + 84],%c29
	ld	[%o0 + globals + 200 + 88],%c30
	ld	[%o0 + globals + 200 + 92],%c31
#ifdef	notdef
	ld	[%o0 + globals + 200 + 96],%g1
	mov	%g1, %csr
	ld	[%o0 + globals + 200 + 100],%g1
	mov	%g1, %cq
#endif
#endif
	ld	[%o0 + globals + 28], %g1		! Restore global regs back
	mov	%g1, %y
	ld	[%o0 + globals + 0], %g1
	ld	[%o0 + globals + 4], %g2
	ld	[%o0 + globals + 8], %g3
	ld	[%o0 + globals + 12],%g4
	ld	[%o0 + globals + 16],%g5
	ld	[%o0 + globals + 20],%g6
	ld	[%o0 + globals + 24],%g7

	/* The following 3 lines are equivalent to: _PRE_Block = 0 */
#ifdef	AFS_SUN5_ENV
	set	PRE_Block, %l0
#else
	set	_PRE_Block, %l0
#endif
	mov	0,%l1
	stb	%l1, [%l0]

	restore					
	restore

	retl
	nop

#endif /* sparc */
#ifdef ibm032
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
	putc	r6,0(r7)			| PRE_Block = 1
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
	putc	r6,0(r7)			| PRE_Block = 0
	lm	r0,0(sp)		| Change if saving fewer regs.
	brx	r15		| Return to previous process
	ai	sp,sp,regspace
 .data
 .ltorg
#endif

#ifdef AFS_AIX22_ENV
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
	stc	6,0(7)
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
	stc	6,0(7)
	lm	0,0(1)		# Change if saving fewer regs.
	brx	15		# Return to previous process
	ai	1,1,regspace
	.data	3
	.globl	_returnto
_returnto:
	.long	.returnto
	.long	_PRE_Block
#endif /* AFS_AIX_ENV */

#ifdef vax
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
#endif

#ifdef mips
#ifdef	sgi
	.option	pic2

#include <regdef.h> /* Allow use of symbolic names for registers. */
/* 9 sregs, ra, 6 fp regs, gp, pad to 8 byte boundary */
#define regspace 9 * 4 + 4 + 6 * 8 + 4 + 4
#define floats 0
#define registers floats + 6 * 8
#define returnaddr regspace - 4
#define topstack 0
#define GPOFF	regspace - 8
	.globl savecontext /* MIPS' C compiler doesnt prepend underscores. */
	.ent savecontext /* Insert debugger information. */
savecontext:
        .set    noreorder
        .cpload t9                      # set up gp for KPIC
        .set    reorder
        subu sp, regspace
        .cprestore GPOFF                # trigger t9/jalr
	.set	noreorder
	li	t0, 1
	.extern	PRE_Block
	sb	t0, PRE_Block
        .set    reorder
	.frame	sp, regspace, ra
/* Save registers. */
	sw	s0, registers + 0(sp)
	sw	s1, registers + 4(sp)
	sw	s2, registers + 8(sp)
	sw	s3, registers + 12(sp)
	sw	s4, registers + 16(sp)
	sw	s5, registers + 20(sp)
	sw	s6, registers + 24(sp)
	sw	s7, registers + 28(sp)
	sw	s8, registers + 32(sp)
/* Save return address */
	sw	ra, returnaddr(sp)
	.mask	0xc0ff0000, -4
/* Need to save floating point registers? */
	s.d	$f20, floats + 0(sp)
	s.d	$f22, floats + 8(sp)
	s.d	$f24, floats + 16(sp)
	s.d	$f26, floats + 24(sp)
	s.d	$f28, floats + 32(sp)
	s.d	$f30, floats + 40(sp)
	.fmask	0x55400000, regspace
	sw	sp, topstack(a1)
	beq	a2, $0, samestack
	move	sp, a2
samestack:
	move	t9, a0
	j	t9
	.end	savecontext

	.globl	returnto
	.ent	returnto
returnto:
        .set    noreorder
        .cpload t9                      # set up gp for KPIC
        .set    reorder

	lw	sp, topstack(a0)
	lw	s0, registers + 0(sp)
	lw	s1, registers + 4(sp)
	lw	s2, registers + 8(sp)
	lw	s3, registers + 12(sp)
	lw	s4, registers + 16(sp)
	lw	s5, registers + 20(sp)
	lw	s6, registers + 24(sp)
	lw	s7, registers + 28(sp)
	lw	s8, registers + 32(sp)
/* Save return address */
	lw	ra, returnaddr(sp)
/* Need to save floating point registers? */
	l.d	$f20, floats + 0(sp)
	l.d	$f22, floats + 8(sp)
	l.d	$f24, floats + 16(sp)
	l.d	$f26, floats + 24(sp)
	l.d	$f28, floats + 32(sp)
	l.d	$f30, floats + 40(sp)
	.set	noreorder
	addu	sp, regspace
	la	t0, PRE_Block
	j	ra
	sb	zero, 0(t0)
	.set	reorder
	.end	returnto

#else
/* Code for MIPS R2000/R3000 architecture
 * Written by Zalman Stern April 30th, 1989.
 */
#include <regdef.h> /* Allow use of symbolic names for registers. */
#define regspace 9 * 4 + 4 + 6 * 8
#define floats 0
#define registers floats + 6 * 8
#define returnaddr regspace - 4
#define topstack 0
	.globl savecontext /* MIPS' C compiler doesnt prepend underscores. */
	.ent savecontext /* Insert debugger information. */
savecontext:
	li	t0, 1
	.extern	PRE_Block
	sb	t0, PRE_Block
	subu	sp, regspace
	.frame	sp, regspace, ra
/* Save registers. */
	sw	s0, registers + 0(sp)
	sw	s1, registers + 4(sp)
	sw	s2, registers + 8(sp)
	sw	s3, registers + 12(sp)
	sw	s4, registers + 16(sp)
	sw	s5, registers + 20(sp)
	sw	s6, registers + 24(sp)
	sw	s7, registers + 28(sp)
	sw	s8, registers + 32(sp)
/* Save return address */
	sw	ra, returnaddr(sp)
	.mask	0xc0ff0000, -4
/* Need to save floating point registers? */
	s.d	$f20, floats + 0(sp)
	s.d	$f22, floats + 8(sp)
	s.d	$f24, floats + 16(sp)
	s.d	$f26, floats + 24(sp)
	s.d	$f28, floats + 32(sp)
	s.d	$f30, floats + 40(sp)
	.fmask	0x55400000, regspace
	sw	sp, topstack(a1)
	beq	a2, $0, samestack
	addu	sp, $0, a2
samestack:
	jal	a0
	.end	savecontext

	.globl	returnto
	.ent	returnto
returnto:
	lw	sp, topstack(a0)
	lw	s0, registers + 0(sp)
	lw	s1, registers + 4(sp)
	lw	s2, registers + 8(sp)
	lw	s3, registers + 12(sp)
	lw	s4, registers + 16(sp)
	lw	s5, registers + 20(sp)
	lw	s6, registers + 24(sp)
	lw	s7, registers + 28(sp)
	lw	s8, registers + 32(sp)
/* Save return address */
	lw	ra, returnaddr(sp)
/* Need to save floating point registers? */
	l.d	$f20, floats + 0(sp)
	l.d	$f22, floats + 8(sp)
	l.d	$f24, floats + 16(sp)
	l.d	$f26, floats + 24(sp)
	l.d	$f28, floats + 32(sp)
	l.d	$f30, floats + 40(sp)
	addu	sp, regspace
	sb	$0, PRE_Block
	j	ra
	.end	returnto
#endif	/* sgi */
#endif

#ifdef AFS_HPUX_ENV
#include "process.s.hpux"
#endif /* AFS_HPUX_ENV */

#if defined(AFS_386i_ENV)
/* i386 Assembly
 *
 * Written by Derek Atkins <warlord@MIT.EDU>
 * (debugging help by Chris Provenzano <proven@mit.edu>)
 * 11/1991
 * +  * "ojala que sea correcto!"
 */
         
#if !defined(SYSV) && !defined(__ELF__) && !defined(AFS_SUN5_ENV)
#ifdef __STDC__
#define SYMBOL(name)     _##name
#define ENTRY(name)    _##name##:
#else
#define SYMBOL(name)  _/**/name
#define ENTRY(name)     _/**/name/**/:
#endif
#else /* SYSV || __ELF__ || AFS_SUN5_ENV */
#define SYMBOL(name)  name
#define ENTRY(name)     name:
#endif
 
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

.globl		SYMBOL(PRE_Block)
.globl		SYMBOL(savecontext)

ENTRY(savecontext)
	pushl   %ebp                    /* New Frame! */
	movl    %esp,%ebp
	pushal                           /* Push all registers */
	movl    $1,SYMBOL(PRE_Block)        /* Do not allow any interrupt finagling */
	movl    area1(%ebp),%eax        /* eax = base of savearea */
	movl    %esp,topstack(%eax)	/* area->topstack = esp */
	movl    newsp(%ebp),%eax        /* get new sp into eax */
	cmpl    $0,%eax
	je      L1                      /* if new sp is 0 then dont change esp */
	movl    %eax,%esp               /* go ahead.  make my day! */
L1:
	jmp     *f(%ebp)                        /* ebx = &f */

/* Shouldnt be here....*/

	call    SYMBOL(abort)

/*
 * returnto(area2)
 *    struct savearea *area2;
 */

/* stack offset */
	.set    area2,8

.globl		SYMBOL(returnto)

ENTRY(returnto)
	pushl   %ebp
	movl    %esp, %ebp              /* New frame, to get correct pointer */
	movl    area2(%ebp),%eax        /* eax = area2 */
	movl    topstack(%eax),%esp	/* restore esp */
	popal
	movl    $0,SYMBOL(PRE_Block)            /* clear it up... */
	popl    %ebp
	ret
	
/* I see, said the blind man, as he picked up his hammer and saw! */
	pushl   $1234
	call    SYMBOL(abort)

#endif /* AFS_386i_ENV */

#ifdef __alpha
/* Code for DEC Alpha architecture */
#ifdef	AFS_OSF_ENV
#include <machine/asm.h>
#include <machine/regdef.h>
#define	fs0	$f2
#define	fs1	$f3
#define	fs2	$f4
#define	fs3	$f5
#define	fs4	$f6
#define	fs5	$f7
#define	fs6	$f8
#define	fs7	$f9
#elif AFS_LINUX_ENV
#define v0	$0
#define t0	$1
#define t1	$2		
#define t2	$3		
#define t3	$4		
#define t4	$5		
#define t5	$6		
#define t6	$7		
#define t7	$8		

#define s0      $9
#define s1	$10
#define s2	$11
#define s3	$12
#define s4	$13
#define s5	$14
#define s6	$15
	
#define fp	$15
#define a0	$16
#define a1	$17
#define a2	$18
#define a3	$19
#define a4	$20
#define a5	$21
#define t8	$22
#define t9	$23
#define t10	$24	
#define t11	$25
#define ra	$26
#define pv      $27
#define t12	$27	
#define at	$28
#define gp	$29
#define sp	$30
#define zero	$31

#define fs0     $f2
#define fs1     $f3
#define fs2     $f4
#define fs3     $f5
#define fs4     $f6
#define fs5     $f7
#define fs6     $f8
#define fs7     $f9

#define M_S0            0x00000200
#define M_S1            0x00000400
#define M_S2            0x00000800
#define M_S3            0x00001000
#define M_S4            0x00002000
#define M_S5            0x00004000
#define M_S6            0x00008000

#define  M_RA           0x04000000

#define END(proc)                                       \
	        .end    proc

#define NESTED(x, fsize, rpc)                           \
	        .globl  x	;                       \
	        .ent    x,0	;                       \
x:				;                       \
	        .frame  sp,fsize, rpc

	
#define LEAF(x)                                         \
	        .globl  x	;                       \
	        .ent    x,0	;                       \
x:				;                       \
	        .frame  sp,0,ra

#define IMPORT(sym, size)                               \
	        .extern sym,size
#elif AFS_NETBSD_ENV
#include <machine/asm.h>
#else	/* OSF */
#include <mach/alpha/asm.h>
#endif	/* OSF */

#define FRAMESIZE ((8*8)+8+(7*8))
#define floats 0
#define registers (floats+(8*8))
#define returnaddr (FRAMESIZE-8)
#define topstack 0

#if defined(AFS_OSF_ENV) || defined(AFS_LINUX_ENV)
	IMPORT(PRE_Block,4)
#endif		
.align	4
#if defined(AFS_OSF_ENV) || defined(AFS_LINUX_ENV)
NESTED(savecontext,FRAMESIZE,ra)
#else	/* OSF || LINUX */
NESTED(savecontext,3,FRAMESIZE,ra,0x0400f700,0x000003fc)
#endif	/* OSF */
	ldgp	gp,0(pv)
	lda	t0, 1(zero)
	stl	t0, PRE_Block
	lda	sp,-FRAMESIZE(sp)
/* Save callee-saved registers. */
	stq	s0, (registers+0) (sp)
	stq	s1, (registers+8) (sp)
	stq	s2, (registers+16) (sp)
	stq	s3, (registers+24) (sp)
	stq	s4, (registers+32) (sp)
	stq	s5, (registers+40) (sp)
	stq	s6, (registers+48) (sp)
/* Save return address */
	stq	ra, returnaddr(sp)

#if !defined(AFS_NETBSD_ENV)
	.mask	(M_S0|M_S1|M_S2|M_S3|M_S4|M_S5|M_S6|M_RA), -FRAMESIZE
#endif

/* Save floating point registers */
	stt	fs0, (floats+0) (sp)
	stt	fs1, (floats+8) (sp)
	stt	fs2, (floats+16) (sp)
	stt	fs3, (floats+24) (sp)
	stt	fs4, (floats+32) (sp)
	stt	fs5, (floats+40) (sp)
	stt	fs6, (floats+48) (sp)
	stt	fs7, (floats+56) (sp)

	.prologue	1
	stq	sp, topstack(a1)
	or	a0,zero,pv		/* call point in pv */
	beq	a2, samestack
	or	a2,zero,sp		/* switch stack */
samestack:
	jsr	ra,(pv),0		/* off we go */
	END(savecontext)

#if defined(AFS_OSF_ENV) || defined(AFS_LINUX_ENV)
LEAF(returnto)
#else	
LEAF(returnto,1)
#endif	
	ldgp	gp,0(pv)

	.prologue	1
	ldq	sp, topstack(a0)
/* Restore callee-saved regs */
	ldq	s0, (registers+0) (sp)
	ldq	s1, (registers+8) (sp)
	ldq	s2, (registers+16) (sp)
	ldq	s3, (registers+24) (sp)
	ldq	s4, (registers+32) (sp)
	ldq	s5, (registers+40) (sp)
	ldq	s6, (registers+48) (sp)
/* Return address */
	ldq	ra, returnaddr(sp)
/* Floating point registers */
	ldt	fs0, (floats+0) (sp)
	ldt	fs1, (floats+8) (sp)
	ldt	fs2, (floats+16) (sp)
	ldt	fs3, (floats+24) (sp)
	ldt	fs4, (floats+32) (sp)
	ldt	fs5, (floats+40) (sp)
	ldt	fs6, (floats+48) (sp)
	ldt	fs7, (floats+56) (sp)
	lda	sp, FRAMESIZE(sp)
	stl	zero, PRE_Block
	RET
	END(returnto)
#endif


#if defined(__powerpc__)

/* Comments:
 *    1. Registers R10..R31 and CR0..CR7 are saved
 *    2. "struct savearea" must hold at least 3 pointers (long)
 *    3. This code will only work on 32 bit machines (601..604), not 620
 *    4. No floating point registers are saved
 *    5. The save stack "frame" is bigger than absolutely necessary.  The
 *       PowerPC [AIX] ABI needs this extra space.
 */


/* Mach-O assemblers */
#if !defined(NeXT) && !defined(__APPLE__)
#define r0    0
#define r1    1
#define r2    2
#define r3    3
#define r4    4
#define r5    5
#define r6    6
#define r7    7
#define r8    8
#define r9    9
#define r10   10
#define r11   11
#define r12   12
#define r13   13
#define r14   14
#define r15   15
#define r16   16
#define r17   17
#define r18   18
#define r19   19
#define r20   20
#define r21   21
#define r22   22
#define r23   23
#define r24   24
#define r25   25
#define r26   26
#define r27   27
#define r28   28
#define r29   29
#define r30   30
#define r31   31
#endif /* !NeXT && !__APPLE__ */


/*
 * savecontext(int (*f)(), struct savearea *save, char *newsp)
 */

#define FRAME_SIZE    (32*4)+(8*4)
#define FRAME_OFFSET  (8*4)

#define TOP_OF_STACK  (0*4)
#define RETURN                (1*4)
#define CCR           (2*4)

#if defined(NeXT) || defined(__APPLE__)
      .globl  _savecontext
_savecontext:
      lis     r9,ha16(_PRE_Block)     /* Disable interrupt fiddling */
      li      r8,1
      stb     r8,lo16(_PRE_Block)(r9)
#else	
      .globl  savecontext
savecontext:
      lis     r9,PRE_Block@ha         /* Disable interrupt fiddling */
      li      r8,1
      stb     r8,PRE_Block@l(r9)
#endif /* NeXT || __APPLE__ */
      subi    r1,r1,FRAME_SIZE
      mfcr    r9
      stw     r9,CCR(r4)
      stw     r10,10*4+FRAME_OFFSET(r1)       /* Save registers */
      stw     r11,11*4+FRAME_OFFSET(r1)
      stw     r12,12*4+FRAME_OFFSET(r1)
      stw     r13,13*4+FRAME_OFFSET(r1)
      stw     r14,14*4+FRAME_OFFSET(r1)
      stw     r15,15*4+FRAME_OFFSET(r1)
      stw     r16,16*4+FRAME_OFFSET(r1)
      stw     r17,17*4+FRAME_OFFSET(r1)
      stw     r18,18*4+FRAME_OFFSET(r1)
      stw     r19,19*4+FRAME_OFFSET(r1)
      stw     r20,20*4+FRAME_OFFSET(r1)
      stw     r21,21*4+FRAME_OFFSET(r1)
      stw     r22,22*4+FRAME_OFFSET(r1)
      stw     r23,23*4+FRAME_OFFSET(r1)
      stw     r24,24*4+FRAME_OFFSET(r1)
      stw     r25,25*4+FRAME_OFFSET(r1)
      stw     r26,26*4+FRAME_OFFSET(r1)
      stw     r27,27*4+FRAME_OFFSET(r1)
      stw     r28,28*4+FRAME_OFFSET(r1)
      stw     r29,29*4+FRAME_OFFSET(r1)
      stw     r30,30*4+FRAME_OFFSET(r1)
      stw     r31,31*4+FRAME_OFFSET(r1)
      stw     r1,TOP_OF_STACK(r4)
      cmpi    0,r5,0                          /* New stack specified? */
      mflr    r0
      stw     r0,RETURN(r4)
      mtlr    r3
      beq     L1                             /* No - don't muck with pointer */

      mr      r1,r5
L1:   blr                                     /* Return */

/*
 * returnto(struct savearea *area)
 */
#if defined(NeXT) || defined(__APPLE__)
      .globl  _returnto
_returnto:
#else	
      .globl  returnto
returnto:
#endif /* NeXT || __APPLE__ */
      lwz     r1,TOP_OF_STACK(r3)             /* Update stack pointer */
      lwz     r0,RETURN(r3)                   /* Get return address */
      mtlr    r0
      lwz     r4,CCR(r3)
      mtcrf   0xFF,r4
      lwz     r10,10*4+FRAME_OFFSET(r1)       /* Restore registers */
      lwz     r11,11*4+FRAME_OFFSET(r1)
      lwz     r12,12*4+FRAME_OFFSET(r1)
      lwz     r13,13*4+FRAME_OFFSET(r1)
      lwz     r14,14*4+FRAME_OFFSET(r1)
      lwz     r15,15*4+FRAME_OFFSET(r1)
      lwz     r16,16*4+FRAME_OFFSET(r1)
      lwz     r17,17*4+FRAME_OFFSET(r1)
      lwz     r18,18*4+FRAME_OFFSET(r1)
      lwz     r19,19*4+FRAME_OFFSET(r1)
      lwz     r20,20*4+FRAME_OFFSET(r1)
      lwz     r21,21*4+FRAME_OFFSET(r1)
      lwz     r22,22*4+FRAME_OFFSET(r1)
      lwz     r23,23*4+FRAME_OFFSET(r1)
      lwz     r24,24*4+FRAME_OFFSET(r1)
      lwz     r25,25*4+FRAME_OFFSET(r1)
      lwz     r26,26*4+FRAME_OFFSET(r1)
      lwz     r27,27*4+FRAME_OFFSET(r1)
      lwz     r28,28*4+FRAME_OFFSET(r1)
      lwz     r29,29*4+FRAME_OFFSET(r1)
      lwz     r30,30*4+FRAME_OFFSET(r1)
      lwz     r31,31*4+FRAME_OFFSET(r1)
#if defined(NeXT) || defined(__APPLE__)
      lis     r9,ha16(_PRE_Block)         /* Re-enable interrupt fiddling */
      li      r8,0
      stb     r8,lo16(_PRE_Block)(r9)
#else	
      lis     r9,PRE_Block@ha         /* Re-enable interrupt fiddling */
      li      r8,0
      stb     r8,PRE_Block@l(r9)
#endif /* NeXT || __APPLE__ */
      addi    r1,r1,FRAME_SIZE
      blr
#endif        /* __powerpc__ */

