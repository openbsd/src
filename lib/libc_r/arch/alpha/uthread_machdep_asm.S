/*	$OpenBSD: uthread_machdep_asm.S,v 1.1 2000/09/25 05:44:13 d Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <machine/asm.h>

#define INTOFF(n)	((n)*8)
#define FPOFF(n)	(INTOFF(9) + (n)*8)
#define ALIGN(x)	(((x)+15)&~15)
#define FRAMESIZE	ALIGN(FPOFF(8))

	.globl	_thread_machdep_switch
	.ent	_thread_machdep_switch, 2
_thread_machdep_switch:
	.frame	sp, FRAMESIZE, ra

	lda	sp, -FRAMESIZE(sp)

	stq	ra, INTOFF(0)(sp)
	stq	s0, INTOFF(1)(sp)
	stq	s1, INTOFF(2)(sp)
	stq	s2, INTOFF(3)(sp)
	stq	s3, INTOFF(4)(sp)
	stq	s4, INTOFF(5)(sp)
	stq	s5, INTOFF(6)(sp)
	stq	s6, INTOFF(7)(sp)
	stq	t12, INTOFF(8)(sp)

	stt	fs0, FPOFF(0)(sp)
	stt	fs1, FPOFF(1)(sp)
	stt	fs2, FPOFF(2)(sp)
	stt	fs3, FPOFF(3)(sp)
	stt	fs4, FPOFF(4)(sp)
	stt	fs5, FPOFF(5)(sp)
	stt	fs6, FPOFF(6)(sp)
	stt	fs7, FPOFF(7)(sp)

	stq	sp, 0(a1)
	ldq	sp, 0(a0)

	ldt	fs7, FPOFF(7)(sp)
	ldt	fs6, FPOFF(6)(sp)
	ldt	fs5, FPOFF(5)(sp)
	ldt	fs4, FPOFF(4)(sp)
	ldt	fs3, FPOFF(3)(sp)
	ldt	fs2, FPOFF(2)(sp)
	ldt	fs1, FPOFF(1)(sp)
	ldt	fs0, FPOFF(0)(sp)

	ldq	t12, INTOFF(8)(sp)
	ldq	s6, INTOFF(7)(sp)
	ldq	s5, INTOFF(6)(sp)
	ldq	s4, INTOFF(5)(sp)
	ldq	s3, INTOFF(4)(sp)
	ldq	s2, INTOFF(3)(sp)
	ldq	s1, INTOFF(2)(sp)
	ldq	s0, INTOFF(1)(sp)
	ldq	ra, INTOFF(0)(sp)

	lda	sp,FRAMESIZE(sp)

	RET

	.end	_thread_machdep_switch
