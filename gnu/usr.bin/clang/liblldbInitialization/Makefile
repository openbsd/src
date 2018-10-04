# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbInitialization
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	SystemInitializerCommon.cpp \
	SystemInitializer.cpp \
	SystemLifetimeManager.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Initialization

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
