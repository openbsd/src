#	$OpenBSD: Makefile,v 1.11 2003/05/15 04:23:49 deraadt Exp $

SUBDIR+= alloca
SUBDIR+= _setjmp db getaddrinfo regex setjmp sigsetjmp malloc sigreturn popen
SUBDIR+= longjmp atexit sprintf
.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
