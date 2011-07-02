#	$OpenBSD: Makefile,v 1.33 2011/07/02 18:12:48 martynas Exp $

SUBDIR+= _setjmp alloca atexit basename cxa-atexit db dirname fnmatch
SUBDIR+= fpclassify getaddrinfo getcap getopt_long glob hsearch longjmp
SUBDIR+= locale malloc netdb popen printf regex setjmp setjmp-signal
SUBDIR+= sigreturn sigsetjmp sprintf strerror strtod strtonum telldir time vis
SUBDIR+= orientation stdio_threading mkstemp env cephes

.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
