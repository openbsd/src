#	$OpenBSD: Makefile,v 1.15 2004/06/24 04:33:07 millert Exp $

SUBDIR+= _setjmp alloca atexit db getaddrinfo hsearch longjmp malloc
SUBDIR+= popen regex setjmp setjmp-signal sigreturn sigsetjmp
SUBDIR+= sprintf strerror time

.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
