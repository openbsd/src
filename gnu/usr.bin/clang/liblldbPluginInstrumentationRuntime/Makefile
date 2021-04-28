# $OpenBSD: Makefile,v 1.3 2021/04/28 12:55:41 patrick Exp $

LIB=	lldbPluginInstrumentationRuntime
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	InstrumentationRuntimeASan.cpp \
	InstrumentationRuntimeMainThreadChecker.cpp \
	InstrumentationRuntimeTSan.cpp \
	InstrumentationRuntimeUBSan.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/InstrumentationRuntime/ASan
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/InstrumentationRuntime/MainThreadChecker
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/InstrumentationRuntime/TSan
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/InstrumentationRuntime/UBSan

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
