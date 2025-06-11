# $OpenBSD: Makefile,v 1.4 2025/06/11 17:46:33 robert Exp $

LIB=	lldbPluginInstrumentationRuntime
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	InstrumentationRuntimeASan.cpp \
	InstrumentationRuntimeASanLibsanitizers.cpp \
	InstrumentationRuntimeMainThreadChecker.cpp \
	InstrumentationRuntimeTSan.cpp \
	InstrumentationRuntimeUBSan.cpp \
	ReportRetriever.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/InstrumentationRuntime/ASan
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/InstrumentationRuntime/ASanLibsanitizers
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/InstrumentationRuntime/MainThreadChecker
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/InstrumentationRuntime/TSan
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/InstrumentationRuntime/UBSan
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/InstrumentationRuntime/Utility

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
