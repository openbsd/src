#	$OpenBSD: Makefile,v 1.52 2018/12/16 15:36:27 millert Exp $

SUBDIR+= _setjmp
SUBDIR+= alloca arc4random-fork atexit
SUBDIR+= basename
SUBDIR+= cephes cxa-atexit
SUBDIR+= db dirname
SUBDIR+= env explicit_bzero
SUBDIR+= fmemopen fnmatch fpclassify fread
SUBDIR+= getaddrinfo getcap getopt_long glob
SUBDIR+= hsearch
SUBDIR+= ieeefp ifnameindex
SUBDIR+= ldexp longjmp locale
SUBDIR+= malloc mkstemp modf
SUBDIR+= netdb
SUBDIR+= open_memstream orientation
SUBDIR+= popen printf
SUBDIR+= qsort
SUBDIR+= regex
SUBDIR+= setjmp setjmp-signal sigsetjmp sprintf stdio_threading
SUBDIR+= stpncpy strerror strlcat strlcpy strnlen strtod strtol strtonum
SUBDIR+= telldir time timingsafe
SUBDIR+= vis

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= setjmp-fpu
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
