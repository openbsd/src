#	$OpenBSD: Makefile,v 1.6 2002/02/18 11:28:27 art Exp $

SUBDIR+= _setjmp db regex setjmp sigsetjmp malloc sigreturn popen
SUBDIR+= longjmp
.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

regress: _SUBDIRUSE

install:

.include <bsd.subdir.mk>
