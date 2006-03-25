#	$OpenBSD: Makefile,v 1.23 2006/03/25 20:28:19 otto Exp $

SUBDIR+= _setjmp alloca atexit db getaddrinfo getcap getopt_long hsearch longjmp
SUBDIR+= locale malloc
SUBDIR+= netdb popen regex setjmp setjmp-signal sigreturn sigsetjmp
SUBDIR+= sprintf strerror strtonum telldir time vis

.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
