# $OpenBSD: Makefile,v 1.7 2012/03/19 17:38:29 miod Exp $

SUBDIR=	sgivol

.if ${MACHINE} == "sgi"
SUBDIR+=	libsa libsa32 libz libz32
SUBDIR+=	boot64 boot32
#SUBDIR+=	bootecoff
.endif

.include <bsd.subdir.mk>
