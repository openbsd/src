# $OpenBSD: Makefile,v 1.1 2007/09/03 14:42:44 millert Exp $

.include <bsd.own.mk>

USE_GCC3?=No

.if ${USE_GCC3:L} == "yes"
SUBDIR+= libgd1 libgd2 test1
.endif

install:

.include <bsd.subdir.mk>
