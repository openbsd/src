#	$OpenBSD: Makefile,v 1.7 2002/02/23 01:25:10 art Exp $

SUBDIR+= _setjmp db regex setjmp sigsetjmp malloc sigreturn popen
SUBDIR+= longjmp
.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
