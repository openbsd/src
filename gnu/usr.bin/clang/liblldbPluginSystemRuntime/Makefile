# $OpenBSD: Makefile,v 1.3 2021/04/28 12:55:42 patrick Exp $

LIB=	lldbPluginSystemRuntime
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	AppleGetItemInfoHandler.cpp \
	AppleGetPendingItemsHandler.cpp \
	AppleGetQueuesHandler.cpp \
	AppleGetThreadItemInfoHandler.cpp \
	SystemRuntimeMacOSX.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/SystemRuntime/MacOSX

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
