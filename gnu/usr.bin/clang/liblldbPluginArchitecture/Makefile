# $OpenBSD: Makefile,v 1.5 2025/06/11 17:46:33 robert Exp $

LIB=	lldbPluginArchitecture
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ArchitectureAArch64.cpp \
	ArchitectureArm.cpp \
	ArchitectureMips.cpp \
	ArchitecturePPC64.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Architecture/AArch64
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Architecture/Arm
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Architecture/Mips
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Architecture/PPC64

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
