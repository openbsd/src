# $OpenBSD: Makefile,v 1.15 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMAnalysis
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Analysis

SRCS+=	AliasAnalysis.cpp \
	AliasAnalysisEvaluator.cpp \
	AliasSetTracker.cpp \
	AnalysisAnalysis.cpp \
	AssumeBundleQueries.cpp \
	AssumptionCache.cpp \
	BasicAliasAnalysis.cpp \
	BlockFrequencyInfo.cpp \
	BlockFrequencyInfoImpl.cpp \
	BranchProbabilityInfo.cpp \
	CFG.cpp \
	CFGPrinter.cpp \
	CFGSCCPrinter.cpp \
	CGSCCPassManager.cpp \
	CallGraph.cpp \
	CallGraphSCCPass.cpp \
	CallPrinter.cpp \
	CaptureTracking.cpp \
	CmpInstAnalysis.cpp \
	CostModel.cpp \
	CodeMetrics.cpp \
	ConstantFolding.cpp \
	CycleAnalysis.cpp \
	DDG.cpp \
	DDGPrinter.cpp \
	ConstraintSystem.cpp \
	Delinearization.cpp \
	DemandedBits.cpp \
	DependenceAnalysis.cpp \
	DependenceGraphBuilder.cpp \
	DevelopmentModeInlineAdvisor.cpp \
	DomConditionCache.cpp \
	DomPrinter.cpp \
	DomTreeUpdater.cpp \
	DominanceFrontier.cpp \
	FunctionPropertiesAnalysis.cpp \
	GlobalsModRef.cpp \
	GuardUtils.cpp \
	HeatUtils.cpp \
	IRSimilarityIdentifier.cpp \
	IVDescriptors.cpp \
	IVUsers.cpp \
	ImportedFunctionsInliningStatistics.cpp \
	IndirectCallPromotionAnalysis.cpp \
	InlineCost.cpp \
	InlineAdvisor.cpp \
	InlineOrder.cpp \
	InlineSizeEstimatorAnalysis.cpp \
	InstCount.cpp \
	InstructionPrecedenceTracking.cpp \
	InstructionSimplify.cpp \
	InteractiveModelRunner.cpp \
	LazyBranchProbabilityInfo.cpp \
	LazyBlockFrequencyInfo.cpp \
	LazyCallGraph.cpp \
	LazyValueInfo.cpp \
	Lint.cpp \
	Loads.cpp \
	Local.cpp \
	LoopAccessAnalysis.cpp \
	LoopAnalysisManager.cpp \
	LoopCacheAnalysis.cpp \
	LoopNestAnalysis.cpp \
	LoopUnrollAnalyzer.cpp \
	LoopInfo.cpp \
	LoopPass.cpp \
	MLInlineAdvisor.cpp \
	MemDerefPrinter.cpp \
	MemoryBuiltins.cpp \
	MemoryDependenceAnalysis.cpp \
	MemoryLocation.cpp \
	MemoryProfileInfo.cpp \
	MemorySSA.cpp \
	MemorySSAUpdater.cpp \
	ModelUnderTrainingRunner.cpp \
	ModuleDebugInfoPrinter.cpp \
	ModuleSummaryAnalysis.cpp \
	MustExecute.cpp \
	NoInferenceModelRunner.cpp \
	ObjCARCAliasAnalysis.cpp \
	ObjCARCAnalysisUtils.cpp \
	ObjCARCInstKind.cpp \
	OptimizationRemarkEmitter.cpp \
	OverflowInstAnalysis.cpp \
	PHITransAddr.cpp \
	PhiValues.cpp \
	PostDominators.cpp \
	ProfileSummaryInfo.cpp \
	PtrUseVisitor.cpp \
	RegionInfo.cpp \
	RegionPass.cpp \
	RegionPrinter.cpp \
	ReplayInlineAdvisor.cpp \
	ScalarEvolution.cpp \
	ScalarEvolutionAliasAnalysis.cpp \
	ScalarEvolutionDivision.cpp \
	ScalarEvolutionNormalization.cpp \
	StackLifetime.cpp \
	StackSafetyAnalysis.cpp \
	AnalysisStructuralHash.cpp \
	SyntheticCountsUtils.cpp \
	TFLiteUtils.cpp \
	TargetLibraryInfo.cpp \
	TargetTransformInfo.cpp \
	TensorSpec.cpp \
	Trace.cpp \
	TrainingLogger.cpp \
	TypeBasedAliasAnalysis.cpp \
	TypeMetadataUtils.cpp \
	UniformityAnalysis.cpp \
	ScopedNoAliasAA.cpp \
	ValueLattice.cpp \
	ValueLatticeUtils.cpp \
	ValueTracking.cpp \
	VectorUtils.cpp

AnalysisAnalysis.cpp:
	ln -s ${LLVM_SRCS}/lib/Analysis/Analysis.cpp $@

AnalysisStructuralHash.cpp:
	ln -s ${LLVM_SRCS}/lib/Analysis/StructuralHash.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Analysis

CLEANFILES+= AnalysisAnalysis.cpp
