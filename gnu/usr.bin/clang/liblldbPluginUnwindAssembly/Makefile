# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbPluginUnwindAssembly
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	UnwindAssembly-x86.cpp \
	UnwindAssemblyInstEmulation.cpp \
	x86AssemblyInspectionEngine.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/UnwindAssembly/InstEmulation
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/UnwindAssembly/x86

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
