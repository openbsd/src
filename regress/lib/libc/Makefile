#	$OpenBSD: Makefile,v 1.16 2004/07/10 06:41:51 david Exp $

SUBDIR+= _setjmp alloca atexit db getaddrinfo getopt_long hsearch longjmp malloc
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
