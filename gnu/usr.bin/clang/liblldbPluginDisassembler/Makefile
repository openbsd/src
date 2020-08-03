# $OpenBSD: Makefile,v 1.2 2020/08/03 14:45:30 patrick Exp $

LIB=	lldbPluginDisassembler
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	DisassemblerLLVMC.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Disassembler/llvm

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
