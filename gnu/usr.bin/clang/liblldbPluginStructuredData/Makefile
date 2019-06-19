# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbPluginStructuredData
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	StructuredDataDarwinLog.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/StructuredData/DarwinLog

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
