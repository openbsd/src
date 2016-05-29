#	$OpenBSD: Makefile,v 1.48 2016/05/29 17:09:07 beck Exp $

SUBDIR+= _setjmp alloca arc4random-fork
SUBDIR+= atexit basename cephes cxa-atexit db dirname env
SUBDIR+= explicit_bzero fmemopen fnmatch fpclassify getcap getopt_long glob
SUBDIR+= hsearch ifnameindex longjmp locale malloc mkstemp modf netdb
SUBDIR+= open_memstream orientation popen printf
SUBDIR+= regex setjmp setjmp-signal sigsetjmp sprintf stdio_threading
SUBDIR+= stpncpy strerror strlcat strlcpy strnlen strtod strtol strtonum
SUBDIR+= telldir time timingsafe vis

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
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
