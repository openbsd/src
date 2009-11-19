#	$OpenBSD: Makefile,v 1.29 2009/11/19 08:06:06 guenther Exp $

SUBDIR+= _setjmp alloca atexit basename cxa-atexit db dirname fnmatch
SUBDIR+= fpclassify getaddrinfo getcap getopt_long glob hsearch longjmp
SUBDIR+= locale malloc netdb popen printf regex setjmp setjmp-signal
SUBDIR+= sigreturn sigsetjmp sprintf strerror strtod strtonum telldir time vis
SUBDIR+= stdio_threading

.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
