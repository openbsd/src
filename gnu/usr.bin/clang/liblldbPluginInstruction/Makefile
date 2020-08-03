# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:30 patrick Exp $

LIB=	lldbPluginInstruction
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	EmulateInstructionARM.cpp \
	EmulateInstructionARM64.cpp \
	EmulateInstructionMIPS.cpp \
	EmulateInstructionMIPS64.cpp \
	EmulateInstructionPPC64.cpp \
	EmulationStateARM.cpp \

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
