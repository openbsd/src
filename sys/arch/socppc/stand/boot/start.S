#include <machine/asm.h>
	
#define STACKSIZE	8192

ENTRY(_start)
	lis	%r1, stack@ha
	addi	%r1, %r1, stack@l
	addi	%r1, %r1, STACKSIZE
	
	b	_C_LABEL(main)

	.data
.lcomm	stack,STACKSIZE,8
