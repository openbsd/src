# $OpenBSD: Makefile,v 1.4 2026/05/29 11:18:31 robert Exp $

LIB=	lldbPluginSystemRuntime
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	AppleGetItemInfoHandler.cpp \
	AppleGetPendingItemsHandler.cpp \
	AppleGetQueuesHandler.cpp \
	AppleGetThreadItemInfoHandler.cpp \
	SystemRuntimeMacOSX.cpp \
	AbortWithPayloadFrameRecognizer.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/SystemRuntime/MacOSX

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
