# $OpenBSD: Makefile,v 1.2 2020/08/03 14:45:31 patrick Exp $

LIB=	lldbPluginUnwindAssembly
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	UnwindAssembly-x86.cpp \
	UnwindAssemblyInstEmulation.cpp \
	x86AssemblyInspectionEngine.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/UnwindAssembly/InstEmulation
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/UnwindAssembly/x86

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
