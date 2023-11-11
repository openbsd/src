# $OpenBSD: Makefile,v 1.1 2023/11/11 18:35:39 robert Exp $

LIB=	clangSupport
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>

SRCS+=	RISCVVIntrinsicUtils.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Support

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
