#	$OpenBSD: Makefile,v 1.3 2001/12/05 09:52:01 deraadt Exp $
#	$NetBSD: Makefile,v 1.6 1995/04/24 05:52:15 cgd Exp $

SUBDIR+= _setjmp db regex setjmp sigsetjmp malloc
.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

regress: _SUBDIRUSE

install:

.include <bsd.subdir.mk>
