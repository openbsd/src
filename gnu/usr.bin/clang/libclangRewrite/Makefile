# $OpenBSD: Makefile,v 1.3 2017/07/09 15:28:36 espie Exp $

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

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Rewrite

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
