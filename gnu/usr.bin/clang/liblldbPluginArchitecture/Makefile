# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:30 patrick Exp $

LIB=	lldbPluginArchitecture
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ArchitectureArm.cpp \
	ArchitectureMips.cpp \
	ArchitecturePPC64.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Architecture/Arm
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Architecture/Mips
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Architecture/PPC64

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
