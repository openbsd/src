#	$OpenBSD: Makefile,v 1.5 2002/02/16 01:58:33 art Exp $
#	$NetBSD: Makefile,v 1.6 1995/04/24 05:52:15 cgd Exp $

SUBDIR+= _setjmp db regex setjmp sigsetjmp malloc sigreturn popen
.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

regress: _SUBDIRUSE

install:

.include <bsd.subdir.mk>
