# $OpenBSD: Makefile,v 1.1.1.1 2007/07/31 20:31:42 kurt Exp $

.include <bsd.own.mk>

USE_GCC3?=No

.if ${USE_GCC3:L} == "yes"
SUBDIR+= libaa libab test1 test2
.endif

install:

.include <bsd.subdir.mk>
