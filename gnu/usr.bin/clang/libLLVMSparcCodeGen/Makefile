# $OpenBSD: Makefile,v 1.6 2026/05/29 11:18:31 robert Exp $

LIB=	LLVMSparcCodeGen
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

SRCS+=	DelaySlotFiller.cpp \
	LeonPasses.cpp \
	SparcAsmPrinter.cpp \
	SparcInstrInfo.cpp \
	SparcISelDAGToDAG.cpp \
	SparcISelLowering.cpp \
	SparcFrameLowering.cpp \
	SparcMachineFunctionInfo.cpp \
	SparcRegisterInfo.cpp \
	SparcSelectionDAGInfo.cpp \
	SparcSubtarget.cpp \
	SparcTargetMachine.cpp \
	SparcTargetObjectFile.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/Sparc
