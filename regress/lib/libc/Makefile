#	$OpenBSD: Makefile,v 1.21 2005/08/29 18:41:56 otto Exp $

SUBDIR+= _setjmp alloca atexit db getaddrinfo getopt_long hsearch longjmp
SUBDIR+= locale malloc
SUBDIR+= netdb popen regex setjmp setjmp-signal sigreturn sigsetjmp
SUBDIR+= sprintf strerror strtonum time vis

.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
