# $OpenBSD: Makefile,v 1.14 2021/12/17 14:55:46 patrick Exp $

LIB=	LLVMScalarOpts
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/Scalar

SRCS+=	ADCE.cpp \
	AlignmentFromAssumptions.cpp \
	AnnotationRemarks.cpp \
	BDCE.cpp \
	CallSiteSplitting.cpp \
	ConstantHoisting.cpp \
	ConstraintElimination.cpp \
	CorrelatedValuePropagation.cpp \
	DCE.cpp \
	DeadStoreElimination.cpp \
	DFAJumpThreading.cpp \
	DivRemPairs.cpp \
	EarlyCSE.cpp \
	FlattenCFGPass.cpp \
	Float2Int.cpp \
	GuardWidening.cpp \
	GVN.cpp \
	GVNHoist.cpp \
	GVNSink.cpp \
	IVUsersPrinter.cpp \
	InductiveRangeCheckElimination.cpp \
	IndVarSimplify.cpp \
	InferAddressSpaces.cpp \
	InstSimplifyPass.cpp \
	JumpThreading.cpp \
	LICM.cpp \
	LoopAccessAnalysisPrinter.cpp \
	LoopBoundSplit.cpp \
	LoopSink.cpp \
	LoopDeletion.cpp \
	LoopDataPrefetch.cpp \
	LoopDistribute.cpp \
	LoopFuse.cpp \
	LoopIdiomRecognize.cpp \
	LoopInstSimplify.cpp \
	LoopInterchange.cpp \
	LoopFlatten.cpp \
	LoopLoadElimination.cpp \
	LoopPassManager.cpp \
	LoopPredication.cpp \
	LoopRerollPass.cpp \
	LoopRotation.cpp \
	LoopSimplifyCFG.cpp \
	LoopStrengthReduce.cpp \
	LoopUnrollPass.cpp \
	LoopUnrollAndJamPass.cpp \
	LoopUnswitch.cpp \
	LoopVersioningLICM.cpp \
	LowerAtomic.cpp \
	LowerConstantIntrinsics.cpp \
	LowerExpectIntrinsic.cpp \
	LowerGuardIntrinsic.cpp \
	LowerMatrixIntrinsics.cpp \
	LowerWidenableCondition.cpp \
	MakeGuardsExplicit.cpp \
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
	ScalarSCCP.cpp \
	SROA.cpp \
	Scalar.cpp \
	Scalarizer.cpp \
	ScalarizeMaskedMemIntrin.cpp \
	SeparateConstOffsetFromGEP.cpp \
	SimpleLoopUnswitch.cpp \
	SimplifyCFGPass.cpp \
	Sink.cpp \
	SpeculativeExecution.cpp \
	StraightLineStrengthReduce.cpp \
	StructurizeCFG.cpp \
	TailRecursionElimination.cpp \
	WarnMissedTransforms.cpp

ScalarSCCP.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/Scalar/SCCP.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Scalar

CLEANFILES+= ScalarSCCP.cpp
