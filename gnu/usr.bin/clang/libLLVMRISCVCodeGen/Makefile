# $OpenBSD: Makefile,v 1.3 2023/11/11 18:35:37 robert Exp $

LIB=	LLVMRISCVCodeGen
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/RISCV \
		-I${LLVM_SRCS}/lib/Target/RISCV

# GISel
SRCS+=	RISCVCallLowering.cpp \
	RISCVInstructionSelector.cpp \
	RISCVLegalizerInfo.cpp \
	RISCVRegisterBankInfo.cpp

SRCS+=	RISCVAsmPrinter.cpp \
	RISCVCodeGenPrepare.cpp \
	RISCVMakeCompressible.cpp \
	RISCVExpandAtomicPseudoInsts.cpp \
	RISCVExpandPseudoInsts.cpp \
	RISCVFrameLowering.cpp \
	RISCVGatherScatterLowering.cpp \
	RISCVInsertVSETVLI.cpp \
	RISCVInstrInfo.cpp \
	RISCVISelDAGToDAG.cpp \
	RISCVISelLowering.cpp \
	RISCVMachineFunctionInfo.cpp \
	RISCVMacroFusion.cpp \
	RISCVMCInstLower.cpp \
	RISCVMergeBaseOffset.cpp \
	RISCVRedundantCopyElimination.cpp \
	RISCVRegisterInfo.cpp \
	RISCVSExtWRemoval.cpp \
	RISCVStripWSuffix.cpp \
	RISCVSubtarget.cpp \
	RISCVTargetMachine.cpp \
	RISCVTargetObjectFile.cpp \
	RISCVTargetTransformInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/RISCV
