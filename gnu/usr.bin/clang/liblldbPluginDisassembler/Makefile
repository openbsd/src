# $OpenBSD: Makefile,v 1.3 2021/04/28 12:55:41 patrick Exp $

LIB=	lldbPluginDisassembler
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	DisassemblerLLVMC.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Disassembler/LLVMC

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
