# $OpenBSD: Makefile,v 1.3 2023/11/11 18:35:39 robert Exp $

LIB=	lldbPluginScriptInterpreter
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	ScriptInterpreterNone.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ScriptInterpreter/None

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
