# $OpenBSD: Makefile,v 1.7 2018/08/12 17:07:00 mortimer Exp $

LIB=	LLVMAArch64CodeGen
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AArch64 \
		-I${LLVM_SRCS}/lib/Target/AArch64

.include <bsd.own.mk>
SRCS=	AArch64A57FPLoadBalancing.cpp \
	AArch64AdvSIMDScalarPass.cpp \
	AArch64AsmPrinter.cpp \
	AArch64CallLowering.cpp \
	AArch64CleanupLocalDynamicTLSPass.cpp \
	AArch64CollectLOH.cpp \
	AArch64CondBrTuning.cpp \
	AArch64ConditionOptimizer.cpp \
	AArch64ConditionalCompares.cpp \
	AArch64DeadRegisterDefinitionsPass.cpp \
	AArch64ExpandPseudoInsts.cpp \
	AArch64FalkorHWPFFix.cpp \
	AArch64FastISel.cpp \
	AArch64A53Fix835769.cpp \
	AArch64FrameLowering.cpp \
	AArch64ISelDAGToDAG.cpp \
	AArch64ISelLowering.cpp \
	AArch64InstrInfo.cpp \
	AArch64InstructionSelector.cpp \
	AArch64LegalizerInfo.cpp \
	AArch64LoadStoreOptimizer.cpp \
	AArch64MacroFusion.cpp \
	AArch64MCInstLower.cpp \
	AArch64PromoteConstant.cpp \
	AArch64PBQPRegAlloc.cpp \
	AArch64RedundantCopyElimination.cpp \
	AArch64RegisterBankInfo.cpp \
	AArch64RegisterInfo.cpp \
	AArch64ReturnProtectorLowering.cpp \
	AArch64SIMDInstrOpt.cpp \
	AArch64SelectionDAGInfo.cpp \
	AArch64StorePairSuppress.cpp \
	AArch64Subtarget.cpp \
	AArch64TargetMachine.cpp \
	AArch64TargetObjectFile.cpp \
	AArch64TargetTransformInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AArch64

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
