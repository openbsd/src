#if 0
#include <sysdep.h>
#endif
/*
 * These are various helper routines that are needed to run an ELF image.
 */

#ifdef NO_UNDERSCORE
#define __dl_linux_resolve _dl_linux_resolve
#define __dl_linux_resolver _dl_linux_resolver
#endif

.text
.even

.globl __dl_linux_resolve
	.type	__dl_linux_resolve,@function
__dl_linux_resolve:
	moveml	%a0/%a1,%sp@-
#ifdef __PIC__
	bsrl	__dl_linux_resolver:PLTPC
#else
	jsr	__dl_linux_resolver
#endif
	moveml	%sp@+,%a0/%a1
	addql 	#8,%sp
	jmp	@(%d0)
.LFE2:
	.size __dl_linux_resolve,.LFE2-__dl_linux_resolve
