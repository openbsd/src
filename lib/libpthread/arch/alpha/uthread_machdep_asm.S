/*	$OpenBSD: uthread_machdep_asm.S,v 1.4 2002/12/12 18:26:18 marc Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <machine/asm.h>

#define INTOFF(n)	((n)*8)
#define FPOFF(n)	(INTOFF(9) + (n)*8)
#define ALIGN(x)	(((x)+15)&~15)
#define FRAMESIZE	ALIGN(FPOFF(8))

	.set	noreorder
	.globl	_thread_machdep_switch
	.ent	_thread_machdep_switch, 2
_thread_machdep_switch:
	.frame	sp, FRAMESIZE, ra
	ldgp	gp, 0(pv)
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
	or	a0, zero, pv
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

LEAF(_atomic_lock,1)
        LDGP(pv)

				/* NOTE: using ldl_l/stl_c instead of
				   ldq_l and ldq_c as machine/spinlock.h
				   defines _spinlock_lock_t as int */
0:      ldl_l   v0, 0(a0)       /* read existing lock value */
        mov     1, t0           /* locked value to store */
        stl_c   t0, 0(a0)       /* attempt to store, status in t0 */
        beq     t0, 1f          /* branch forward to optimise prediction */
        mb                      /* sync with other processors */
        RET                     /* return with v0==0 if lock obtained */
1:      br      0b              /* loop to try again */
END(_atomic_lock)
