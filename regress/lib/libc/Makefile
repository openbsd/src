#	$OpenBSD: Makefile,v 1.9 2002/08/30 07:58:08 dhartmei Exp $

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
