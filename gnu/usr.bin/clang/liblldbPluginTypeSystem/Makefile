# $OpenBSD: Makefile,v 1.1 2021/04/28 12:55:42 patrick Exp $

LIB=	lldbPluginTypeSystem
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	TypeSystemClang.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/TypeSystem/Clang

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
