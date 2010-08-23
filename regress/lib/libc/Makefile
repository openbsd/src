#	$OpenBSD: Makefile,v 1.32 2010/08/23 22:34:37 millert Exp $

SUBDIR+= _setjmp alloca atexit basename cxa-atexit db dirname fnmatch
SUBDIR+= fpclassify getaddrinfo getcap getopt_long glob hsearch longjmp
SUBDIR+= locale malloc netdb popen printf regex setjmp setjmp-signal
SUBDIR+= sigreturn sigsetjmp sprintf strerror strtod strtonum telldir time vis
SUBDIR+= orientation stdio_threading mkstemp env

.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
