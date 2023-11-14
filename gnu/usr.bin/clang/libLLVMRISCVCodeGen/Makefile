# $OpenBSD: Makefile,v 1.4 2023/11/14 16:42:14 jca Exp $

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
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/RISCV/GISel
