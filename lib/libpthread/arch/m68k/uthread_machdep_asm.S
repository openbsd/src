/*	$OpenBSD: uthread_machdep_asm.S,v 1.1 2000/09/25 09:03:44 d Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <machine/asm.h>

#define SA(x)		(((x)+3)&~3)
#define FRAMESIZE	4*11

ENTRY(_thread_machdep_switch)
	link	a6, #-SA(FRAMESIZE)
	moveml	#0x7CFC, sp@		/* d2-d7,a2-a6 */
	movel	a6@(8), a0
	movel	a6@(12), a1
	movel	sp, a1@
	movel	a0@, sp
	moveml	sp@, #0x7CFC
	unlk	a6
	rts
