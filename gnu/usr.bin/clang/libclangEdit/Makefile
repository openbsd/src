# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

LIB=	clangEdit
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	Commit.cpp \
	EditedSource.cpp \
	RewriteObjCFoundationAPI.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Edit

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
