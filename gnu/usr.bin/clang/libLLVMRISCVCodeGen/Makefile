# $OpenBSD: Makefile,v 1.1 2021/04/29 05:32:01 drahn Exp $

LIB=	LLVMRISCVCodeGen
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/RISCV \
		-I${LLVM_SRCS}/lib/Target/RISCV

SRCS+=	RISCVAsmPrinter.cpp \
	RISCVCallLowering.cpp \
	RISCVExpandAtomicPseudoInsts.cpp\
	RISCVExpandPseudoInsts.cpp \
	RISCVFrameLowering.cpp \
	RISCVInstrInfo.cpp \
	RISCVInstructionSelector.cpp \
	RISCVISelDAGToDAG.cpp \
	RISCVISelLowering.cpp \
	RISCVLegalizerInfo.cpp \
	RISCVMCInstLower.cpp \
	RISCVMergeBaseOffset.cpp \
	RISCVRegisterBankInfo.cpp \
	RISCVRegisterInfo.cpp \
	RISCVSubtarget.cpp \
	RISCVTargetMachine.cpp \
	RISCVTargetObjectFile.cpp \
	RISCVTargetTransformInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/RISCV
