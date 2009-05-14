# $OpenBSD: Makefile,v 1.6 2009/05/14 18:57:41 miod Exp $

SUBDIR=	sgivol

.if ${MACHINE} == "sgi"
SUBDIR+=	libsa libsa32 libz libz32
SUBDIR+=	boot64 boot32
.endif

.include <bsd.subdir.mk>
