#	$OpenBSD: Makefile,v 1.42 2014/06/18 08:24:00 matthew Exp $

SUBDIR+= _setjmp alloca arc4random-fork
SUBDIR+= atexit basename cephes cxa-atexit db dirname env
SUBDIR+= explicit_bzero fmemopen fnmatch fpclassify getcap getopt_long glob
SUBDIR+= hsearch longjmp locale malloc mkstemp modf netdb open_memstream
SUBDIR+= orientation popen printf
SUBDIR+= regex setjmp setjmp-signal sigreturn sigsetjmp sprintf
SUBDIR+= stdio_threading stpncpy strerror strtod strtol strtonum
SUBDIR+= telldir time timingsafe vis

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
