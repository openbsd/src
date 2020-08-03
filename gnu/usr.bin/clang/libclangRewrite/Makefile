# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:29 patrick Exp $

LIB=	clangRewrite
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	DeltaTree.cpp \
	HTMLRewrite.cpp \
	RewriteRope.cpp \
	Rewriter.cpp \
	TokenRewriter.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Rewrite

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
