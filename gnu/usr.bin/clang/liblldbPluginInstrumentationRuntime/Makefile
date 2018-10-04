# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbPluginInstrumentationRuntime
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ASanRuntime.cpp \
	MainThreadCheckerRuntime.cpp \
	TSanRuntime.cpp \
	UBSanRuntime.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/InstrumentationRuntime/ASan
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/InstrumentationRuntime/MainThreadChecker
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/InstrumentationRuntime/TSan
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/InstrumentationRuntime/UBSan

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
