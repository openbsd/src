# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbPluginOperatingSystem
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	OperatingSystemGo.cpp \
	OperatingSystemPython.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/OperatingSystem/Go
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/OperatingSystem/Python

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
