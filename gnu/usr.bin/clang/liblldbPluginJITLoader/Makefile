# $OpenBSD: Makefile,v 1.2 2020/08/03 14:45:30 patrick Exp $

LIB=	lldbPluginJITLoader
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	JITLoaderGDB.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/JITLoader/GDB

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Plugins
