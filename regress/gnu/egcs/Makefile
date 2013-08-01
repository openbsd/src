# $OpenBSD: Makefile,v 1.6 2013/08/01 21:26:30 kettenis Exp $

SUBDIR+= gcc libiberty

.if defined(REGRESS_FULL)
SUBDIR += gcc-bounds
.endif

install:

.include <bsd.subdir.mk>
