#	$NetBSD: Makefile,v 1.4 1995/10/03 21:59:36 phil Exp $

.if ${MACHINE} == "pc532"
SUBDIR+= round
.else
SUBDIR+= except round
.endif

regress: _SUBDIRUSE

install:

.include <bsd.subdir.mk>
