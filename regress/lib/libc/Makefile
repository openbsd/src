#	$OpenBSD: Makefile,v 1.13 2004/02/11 23:38:11 deraadt Exp $

SUBDIR+= _setjmp alloca atexit db getaddrinfo longjmp malloc
SUBDIR+= popen regex setjmp setjmp-signal sigreturn sigsetjmp
SUBDIR+= sprintf time

.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
