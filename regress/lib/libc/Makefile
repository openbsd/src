#	$OpenBSD: Makefile,v 1.35 2012/11/18 04:11:09 jsing Exp $

SUBDIR+= _setjmp alloca atexit basename cephes cxa-atexit db dirname env
SUBDIR+= fnmatch fpclassify getaddrinfo getcap getopt_long glob hsearch
SUBDIR+= longjmp locale malloc mkstemp netdb orientation popen printf
SUBDIR+= regex setjmp setjmp-signal sigreturn sigsetjmp sprintf
SUBDIR+= stdio_threading stpncpy strerror strtod strtol strtonum
SUBDIR+= telldir time vis

.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
