# $OpenBSD: Makefile,v 1.12 2021/12/17 14:55:45 patrick Exp $

LIB=	LLVMAArch64CodeGen
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AArch64 \
		-I${LLVM_SRCS}/lib/Target/AArch64

# GISel
SRCS+=	AArch64CallLowering.cpp \
	AArch64GlobalISelUtils.cpp \
	AArch64InstructionSelector.cpp \
	AArch64LegalizerInfo.cpp \
	AArch64O0PreLegalizerCombiner.cpp \
	AArch64PreLegalizerCombiner.cpp \
	AArch64PostLegalizerCombiner.cpp \
	AArch64PostLegalizerLowering.cpp \
	AArch64PostSelectOptimize.cpp \
	AArch64RegisterBankInfo.cpp

SRCS+=	AArch64A57FPLoadBalancing.cpp \
	AArch64AdvSIMDScalarPass.cpp \
	AArch64AsmPrinter.cpp \
	AArch64BranchTargets.cpp \
	AArch64CallingConvention.cpp \
	AArch64CleanupLocalDynamicTLSPass.cpp \
	AArch64CollectLOH.cpp \
	AArch64CondBrTuning.cpp \
	AArch64ConditionalCompares.cpp \
	AArch64DeadRegisterDefinitionsPass.cpp \
	AArch64ExpandImm.cpp \
	AArch64ExpandPseudoInsts.cpp \
	AArch64FalkorHWPFFix.cpp \
	AArch64FastISel.cpp \
	AArch64A53Fix835769.cpp \
	AArch64FrameLowering.cpp \
	AArch64CompressJumpTables.cpp \
	AArch64ConditionOptimizer.cpp \
	AArch64RedundantCopyElimination.cpp \
	AArch64ISelDAGToDAG.cpp \
	AArch64ISelLowering.cpp \
	AArch64InstrInfo.cpp \
	AArch64LoadStoreOptimizer.cpp \
	AArch64LowerHomogeneousPrologEpilog.cpp \
	AArch64MachineFunctionInfo.cpp \
	AArch64MacroFusion.cpp \
	AArch64MCInstLower.cpp \
	AArch64PromoteConstant.cpp \
	AArch64PBQPRegAlloc.cpp \
	AArch64RegisterInfo.cpp \
	AArch64ReturnProtectorLowering.cpp \
	AArch64SLSHardening.cpp \
	AArch64SelectionDAGInfo.cpp \
	AArch64SpeculationHardening.cpp \
	AArch64StackTagging.cpp \
	AArch64StackTaggingPreRA.cpp \
	AArch64StorePairSuppress.cpp \
	AArch64Subtarget.cpp \
	AArch64TargetMachine.cpp \
	AArch64TargetObjectFile.cpp \
	AArch64TargetTransformInfo.cpp \
	SVEIntrinsicOpts.cpp \
	AArch64SIMDInstrOpt.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/AArch64
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/AArch64/GISel
