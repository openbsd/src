/* $KTH: process.sparc.S,v 1.2.4.1 2002/08/13 22:47:40 lha Exp $ */

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

#if defined(__sparc64__) && !defined(__sparcv9)
#define __sparcv9 1
#endif


#if defined(AFS_SUN5_ENV)
#include <sys/asm_linkage.h>
#include <sys/trap.h>
#elif defined(AFS_BSD_ENV)
#ifdef HAVE_MACHINE_ASM_H
#include <machine/asm.h>
#endif
#include <machine/trap.h>
#define ST_FLUSH_WINDOWS ST_FLUSHWIN
#elif defined(AFS_LINUX_ENV)
#define ST_FLUSH_WINDOWS 0x03   /* XXX:  from asm/traps.h */
#else				/* SunOS4 */
#include <sun4/asm_linkage.h>
#include <sun4/trap.h>
#endif

#ifdef __sparcv9
	
#ifndef STACK_BIAS
#define STACK_BIAS      (2048-1)
#endif
#ifndef STACK_ALIGN	
#define STACK_ALIGN	8
#endif
#ifndef MINFRAME
#define MINFRAME	176
#endif
	
#else /* !__sparcv9 */
	
#ifndef STACK_BIAS
#define STACK_BIAS      0
#endif
#ifndef STACK_ALIGN	
#define STACK_ALIGN	4
#endif
#ifndef MINFRAME
#define MINFRAME	96
#endif
	
#endif /* __sparcv9 */
	
#ifndef SA
#define SA(x)		(((x)+(STACK_ALIGN-1))& ~(STACK_ALIGN-1))
#endif

#include <lwp_elf.h>

	.data	
	.globl	_C_LABEL(PRE_Block)
/*
# savecontext(f, area1, newsp)
#     int (*f)(); struct savearea *area1; char *newsp;
*/
	
	.text
	.globl	_C_LABEL(savecontext)
ENTRY(savecontext)
	save	%sp, -SA(MINFRAME), %sp	! Get new window
	ta	ST_FLUSH_WINDOWS		! FLush all other active windows

	/* The following 3 lines do the equivalent of: _PRE_Block = 1 */
#ifdef	__sparcv9	
	sethi	%hh(PRE_Block),%l0
	or	%l0,%hm(PRE_Block),%l0
	sethi	%lm(PRE_Block),%g1
	or	%g1,%lo(PRE_Block),%g1
	sllx	%l0,32,%l0
	or	%l0,%g1,%l0
#else	
	sethi	%hi(PRE_Block),%l0
	or	%l0,%lo(_C_LABEL(PRE_Block)),%l0
#endif
	mov	1,%l1
	stb	%l1, [%l0]

#ifdef	__sparcv9
	
topstack	= 0
globals = 8

	/* These declarations are needed if you're running Solaris 7,
	 * and are compiling with -xarch=v9 and have an as from WorkShop
	 * Compilers 5.0 98/12/21 (or more recent).  Hopefully,
	 * AS_UNDERSTANDS_REGISTER will be set correctly by configure, if
	 * that's not the case, edit here and send a bug report.
	 */

#if AS_UNDERSTANDS_REGISTER
	.register	%g2, #scratch
	.register	%g3, #scratch
	.register	%g6, #scratch
	.register	%g7, #scratch
#endif

	stx	%fp,[%i1+topstack]		! area1->topstack = sp
	
	stx	%g1, [%i1 + globals + 0]		/* Save all globals just in case */
	stx	%g2, [%i1 + globals + 8]
	stx	%g3, [%i1 + globals + 16]
	stx	%g4, [%i1 + globals + 24]
	stx	%g5, [%i1 + globals + 32]
	stx	%g6, [%i1 + globals + 40]
	stx	%g7, [%i1 + globals + 48]
	mov	%y, %g1
	stx	%g1, [%i1 + globals + 56]

#ifdef	save_allregs
	stx	%f0,  [%i1 + globals + 64 + 0]
	stx	%f1,  [%i1 + globals + 64 + 8]
	stx	%f2,  [%i1 + globals + 64 + 16]
	stx	%f3,  [%i1 + globals + 64 + 24]
	stx	%f4,  [%i1 + globals + 64 + 32]
	stx	%f5,  [%i1 + globals + 64 + 40]
	stx	%f6,  [%i1 + globals + 64 + 48]
	stx	%f7,  [%i1 + globals + 64 + 56]
	stx	%f8,  [%i1 + globals + 64 + 64]
	stx	%f9,  [%i1 + globals + 64 + 72]
	stx	%f10, [%i1 + globals + 64 + 80]
	stx	%f11, [%i1 + globals + 64 + 88]
	stx	%f12, [%i1 + globals + 64 + 96]
	stx	%f13, [%i1 + globals + 64 + 104]
	stx	%f14, [%i1 + globals + 64 + 112]
	stx	%f15, [%i1 + globals + 64 + 120]
	stx	%f16, [%i1 + globals + 64 + 128]
	stx	%f17, [%i1 + globals + 64 + 136]
	stx	%f18, [%i1 + globals + 64 + 144]
	stx	%f19, [%i1 + globals + 64 + 152]
	stx	%f20, [%i1 + globals + 64 + 160]
	stx	%f21, [%i1 + globals + 64 + 168]
	stx	%f22, [%i1 + globals + 64 + 176]
	stx	%f23, [%i1 + globals + 64 + 184]
	stx	%f24, [%i1 + globals + 64 + 192]
	stx	%f25, [%i1 + globals + 64 + 200]
	stx	%f26, [%i1 + globals + 64 + 208]
	stx	%f27, [%i1 + globals + 64 + 216]
	stx	%f28, [%i1 + globals + 64 + 224]
	stx	%f29, [%i1 + globals + 64 + 232]
	stx	%f30, [%i1 + globals + 64 + 240]
	stx	%f31, [%i1 + globals + 64 + 248]
	stx	%f32, [%i1 + globals + 64 + 256]
	stx	%f33, [%i1 + globals + 64 + 264]
	stx	%f34, [%i1 + globals + 64 + 272]
	stx	%f35, [%i1 + globals + 64 + 280]
	stx	%f36, [%i1 + globals + 64 + 288]
	stx	%f37, [%i1 + globals + 64 + 296]
	stx	%f38, [%i1 + globals + 64 + 304]
	stx	%f39, [%i1 + globals + 64 + 312]
	stx	%f40, [%i1 + globals + 64 + 320]
	stx	%f41, [%i1 + globals + 64 + 328]
	stx	%f42, [%i1 + globals + 64 + 336]
	stx	%f43, [%i1 + globals + 64 + 344]
	stx	%f44, [%i1 + globals + 64 + 352]
	stx	%f45, [%i1 + globals + 64 + 360]
	stx	%f46, [%i1 + globals + 64 + 368]
	stx	%f47, [%i1 + globals + 64 + 376]
	stx	%f48, [%i1 + globals + 64 + 384]
	stx	%f49, [%i1 + globals + 64 + 392]
	stx	%f50, [%i1 + globals + 64 + 400]
	stx	%f51, [%i1 + globals + 64 + 408]
	stx	%f52, [%i1 + globals + 64 + 416]
	stx	%f53, [%i1 + globals + 64 + 424]
	stx	%f54, [%i1 + globals + 64 + 432]
	stx	%f55, [%i1 + globals + 64 + 440]
	stx	%f56, [%i1 + globals + 64 + 448]
	stx	%f57, [%i1 + globals + 64 + 456]
	stx	%f59, [%i1 + globals + 64 + 464]
	stx	%f60, [%i1 + globals + 64 + 472]
	stx	%f61, [%i1 + globals + 64 + 480]
#ifdef	notdef
	mov	%fsr,%g1
	stx	%g1, [%i1 + globals + 64 + 488]
	mov	%fq,%g1
	stx	%g1, [%i1 + globals + 64 + 496]
#endif
	
#endif	
	
#else	/* !__sparcv9 */

topstack	= 0
globals = 4
	
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
	
#endif  /* __sparcv9 */

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
	sub	%o2, SA(MINFRAME) + STACK_BIAS, %sp
#endif	

L1:	call	%i0			! call f()
	nop


! returnto(area1)
!     struct savearea *area1;
	.globl _C_LABEL(returnto)
ENTRY(returnto)
	ta	ST_FLUSH_WINDOWS		! FLush all other active windows

#ifdef	__sparcv9

#ifdef	save_allregs
	ldx	[%i1 + globals + 64 + 0], %f0
	ldx	[%i1 + globals + 64 + 8], %f1
	ldx	[%i1 + globals + 64 + 16], %f2
	ldx	[%i1 + globals + 64 + 24], %f3
	ldx	[%i1 + globals + 64 + 32], %f4
	ldx	[%i1 + globals + 64 + 40], %f5
	ldx	[%i1 + globals + 64 + 48], %f6
	ldx	[%i1 + globals + 64 + 56], %f7
	ldx	[%i1 + globals + 64 + 64], %f8
	ldx	[%i1 + globals + 64 + 72], %f9
	ldx	[%i1 + globals + 64 + 80], %f10
	ldx	[%i1 + globals + 64 + 88], %f11
	ldx	[%i1 + globals + 64 + 96], %f12
	ldx	[%i1 + globals + 64 + 104], %f13
	ldx	[%i1 + globals + 64 + 112], %f14
	ldx	[%i1 + globals + 64 + 120], %f15
	ldx	[%i1 + globals + 64 + 128], %f16
	ldx	[%i1 + globals + 64 + 136], %f17
	ldx	[%i1 + globals + 64 + 144], %f18
	ldx	[%i1 + globals + 64 + 152], %f19
	ldx	[%i1 + globals + 64 + 160], %f20
	ldx	[%i1 + globals + 64 + 168], %f21
	ldx	[%i1 + globals + 64 + 176], %f22
	ldx	[%i1 + globals + 64 + 184], %f23
	ldx	[%i1 + globals + 64 + 192], %f24
	ldx	[%i1 + globals + 64 + 200], %f25
	ldx	[%i1 + globals + 64 + 208], %f26
	ldx	[%i1 + globals + 64 + 216], %f27
	ldx	[%i1 + globals + 64 + 224], %f28
	ldx	[%i1 + globals + 64 + 232], %f29
	ldx	[%i1 + globals + 64 + 240], %f30
	ldx	[%i1 + globals + 64 + 248], %f31
	ldx	[%i1 + globals + 64 + 256], %f32
	ldx	[%i1 + globals + 64 + 264], %f33
	ldx	[%i1 + globals + 64 + 272], %f34
	ldx	[%i1 + globals + 64 + 280], %f35
	ldx	[%i1 + globals + 64 + 288], %f36
	ldx	[%i1 + globals + 64 + 296], %f37
	ldx	[%i1 + globals + 64 + 304], %f38
	ldx	[%i1 + globals + 64 + 312], %f39
	ldx	[%i1 + globals + 64 + 320], %f40
	ldx	[%i1 + globals + 64 + 328], %f41
	ldx	[%i1 + globals + 64 + 336], %f42
	ldx	[%i1 + globals + 64 + 344], %f43
	ldx	[%i1 + globals + 64 + 352], %f44
	ldx	[%i1 + globals + 64 + 360], %f45
	ldx	[%i1 + globals + 64 + 368], %f46
	ldx	[%i1 + globals + 64 + 376], %f47
	ldx	[%i1 + globals + 64 + 384], %f48
	ldx	[%i1 + globals + 64 + 392], %f49
	ldx	[%i1 + globals + 64 + 400], %f50
	ldx	[%i1 + globals + 64 + 408], %f51
	ldx	[%i1 + globals + 64 + 416], %f52
	ldx	[%i1 + globals + 64 + 424], %f53
	ldx	[%i1 + globals + 64 + 432], %f54
	ldx	[%i1 + globals + 64 + 440], %f55
	ldx	[%i1 + globals + 64 + 448], %f56
	ldx	[%i1 + globals + 64 + 456], %f57
	ldx	[%i1 + globals + 64 + 464], %f59
	ldx	[%i1 + globals + 64 + 472], %f60
	ldx	[%i1 + globals + 64 + 480], %f61
#ifdef	notdef
	mov	%fsr,%g1
	ldx	[%i1 + globals + 64 + 488], %g1
	mov	%fq,%g1
	ldx	[%i1 + globals + 64 + 496], %g1
#endif
	
#endif
	
	ldx	[%o0+topstack],%g1		! sp = area1->topstack
	sub	%g1, SA(MINFRAME), %fp	! Adjust sp to the right place
	sub	%fp, SA(MINFRAME), %sp
	
	ldx	[%o0 + globals + 56], %g1		! Restore global regs back
	mov	%g1, %y
	ldx	[%o0 + globals + 0], %g1
	ldx	[%o0 + globals + 8], %g2
	ldx	[%o0 + globals + 16], %g3
	ldx	[%o0 + globals + 24],%g4
	ldx	[%o0 + globals + 32],%g5
	ldx	[%o0 + globals + 40],%g6
	ldx	[%o0 + globals + 48],%g7
	
#else	/* !__sparcv9 */

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
	
#endif	/* __sparcv9 */

	/* The following 3 lines are equivalent to: _PRE_Block = 0 */
#ifdef	__sparcv9	
	sethi	%hh(_C_LABEL(PRE_Block)),%l0
	or	%l0,%hm(_C_LABEL(PRE_Block)),%l0
	sethi	%lm(_C_LABEL(PRE_Block)),%g1
	or	%g1,%lo(_C_LABEL(PRE_Block)),%g1
	sllx	%l0,32,%l0
	or	%l0,%g1,%l0
#else
	sethi	%hi(_C_LABEL(PRE_Block)),%l0
	or	%l0,%lo(_C_LABEL(PRE_Block)),%l0
#endif
	mov	0,%l1
	stb	%l1, [%l0]

	restore					
	restore

	retl
	nop

