/*	$OpenBSD: uthread_machdep_asm.S,v 1.1 2000/09/25 01:16:40 d Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <machine/asm.h>

/*
 * Switch stacks
 */

/* void _thread_machdep_switch(new, oldsave); */
ENTRY(_thread_machdep_switch)
	pushal		/* pushl %eax,%ecx,%edx,%ebx,%esp,%ebp,%esi,%edi */
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs
#define DISTANCE	((8+1+1+1+1)*4)
	movl	(DISTANCE+8)(%esp), %eax	/* %eax = arg2  */
	movl	%esp, 0(%eax)			/* *arg2 = %esp */
	movl	(DISTANCE+4)(%esp), %eax	/* %eax = arg1  */
	movl	0(%eax), %esp			/* %esp = *arg1 */
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popal		/* popl %edi,%esi,%ebp,%esp,%ebx,%edx,%ecx,%eax */
	ret
