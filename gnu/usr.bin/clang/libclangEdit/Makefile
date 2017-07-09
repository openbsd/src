# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:36 espie Exp $

LIB=	clangEdit
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	Commit.cpp \
	EditedSource.cpp \
	RewriteObjCFoundationAPI.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Edit

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
