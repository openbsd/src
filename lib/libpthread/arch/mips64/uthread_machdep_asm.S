/*	$OpenBSD: uthread_machdep_asm.S,v 1.2 2004/09/09 16:59:21 pefo Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <machine/asm.h>

#define SOFF(n)		((n)*REGSZ)
#define FPOFF(n)	(SOFF(9) + REGSZ + (n)*8)
#define REGOFF(n)	(FPOFF(3) + (n)*REGSZ)

#define FRAMESIZE	(REGOFF(2) + REGSZ*4+REGSZ+REGSZ)

NON_LEAF(_thread_machdep_switch, FRAMESIZE, ra)
	PTR_ADDU sp, sp, -FRAMESIZE

	REG_S	s0, SOFF(0)(sp)
	REG_S	s1, SOFF(1)(sp)
	REG_S	s2, SOFF(2)(sp)
	REG_S	s3, SOFF(3)(sp)
	REG_S	s4, SOFF(4)(sp)
	REG_S	s5, SOFF(5)(sp)
	REG_S	s6, SOFF(6)(sp)
	REG_S	s7, SOFF(7)(sp)
	REG_S	s8, SOFF(8)(sp)
	s.d	$f0, FPOFF(0)(sp)	/* XXX why? */
	s.d	$f2, FPOFF(1)(sp)
	s.d	$f4, FPOFF(2)(sp)
	REG_S	t9, REGOFF(0)(sp)
	REG_S	ra, REGOFF(1)(sp)

	REG_S	sp, 0(a1)
	REG_L	sp, 0(a0)

	.set	noreorder		/* avoid nops */
	REG_L	ra, REGOFF(1)(sp)
	REG_L	t9, REGOFF(0)(sp)
	l.d	$f4, FPOFF(2)(sp)
	l.d	$f2, FPOFF(1)(sp)
	l.d	$f0, FPOFF(0)(sp)
	REG_L	s8, SOFF(8)(sp)
	REG_L	s7, SOFF(7)(sp)
	REG_L	s6, SOFF(6)(sp)
	REG_L	s5, SOFF(5)(sp)
	REG_L	s4, SOFF(4)(sp)
	REG_L	s3, SOFF(3)(sp)
	REG_L	s2, SOFF(2)(sp)
	REG_L	s1, SOFF(1)(sp)
	REG_L	s0, SOFF(0)(sp)
	.set	reorder

	PTR_ADDU sp, sp, FRAMESIZE
	j	ra
END(_thread_machdep_switch)
