# $OpenBSD: Makefile,v 1.4 2023/11/11 18:35:39 robert Exp $

LIB=	lldbPluginSymbolVendor
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	SymbolVendorELF.cpp \
	SymbolVendorPECOFF.cpp \
	SymbolVendorWasm.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/SymbolVendor/ELF
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/SymbolVendor/PECOFF
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/SymbolVendor/wasm

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
