# $OpenBSD: Makefile,v 1.3 2021/04/28 12:55:42 patrick Exp $

LIB=	lldbPluginUnwindAssembly
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	UnwindAssemblyInstEmulation.cpp \
	UnwindAssembly-x86.cpp \
	x86AssemblyInspectionEngine.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/UnwindAssembly/InstEmulation
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/UnwindAssembly/x86

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
