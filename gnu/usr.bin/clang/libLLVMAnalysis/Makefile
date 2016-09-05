# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:48 pascal Exp $

LIB=	LLVMAnalysis
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Analysis

.include <bsd.own.mk>
SRCS=	AliasAnalysis.cpp \
	AliasAnalysisEvaluator.cpp \
	AliasSetTracker.cpp \
	Analysis.cpp \
	AssumptionCache.cpp \
	BasicAliasAnalysis.cpp \
	BlockFrequencyInfo.cpp \
	BlockFrequencyInfoImpl.cpp \
	BranchProbabilityInfo.cpp \
	CFG.cpp \
	CFGPrinter.cpp \
	CFLAliasAnalysis.cpp \
	CGSCCPassManager.cpp \
	CallGraph.cpp \
	CallGraphSCCPass.cpp \
	CallPrinter.cpp \
	CaptureTracking.cpp \
	CostModel.cpp \
	CodeMetrics.cpp \
	ConstantFolding.cpp \
	Delinearization.cpp \
	DemandedBits.cpp \
	DependenceAnalysis.cpp \
	DivergenceAnalysis.cpp \
	DomPrinter.cpp \
	DominanceFrontier.cpp \
	EHPersonalities.cpp \
	GlobalsModRef.cpp \
	IVUsers.cpp \
	InlineCost.cpp \
	InstCount.cpp \
	InstructionSimplify.cpp \
	Interval.cpp \
	IntervalPartition.cpp \
	IteratedDominanceFrontier.cpp \
	LazyCallGraph.cpp \
	LazyValueInfo.cpp \
	Lint.cpp \
	Loads.cpp \
	LoopAccessAnalysis.cpp \
	LoopInfo.cpp \
	LoopPass.cpp \
	MemDepPrinter.cpp \
	MemDerefPrinter.cpp \
	MemoryBuiltins.cpp \
	MemoryDependenceAnalysis.cpp \
	MemoryLocation.cpp \
	ModuleDebugInfoPrinter.cpp \
	ObjCARCAliasAnalysis.cpp \
	ObjCARCAnalysisUtils.cpp \
	ObjCARCInstKind.cpp \
	OrderedBasicBlock.cpp \
	PHITransAddr.cpp \
	PostDominators.cpp \
	PtrUseVisitor.cpp \
	RegionInfo.cpp \
	RegionPass.cpp \
	RegionPrinter.cpp \
	ScalarEvolution.cpp \
	ScalarEvolutionAliasAnalysis.cpp \
	ScalarEvolutionExpander.cpp \
	ScalarEvolutionNormalization.cpp \
	SparsePropagation.cpp \
	TargetLibraryInfo.cpp \
	TargetTransformInfo.cpp \
	Trace.cpp \
	TypeBasedAliasAnalysis.cpp \
	ScopedNoAliasAA.cpp \
	ValueTracking.cpp \
	VectorUtils.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Analysis

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
