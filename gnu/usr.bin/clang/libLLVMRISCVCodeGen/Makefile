# $OpenBSD: Makefile,v 1.6 2026/05/29 11:18:31 robert Exp $

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
	RISCVCallingConv.cpp \
	RISCVCodeGenPrepare.cpp \
	RISCVConstantPoolValue.cpp \
	RISCVDeadRegisterDefinitions.cpp \
	RISCVExpandAtomicPseudoInsts.cpp \
	RISCVExpandPseudoInsts.cpp \
	RISCVFoldMemOffset.cpp \
	RISCVFrameLowering.cpp \
	RISCVGatherScatterLowering.cpp \
	RISCVIndirectBranchTracking.cpp \
	RISCVInsertReadWriteCSR.cpp \
	RISCVInsertVSETVLI.cpp \
	RISCVInsertWriteVXRM.cpp \
	RISCVInstrInfo.cpp \
	RISCVInterleavedAccess.cpp \
	RISCVISelDAGToDAG.cpp \
	RISCVISelLowering.cpp \
	RISCVLandingPadSetup.cpp \
	RISCVLateBranchOpt.cpp \
	RISCVLoadStoreOptimizer.cpp \
	RISCVMachineFunctionInfo.cpp \
	RISCVMakeCompressible.cpp \
	RISCVMachineScheduler.cpp \
	RISCVMergeBaseOffset.cpp \
	RISCVMoveMerger.cpp \
	RISCVOptWInstrs.cpp \
	RISCVPostRAExpandPseudoInsts.cpp \
	RISCVPromoteConstant.cpp \
	RISCVPushPopOptimizer.cpp \
	RISCVRedundantCopyElimination.cpp \
	RISCVRegisterInfo.cpp \
	RISCVSelectionDAGInfo.cpp \
	RISCVSubtarget.cpp \
	RISCVTargetMachine.cpp \
	RISCVTargetObjectFile.cpp \
	RISCVTargetTransformInfo.cpp \
	RISCVVectorMaskDAGMutation.cpp \
	RISCVVectorPeephole.cpp \
	RISCVVLOptimizer.cpp \
	RISCVVMV0Elimination.cpp \
	RISCVVSETVLIInfoAnalysis.cpp \
	RISCVZacasABIFix.cpp \
	RISCVZilsdOptimizer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/RISCV
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/RISCV/GISel
