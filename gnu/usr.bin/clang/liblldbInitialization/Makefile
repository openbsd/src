# $OpenBSD: Makefile,v 1.3 2021/04/28 12:55:41 patrick Exp $

LIB=	lldbInitialization
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	SystemInitializerCommon.cpp \
	SystemInitializer.cpp \
	SystemLifetimeManager.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Initialization

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
