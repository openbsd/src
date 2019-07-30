#include <machine/asm.h>
	
#define STACKSIZE	8192

        .globl  _C_LABEL(fdtaddrsave)
        .type   _C_LABEL(fdtaddrsave),@object
	.data
_C_LABEL(fdtaddrsave):       .long   0

ENTRY(_start)
	lis	%r1, fdtaddrsave@ha
	stw	%r3, fdtaddrsave@l(%r1)

	lis	%r1, stack@ha
	addi	%r1, %r1, stack@l
	addi	%r1, %r1, STACKSIZE
	
	b	_C_LABEL(main)

	.data
.lcomm	stack,STACKSIZE,8
