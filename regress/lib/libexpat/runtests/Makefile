# $OpenBSD: Makefile,v 1.2 2017/07/06 13:11:15 bluhm Exp $

SRCS=		runtests.c chardata.c memcheck.c minicheck.c
# align_limit_to_full_utf8_characters is not exported in dynamic library
LDFLAGS=	-static

.include <bsd.regress.mk>
