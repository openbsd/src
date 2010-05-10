# $OpenBSD: Makefile,v 1.2 2010/05/10 18:20:31 drahn Exp $

.include <bsd.own.mk>


.if ${COMPILER_VERSION:L} == "gcc3" || ${COMPILER_VERSION:L} == "gcc4" 
SUBDIR+= libgd1 libgd2 test1
.endif

install:

.include <bsd.subdir.mk>
