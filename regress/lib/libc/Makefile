#	$OpenBSD: Makefile,v 1.18 2004/10/25 15:10:36 otto Exp $

SUBDIR+= _setjmp alloca atexit db getaddrinfo getopt_long hsearch longjmp malloc
SUBDIR+= netdb popen regex setjmp setjmp-signal sigreturn sigsetjmp
SUBDIR+= sprintf strerror strtonum time

.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
