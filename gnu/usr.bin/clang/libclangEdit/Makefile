# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:28 patrick Exp $

LIB=	clangEdit
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	Commit.cpp \
	EditedSource.cpp \
	RewriteObjCFoundationAPI.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Edit

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
