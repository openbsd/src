#	$NetBSD: Makefile,v 1.4 1995/04/20 22:41:08 cgd Exp $

SUBDIR+= kern
.if exists(arch/${MACHINE})
SUBDIR+= arch/${MACHINE}
.endif

regress: _SUBDIRUSE

install:

.include <bsd.subdir.mk>
