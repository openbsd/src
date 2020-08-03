# $OpenBSD: Makefile,v 1.2 2020/08/03 14:45:30 patrick Exp $

LIB=	lldbPluginInstrumentationRuntime
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ASanRuntime.cpp \
	MainThreadCheckerRuntime.cpp \
	TSanRuntime.cpp \
	UBSanRuntime.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/InstrumentationRuntime/ASan
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/InstrumentationRuntime/MainThreadChecker
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/InstrumentationRuntime/TSan
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/InstrumentationRuntime/UBSan

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
