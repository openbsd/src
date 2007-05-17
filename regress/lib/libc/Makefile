#	$OpenBSD: Makefile,v 1.25 2007/05/17 03:02:32 ray Exp $

SUBDIR+= _setjmp alloca atexit basename db dirname getaddrinfo
SUBDIR+= getcap getopt_long hsearch longjmp locale malloc
SUBDIR+= netdb popen regex setjmp setjmp-signal sigreturn sigsetjmp
SUBDIR+= sprintf strerror strtod strtonum telldir time vis

.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
