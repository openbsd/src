# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:36 espie Exp $

LIB=	clangFrontendTool
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	ExecuteCompilerInvocation.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/FrontendTool

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
