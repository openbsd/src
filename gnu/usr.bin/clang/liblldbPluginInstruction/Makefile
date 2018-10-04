# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbPluginInstruction
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	EmulateInstructionARM.cpp \
	EmulateInstructionARM64.cpp \
	EmulateInstructionMIPS.cpp \
	EmulateInstructionMIPS64.cpp \
	EmulationStateARM.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Instruction/ARM
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Instruction/ARM64
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Instruction/MIPS
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Instruction/MIPS64

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
