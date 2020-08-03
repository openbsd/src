# $OpenBSD: Makefile,v 1.2 2020/08/03 14:45:30 patrick Exp $

LIB=	lldbPluginObjectContainer
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ObjectContainerBSDArchive.cpp \
	ObjectContainerUniversalMachO.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectContainer/BSD-Archive
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectContainer/Universal-Mach-O

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
