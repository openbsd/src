# $OpenBSD: Makefile,v 1.4 2019/06/29 00:35:06 bluhm Exp $

SRCS=		runtestspp.cpp chardata.c memcheck.c minicheck.c structdata.c
# _INTERNAL_trim_to_complete_utf8_characters is not exported in dynamic library
LDFLAGS=	-static

.include <bsd.regress.mk>
