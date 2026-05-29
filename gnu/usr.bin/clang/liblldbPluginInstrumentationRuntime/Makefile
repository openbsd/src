# $OpenBSD: Makefile,v 1.5 2026/05/29 11:18:31 robert Exp $

LIB=	lldbPluginInstrumentationRuntime
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	InstrumentationRuntimeASan.cpp \
	InstrumentationRuntimeASanLibsanitizers.cpp \
	InstrumentationRuntimeMainThreadChecker.cpp \
	InstrumentationRuntimeTSan.cpp \
	InstrumentationRuntimeUBSan.cpp \
	ReportRetriever.cpp \
	Utility.cpp

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
