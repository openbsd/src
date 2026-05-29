# $OpenBSD: Makefile,v 1.5 2026/05/29 11:18:31 robert Exp $

LIB=	clangRewrite
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	HTMLRewrite.cpp \
	Rewriter.cpp \
	TokenRewriter.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Rewrite

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
