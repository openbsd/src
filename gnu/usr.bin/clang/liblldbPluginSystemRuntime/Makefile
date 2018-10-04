# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbPluginSystemRuntime
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	AppleGetItemInfoHandler.cpp \
	AppleGetPendingItemsHandler.cpp \
	AppleGetQueuesHandler.cpp \
	AppleGetThreadItemInfoHandler.cpp \
	SystemRuntimeMacOSX.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/SystemRuntime/MacOSX

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
