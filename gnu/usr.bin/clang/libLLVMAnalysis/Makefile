# $OpenBSD: Makefile,v 1.11 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMAnalysis
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Analysis

SRCS+=	AliasAnalysis.cpp \
	AliasAnalysisEvaluator.cpp \
	AliasAnalysisSummary.cpp \
	AliasSetTracker.cpp \
	AnalysisAnalysis.cpp \
	AssumptionCache.cpp \
	BasicAliasAnalysis.cpp \
	BlockFrequencyInfo.cpp \
	BlockFrequencyInfoImpl.cpp \
	BranchProbabilityInfo.cpp \
	CFG.cpp \
	CFGPrinter.cpp \
	CFLAndersAliasAnalysis.cpp \
	CFLSteensAliasAnalysis.cpp \
	CGSCCPassManager.cpp \
	CallGraph.cpp \
	CallGraphSCCPass.cpp \
	CallPrinter.cpp \
	CaptureTracking.cpp \
	CmpInstAnalysis.cpp \
	CostModel.cpp \
	CodeMetrics.cpp \
	ConstantFolding.cpp \
	DDG.cpp \
	Delinearization.cpp \
	DemandedBits.cpp \
	DependenceAnalysis.cpp \
	DependenceGraphBuilder.cpp \
	DivergenceAnalysis.cpp \
	DomPrinter.cpp \
	DomTreeUpdater.cpp \
	DominanceFrontier.cpp \
	EHPersonalities.cpp \
	GlobalsModRef.cpp \
	GuardUtils.cpp \
	IVDescriptors.cpp \
	IVUsers.cpp \
	IndirectCallPromotionAnalysis.cpp \
	InlineCost.cpp \
	InstCount.cpp \
	InstructionPrecedenceTracking.cpp \
	InstructionSimplify.cpp \
	Interval.cpp \
	IntervalPartition.cpp \
	LazyBlockFrequencyInfo.cpp \
	LazyBranchProbabilityInfo.cpp \
	LazyCallGraph.cpp \
	LazyValueInfo.cpp \
	LegacyDivergenceAnalysis.cpp \
	Lint.cpp \
	Loads.cpp \
	LoopAccessAnalysis.cpp \
	LoopAnalysisManager.cpp \
	LoopCacheAnalysis.cpp \
	LoopInfo.cpp \
	LoopPass.cpp \
	LoopUnrollAnalyzer.cpp \
	MemDepPrinter.cpp \
	MemDerefPrinter.cpp \
	MemoryBuiltins.cpp \
	MemoryDependenceAnalysis.cpp \
	MemoryLocation.cpp \
	MemorySSA.cpp \
	MemorySSAUpdater.cpp \
	ModuleDebugInfoPrinter.cpp \
	ModuleSummaryAnalysis.cpp \
	MustExecute.cpp \
	ObjCARCAliasAnalysis.cpp \
	ObjCARCAnalysisUtils.cpp \
	ObjCARCInstKind.cpp \
	OptimizationRemarkEmitter.cpp \
	OrderedBasicBlock.cpp \
	OrderedInstructions.cpp \
	PHITransAddr.cpp \
	PhiValues.cpp \
	PostDominators.cpp \
	ProfileSummaryInfo.cpp \
	PtrUseVisitor.cpp \
	RegionInfo.cpp \
	RegionPass.cpp \
	RegionPrinter.cpp \
	ScalarEvolution.cpp \
	ScalarEvolutionAliasAnalysis.cpp \
	ScalarEvolutionExpander.cpp \
	ScalarEvolutionNormalization.cpp \
	StackSafetyAnalysis.cpp \
	SyncDependenceAnalysis.cpp \
	SyntheticCountsUtils.cpp \
	TargetLibraryInfo.cpp \
	TargetTransformInfo.cpp \
	Trace.cpp \
	TypeBasedAliasAnalysis.cpp \
	TypeMetadataUtils.cpp \
	ScopedNoAliasAA.cpp \
	ValueLattice.cpp \
	ValueLatticeUtils.cpp \
	ValueTracking.cpp \
	VectorUtils.cpp \
	VFABIDemangling.cpp

AnalysisAnalysis.cpp:
	ln -s ${LLVM_SRCS}/lib/Analysis/Analysis.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Analysis

CLEANFILES+= AnalysisAnalysis.cpp
