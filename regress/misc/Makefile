#	$OpenBSD: Makefile,v 1.6 2017/04/17 12:54:48 kettenis Exp $

.include <bsd.own.mk>

SUBDIR+= X11 c++abi exceptions sse2

.if ${COMPILER_VERSION:L} != "clang"
SUBDIR+= objc
.endif

install:

.include <bsd.subdir.mk>
