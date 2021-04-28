# $OpenBSD: Makefile,v 1.3 2021/04/28 12:55:42 patrick Exp $

LIB=	lldbPluginSymbolVendor
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	SymbolVendorELF.cpp \
	SymbolVendorWasm.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/SymbolVendor/ELF
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/SymbolVendor/wasm

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
