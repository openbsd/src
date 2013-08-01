#	$OpenBSD: Makefile,v 1.37 2013/08/01 21:26:30 kettenis Exp $

SUBDIR+= _setjmp alloca atexit basename cephes cxa-atexit db dirname env
SUBDIR+= fmemopen fnmatch fpclassify getcap getopt_long glob
SUBDIR+= hsearch longjmp locale malloc mkstemp netdb open_memstream 
SUBDIR+= orientation popen printf
SUBDIR+= regex setjmp setjmp-signal sigreturn sigsetjmp sprintf
SUBDIR+= stdio_threading stpncpy strerror strtod strtol strtonum
SUBDIR+= telldir time vis

.if defined(REGRESS_FULL)
SUBDIR+= getaddrinfo
.endif

.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
