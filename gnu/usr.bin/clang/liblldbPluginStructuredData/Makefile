# $OpenBSD: Makefile,v 1.2 2020/08/03 14:45:31 patrick Exp $

LIB=	lldbPluginStructuredData
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	StructuredDataDarwinLog.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/StructuredData/DarwinLog

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Plugins
