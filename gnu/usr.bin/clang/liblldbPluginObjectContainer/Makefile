# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbPluginObjectContainer
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ObjectContainerBSDArchive.cpp \
	ObjectContainerUniversalMachO.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ObjectContainer/BSD-Archive
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ObjectContainer/Universal-Mach-O

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
