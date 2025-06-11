# $OpenBSD: Makefile,v 1.5 2025/06/11 17:46:33 robert Exp $

LIB=	lldbPluginInstruction
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	EmulateInstructionARM.cpp \
	EmulationStateARM.cpp \
	EmulateInstructionARM64.cpp \
	EmulateInstructionLoongArch.cpp \
	EmulateInstructionMIPS.cpp \
	EmulateInstructionMIPS64.cpp \
	EmulateInstructionPPC64.cpp \
	EmulateInstructionRISCV.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Instruction/ARM
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Instruction/ARM64
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Instruction/LoongArch
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Instruction/MIPS
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Instruction/MIPS64
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Instruction/PPC64
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Instruction/RISCV

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
