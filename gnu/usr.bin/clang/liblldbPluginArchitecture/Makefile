# $OpenBSD: Makefile,v 1.3 2019/06/23 22:07:43 patrick Exp $

LIB=	lldbPluginArchitecture
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ArchitectureArm.cpp \
	ArchitectureMips.cpp \
	ArchitecturePPC64.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Architecture/Arm
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Architecture/Mips
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Architecture/PPC64

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
