#	$OpenBSD: Makefile,v 1.8 2002/07/05 15:54:30 itojun Exp $

SUBDIR+= _setjmp db getaddrinfo regex setjmp sigsetjmp malloc sigreturn popen
SUBDIR+= longjmp
.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
