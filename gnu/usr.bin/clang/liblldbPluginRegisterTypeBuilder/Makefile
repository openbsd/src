# $OpenBSD: Makefile,v 1.1 2025/06/11 17:46:33 robert Exp $

LIB=	lldbPluginRegisterTypeBuilder
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	RegisterTypeBuilderClang.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/RegisterTypeBuilder

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Plugins
