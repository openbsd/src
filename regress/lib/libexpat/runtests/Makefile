# $OpenBSD: Makefile,v 1.3 2017/11/03 18:30:34 bluhm Exp $

SRCS=		runtests.c chardata.c memcheck.c minicheck.c structdata.c
# align_limit_to_full_utf8_characters is not exported in dynamic library
LDFLAGS=	-static

.include <bsd.regress.mk>
