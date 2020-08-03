# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:30 patrick Exp $

LIB=	lldbPluginOperatingSystem
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	OperatingSystemPython.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/OperatingSystem/Python

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
