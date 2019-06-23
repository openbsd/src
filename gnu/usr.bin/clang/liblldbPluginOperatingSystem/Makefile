# $OpenBSD: Makefile,v 1.2 2019/06/23 22:07:44 patrick Exp $

LIB=	lldbPluginOperatingSystem
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	OperatingSystemPython.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/OperatingSystem/Python

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
