#	$OpenBSD: Makefile,v 1.7 1997/09/21 09:29:24 niklas Exp $

.if	!make(install)
SUBDIR= special
.endif
.if exists(${MACHINE})
SUBDIR+= ${MACHINE}
.endif
SUBDIR+= notes

.include <bsd.subdir.mk>
