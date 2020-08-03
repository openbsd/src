# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:29 patrick Exp $

LIB=	clangFrontendTool
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	ExecuteCompilerInvocation.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/FrontendTool

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
