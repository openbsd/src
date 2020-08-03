# $OpenBSD: Makefile,v 1.2 2020/08/03 14:45:31 patrick Exp $

LIB=	lldbPluginSymbolVendor
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	SymbolVendorELF.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/SymbolVendor/ELF

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
