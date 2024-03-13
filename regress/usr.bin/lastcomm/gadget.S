/*	$OpenBSD: gadget.S,v 1.2 2024/03/13 11:18:42 kettenis Exp $	*/

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
	endbr64
	mov	$SYS_getpid, %eax
	syscall
	ret
#elif defined(__aarch64__)
	bti	c
	mov	x8, #SYS_getpid
	svc	0
	dsb	nsh
	isb
	ret
#else
#error "Missing gadget."
#endif

	.space 4096
