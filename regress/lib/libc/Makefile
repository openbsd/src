#	$OpenBSD: Makefile,v 1.27 2008/09/07 20:36:10 martynas Exp $

SUBDIR+= _setjmp alloca atexit basename cxa-atexit db dirname fpclassify 
SUBDIR+= getaddrinfo getcap getopt_long hsearch longjmp locale malloc
SUBDIR+= netdb popen printf regex setjmp setjmp-signal sigreturn sigsetjmp
SUBDIR+= sprintf strerror strtod strtonum telldir time vis

.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
