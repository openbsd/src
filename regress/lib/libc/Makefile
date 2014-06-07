#	$OpenBSD: Makefile,v 1.39 2014/06/07 01:46:40 tobiasu Exp $

SUBDIR+= _setjmp alloca atexit basename cephes cxa-atexit db dirname env
SUBDIR+= fmemopen fnmatch fpclassify getcap getopt_long glob
SUBDIR+= hsearch longjmp locale malloc mkstemp modf netdb open_memstream
SUBDIR+= orientation popen printf
SUBDIR+= regex setjmp setjmp-signal sigreturn sigsetjmp sprintf
SUBDIR+= stdio_threading stpncpy strerror strtod strtol strtonum
SUBDIR+= telldir time vis

.if defined(REGRESS_FULL)
SUBDIR+= getaddrinfo
SUBDIR+= setjmp-fpu
.endif

.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
