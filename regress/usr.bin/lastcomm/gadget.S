/*	$OpenBSD: gadget.S,v 1.1 2023/01/09 11:50:01 anton Exp $	*/

#include <sys/syscall.h>
#include <machine/asm.h>

#if !defined(_ASM_TYPE_FUNCTION)
#define _ASM_TYPE_FUNCTION @function
#endif

        .section .rodata
        .globl	gadget_getpid
        .type	gadget_getpid,_ASM_TYPE_FUNCTION
gadget_getpid:
#if defined(__amd64__)
	mov	$SYS_getpid, %eax
	syscall
	ret
#elif defined(__aarch64__)
	ldr	x8, #SYS_getpid
	svc	0
	dsb	nsh
	isb
	ret
#else
#error "Missing gadget."
#endif

	.space 4096
