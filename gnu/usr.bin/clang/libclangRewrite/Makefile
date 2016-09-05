# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

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
	# Nothing here so far ...

.include <bsd.lib.mk>
