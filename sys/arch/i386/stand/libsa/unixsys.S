

#include <machine/asm.h>
#include <sys/syscall.h>


#define RSYSCALL(x) \
	ENTRY(u/**/x); \
		movl	 $(SYS_/**/x), %eax; \
		jmp	 1f

	.globl	_errno

RSYSCALL(open)
RSYSCALL(read)
RSYSCALL(write)
RSYSCALL(ioctl)
RSYSCALL(close)
RSYSCALL(exit)

1:
	int	$0x80
2:	jnc	3f
	movl	%eax, _errno
	movl	$-1, %eax
	movl	$-1, %edx
3:	ret

ENTRY(__syscall)
	movl	$(SYS___syscall), %eax
	jmp	1b

ENTRY(syscall)
	popl	%ecx	/* rta */
	popl	%eax	/* syscall number */
	pushl	%ecx
	int	$0x80
	pushl	%ecx
	jmp	2b
