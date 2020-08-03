# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:29 patrick Exp $

LIB=	clangRewriteFrontend
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	FixItRewriter.cpp \
	FrontendActions.cpp \
	HTMLPrint.cpp \
	InclusionRewriter.cpp \
	RewriteMacros.cpp \
	RewriteModernObjC.cpp \
	RewriteObjC.cpp \
	RewriteTest.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Frontend/Rewrite

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
