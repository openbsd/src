#	$OpenBSD: Makefile,v 1.10 2003/02/12 06:49:04 mickey Exp $

SUBDIR+= alloca
SUBDIR+= _setjmp db getaddrinfo regex setjmp sigsetjmp malloc sigreturn popen
SUBDIR+= longjmp atexit
.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
