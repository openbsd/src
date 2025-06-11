# $OpenBSD: Makefile,v 1.5 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMRISCVCodeGen
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/RISCV \
		-I${LLVM_SRCS}/lib/Target/RISCV

# GISel
SRCS+=	RISCVCallLowering.cpp \
	RISCVInstructionSelector.cpp \
	RISCVLegalizerInfo.cpp \
	RISCVPostLegalizerCombiner.cpp \
	RISCVO0PreLegalizerCombiner.cpp \
	RISCVPreLegalizerCombiner.cpp \
	RISCVRegisterBankInfo.cpp

SRCS+=	RISCVAsmPrinter.cpp \
	RISCVCodeGenPrepare.cpp \
	RISCVDeadRegisterDefinitions.cpp \
	RISCVMakeCompressible.cpp \
	RISCVExpandAtomicPseudoInsts.cpp \
	RISCVExpandPseudoInsts.cpp \
	RISCVFrameLowering.cpp \
	RISCVGatherScatterLowering.cpp \
	RISCVInsertVSETVLI.cpp \
	RISCVInsertReadWriteCSR.cpp \
	RISCVInsertWriteVXRM.cpp \
	RISCVInstrInfo.cpp \
	RISCVISelDAGToDAG.cpp \
	RISCVISelLowering.cpp \
	RISCVMachineFunctionInfo.cpp \
	RISCVMergeBaseOffset.cpp \
	RISCVOptWInstrs.cpp \
	RISCVPostRAExpandPseudoInsts.cpp \
	RISCVRedundantCopyElimination.cpp \
	RISCVMoveMerger.cpp \
	RISCVPushPopOptimizer.cpp \
	RISCVRegisterInfo.cpp \
	RISCVSubtarget.cpp \
	RISCVTargetMachine.cpp \
	RISCVTargetObjectFile.cpp \
	RISCVTargetTransformInfo.cpp \
	RISCVVectorPeephole.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/RISCV
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/RISCV/GISel
