# $OpenBSD: Makefile,v 1.6 2018/04/06 14:44:04 patrick Exp $

LIB=	LLVMScalarOpts
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/Scalar

.include <bsd.own.mk>
SRCS=	ADCE.cpp \
	AlignmentFromAssumptions.cpp \
	BDCE.cpp \
	CallSiteSplitting.cpp \
	ConstantHoisting.cpp \
	ConstantProp.cpp \
	CorrelatedValuePropagation.cpp \
	DCE.cpp \
	DeadStoreElimination.cpp \
	DivRemPairs.cpp \
	EarlyCSE.cpp \
	FlattenCFGPass.cpp \
	Float2Int.cpp \
	GVN.cpp \
	GVNHoist.cpp \
	GVNSink.cpp \
	GuardWidening.cpp \
	IVUsersPrinter.cpp \
	InductiveRangeCheckElimination.cpp \
	IndVarSimplify.cpp \
	JumpThreading.cpp \
	LICM.cpp \
	LoopAccessAnalysisPrinter.cpp \
	LoopDataPrefetch.cpp \
	LoopDeletion.cpp \
	LoopDistribute.cpp \
	LoopIdiomRecognize.cpp \
	LoopInstSimplify.cpp \
	LoopInterchange.cpp \
	LoopLoadElimination.cpp \
	LoopPassManager.cpp \
	LoopPredication.cpp \
	LoopRerollPass.cpp \
	LoopRotation.cpp \
	LoopSimplifyCFG.cpp \
	LoopSink.cpp \
	LoopStrengthReduce.cpp \
	LoopUnrollPass.cpp \
	LoopUnswitch.cpp \
	LoopVersioningLICM.cpp \
	LowerAtomic.cpp \
	LowerExpectIntrinsic.cpp \
	LowerGuardIntrinsic.cpp \
	MemCpyOptimizer.cpp \
	MergeICmps.cpp \
	MergedLoadStoreMotion.cpp \
	NaryReassociate.cpp \
	NewGVN.cpp \
	PartiallyInlineLibCalls.cpp \
	PlaceSafepoints.cpp \
	Reassociate.cpp \
	Reg2Mem.cpp \
	RewriteStatepointsForGC.cpp \
	SCCP.cpp \
	SROA.cpp \
	Scalar.cpp \
	Scalarizer.cpp \
	SeparateConstOffsetFromGEP.cpp \
	SimpleLoopUnswitch.cpp \
	SimplifyCFGPass.cpp \
	Sink.cpp \
	SpeculateAroundPHIs.cpp \
	SpeculativeExecution.cpp \
	StraightLineStrengthReduce.cpp \
	StructurizeCFG.cpp \
	TailRecursionElimination.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/Scalar

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
