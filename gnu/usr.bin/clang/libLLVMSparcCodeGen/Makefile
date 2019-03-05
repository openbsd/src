# $OpenBSD: Makefile,v 1.4 2019/03/05 09:44:42 jsg Exp $

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
	SparcSubtarget.cpp \
	SparcTargetMachine.cpp \
	SparcMCInstLower.cpp \
	SparcTargetObjectFile.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Sparc
