# $OpenBSD: Makefile,v 1.7 2013/12/28 02:14:32 martynas Exp $

SUBDIR+= gcc libiberty

.if defined(REGRESS_FULL)
SUBDIR += gcc-bounds
SUBDIR += gcc-builtins
.endif

install:

.include <bsd.subdir.mk>
