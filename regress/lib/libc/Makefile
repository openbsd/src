#	$OpenBSD: Makefile,v 1.12 2004/01/20 16:47:55 millert Exp $

SUBDIR+= alloca
SUBDIR+= _setjmp db getaddrinfo regex setjmp sigsetjmp malloc sigreturn popen
SUBDIR+= longjmp atexit sprintf time
.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
