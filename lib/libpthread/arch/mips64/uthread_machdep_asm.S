/*	$OpenBSD: uthread_machdep_asm.S,v 1.3 2004/11/02 21:36:11 pefo Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <machine/asm.h>

/*
 *   Save layout:
 *
 *          -----------
 *     +152 |  sp     | 
 *     +144 |  t9     | 
 *          -----------
 *     +136 |  fcr    | 
 *          -----------
 *     +128 |  f31    | 
 *     +120 |  f30    | 
 *     +112 |  f29    | 
 *     +104 |  f28    | 
 *     +96  |  f27    | 
 *     +88  |  f26    | 
 *     +80  |  f25    | 
 *     +72  |  f24    | 
 *          -----------
 *     +64  |  s8     | 
 *     +56  |  s7     | 
 *     +48  |  s6     | 
 *     +40  |  s5     | 
 *     +32  |  s4     | 
 *     +24  |  s3     | 
 *     +16  |  s2     | 
 *     +8   |  s1     | 
 *   sp ->  |  s0     |
 *          -----------
 */

#define SOFF(n)		((n)*REGSZ)
#define FPOFF(n)	(SOFF(9) + (n)*8)
#define REGOFF(n)	(FPOFF(9) + (n)*REGSZ)

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
	s.d	$f24, FPOFF(0)(sp)
	s.d	$f25, FPOFF(1)(sp)
	s.d	$f26, FPOFF(2)(sp)
	s.d	$f27, FPOFF(3)(sp)
	s.d	$f28, FPOFF(4)(sp)
	s.d	$f29, FPOFF(5)(sp)
	s.d	$f30, FPOFF(6)(sp)
	s.d	$f31, FPOFF(7)(sp)
	REG_S	t9, REGOFF(0)(sp)
	REG_S	ra, REGOFF(1)(sp)
	cfc1	t9, $31
	REG_S	t9, FPOFF(8)(sp)

	REG_S	sp, 0(a1)
	REG_L	sp, 0(a0)

	.set	noreorder		/* avoid nops */
	REG_L	t9, FPOFF(8)(sp)
	ctc1	t9, $31
	REG_L	ra, REGOFF(1)(sp)
	REG_L	t9, REGOFF(0)(sp)
	l.d	$f24, FPOFF(0)(sp)
	l.d	$f25, FPOFF(1)(sp)
	l.d	$f26, FPOFF(2)(sp)
	l.d	$f27, FPOFF(3)(sp)
	l.d	$f28, FPOFF(4)(sp)
	l.d	$f29, FPOFF(5)(sp)
	l.d	$f30, FPOFF(6)(sp)
	l.d	$f31, FPOFF(7)(sp)
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
