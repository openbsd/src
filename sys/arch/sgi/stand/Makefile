# $OpenBSD: Makefile,v 1.8 2012/03/28 20:44:23 miod Exp $

SUBDIR=	sgivol

.if ${MACHINE} == "sgi"
SUBDIR+=	libsa libsa32 libz libz32
SUBDIR+=	boot64 boot32 bootecoff
.endif

.include <bsd.subdir.mk>
