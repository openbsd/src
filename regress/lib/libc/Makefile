#	$OpenBSD: Makefile,v 1.14 2004/04/30 17:15:12 espie Exp $

SUBDIR+= _setjmp alloca atexit db getaddrinfo longjmp malloc
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
