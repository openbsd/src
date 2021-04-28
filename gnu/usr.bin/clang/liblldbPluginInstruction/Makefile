# $OpenBSD: Makefile,v 1.4 2021/04/28 12:55:41 patrick Exp $

LIB=	lldbPluginInstruction
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	EmulateInstructionARM.cpp \
	EmulationStateARM.cpp \
	EmulateInstructionARM64.cpp \
	EmulateInstructionMIPS.cpp \
	EmulateInstructionMIPS64.cpp \
	EmulateInstructionPPC64.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Instruction/ARM
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Instruction/ARM64
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Instruction/MIPS
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Instruction/MIPS64
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Instruction/PPC64

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
