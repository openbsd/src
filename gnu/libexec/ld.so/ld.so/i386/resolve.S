#if 0
#include <sysdep.h>
#endif
/*
 * These are various helper routines that are needed to run an ELF image.
 */
#ifndef ALIGN
#define ALIGN 4
#endif

#ifndef NO_UNDERSCORE
#define RUN _linux_run
#define RESOLVE __dl_linux_resolve
#define EXIT __interpreter_exit
#define RESOLVER __dl_linux_resolver
#define INIT ___loader_bootstrap
#else
#define RUN linux_run
#define RESOLVE _dl_linux_resolve
#define RESOLVER _dl_linux_resolver
#define EXIT _interpreter_exit
#define INIT __loader_bootstrap
#endif

.text
.align ALIGN
	.align 16

.globl RESOLVE
	.type	RESOLVE,@function
RESOLVE:
#ifdef __PIC__
	pushl %ebx;
	call	.L26
.L24:
	movl RESOLVER@GOT(%ebx),%ebx
	call *%ebx
#else
	call RESOLVER
#endif
	popl %ebx
	addl $8,%esp
	jmp *%eax
#ifdef __PIC__
.L26:
	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-.L24],%ebx
	jmp	.L24
#endif
.LFE2:
	.size RESOLVE,.LFE2-RESOLVE
