#	$OpenBSD: Makefile,v 1.55 2020/03/23 03:01:21 schwarze Exp $

SUBDIR+= _setjmp
SUBDIR+= alloca arc4random-fork atexit
SUBDIR+= basename
SUBDIR+= cephes cxa-atexit
SUBDIR+= db dirname
SUBDIR+= env explicit_bzero
SUBDIR+= fmemopen fnmatch fpclassify fread
SUBDIR+= gcvt getaddrinfo getcap getopt getopt_long glob
SUBDIR+= hsearch
SUBDIR+= ieeefp ifnameindex
SUBDIR+= ldexp locale longjmp
SUBDIR+= malloc mkstemp modf
SUBDIR+= netdb
SUBDIR+= open_memstream orientation
SUBDIR+= popen printf
SUBDIR+= qsort
SUBDIR+= regex
SUBDIR+= setjmp setjmp-signal sigsetjmp sigthr sleep sprintf stdio_threading
SUBDIR+= stpncpy strerror strlcat strlcpy strnlen strtod strtol strtonum
SUBDIR+= sys
SUBDIR+= telldir time timingsafe
SUBDIR+= vis
SUBDIR+= wprintf

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= setjmp-fpu
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

# XXX asr tests to not use bsd.regress.mk and works completly different
# SUBDIR+= asr

install:

.include <bsd.subdir.mk>
