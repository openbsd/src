#	$OpenBSD: Makefile,v 1.2 2001/01/29 02:05:37 niklas Exp $
#	$NetBSD: Makefile,v 1.6 1995/04/24 05:52:15 cgd Exp $

SUBDIR+= _setjmp db regex setjmp sigsetjmp
.if (${MACHINE_ARCH} != "vax")
SUBDIR+= ieeefp
.endif

.if exists(arch/${MACHINE_ARCH})
SUBDIR+= arch/${MACHINE_ARCH}
.endif

regress: _SUBDIRUSE

install:

.include <bsd.subdir.mk>
