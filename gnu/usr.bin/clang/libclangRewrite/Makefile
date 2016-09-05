# $OpenBSD: Makefile,v 1.2 2016/09/05 12:48:25 pascal Exp $

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

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
