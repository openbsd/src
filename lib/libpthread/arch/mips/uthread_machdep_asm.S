/*	$OpenBSD: uthread_machdep_asm.S,v 1.1 2000/10/03 02:44:15 d Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <machine/asm.h>

#define SOFF(n)		((n)*4)
#define FPOFF(n)	(SOFF(9) + 4 + (n)*8)
#define REGOFF(n)	(FPOFF(3) + (n)*4)

#define FRAMESIZE	(REGOFF(2) + 4*4+4+4)

NON_LEAF(_thread_machdep_switch, FRAMESIZE, ra)
	add	sp, sp, -FRAMESIZE

	sw	s0, SOFF(0)(sp)
	sw	s1, SOFF(1)(sp)
	sw	s2, SOFF(2)(sp)
	sw	s3, SOFF(3)(sp)
	sw	s4, SOFF(4)(sp)
	sw	s5, SOFF(5)(sp)
	sw	s6, SOFF(6)(sp)
	sw	s7, SOFF(7)(sp)
	sw	s8, SOFF(8)(sp)
	s.d	$f0, FPOFF(0)(sp)	/* XXX why? */
	s.d	$f2, FPOFF(1)(sp)
	s.d	$f4, FPOFF(2)(sp)
	sw	t9, REGOFF(0)(sp)
	sw	ra, REGOFF(1)(sp)

	sw	sp, 0(a1)
	lw	sp, 0(a0)

	.set	noreorder		/* avoid nops */
	lw	ra, REGOFF(1)(sp)
	lw	t9, REGOFF(0)(sp)
	l.d	$f4, FPOFF(2)(sp)
	l.d	$f2, FPOFF(1)(sp)
	l.d	$f0, FPOFF(0)(sp)
	lw	s8, SOFF(8)(sp)
	lw	s7, SOFF(7)(sp)
	lw	s6, SOFF(6)(sp)
	lw	s5, SOFF(5)(sp)
	lw	s4, SOFF(4)(sp)
	lw	s3, SOFF(3)(sp)
	lw	s2, SOFF(2)(sp)
	lw	s1, SOFF(1)(sp)
	lw	s0, SOFF(0)(sp)
	.set	reorder

	add	sp, sp, FRAMESIZE
	j	ra
END(_thread_machdep_switch)
