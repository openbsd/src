# $OpenBSD: Makefile,v 1.1 2026/05/29 11:18:31 robert Exp $

LIB=	lldbInterpreterInterfaces
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	ScriptedInterfaceUsages.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Interpreter/Interfaces

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
