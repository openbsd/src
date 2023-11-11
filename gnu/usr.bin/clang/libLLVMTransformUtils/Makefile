# $OpenBSD: Makefile,v 1.15 2023/11/11 18:35:38 robert Exp $

LIB=	LLVMTransformUtils
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/Utils

SRCS+=	AddDiscriminators.cpp \
	AMDGPUEmitPrintf.cpp \
	ASanStackFrameLayout.cpp \
	AssumeBundleBuilder.cpp \
	BasicBlockUtils.cpp \
	BreakCriticalEdges.cpp \
	BuildLibCalls.cpp \
	BypassSlowDivision.cpp \
	CallPromotionUtils.cpp \
	CallGraphUpdater.cpp \
	CanonicalizeAliases.cpp \
	CanonicalizeFreezeInLoops.cpp \
	CloneFunction.cpp \
	CloneModule.cpp \
	CodeExtractor.cpp \
	CodeLayout.cpp \
	CodeMoverUtils.cpp \
	CtorUtils.cpp \
	Debugify.cpp \
	DemoteRegToStack.cpp \
	EntryExitInstrumenter.cpp \
	EscapeEnumerator.cpp \
	Evaluator.cpp \
	FixIrreducible.cpp \
	FlattenCFG.cpp \
	FunctionComparator.cpp \
	FunctionImportUtils.cpp \
	GlobalStatus.cpp \
	TransformUtilsGuardUtils.cpp \
	HelloWorld.cpp \
	InlineFunction.cpp \
	InjectTLIMappings.cpp \
	InstructionNamer.cpp \
	IntegerDivision.cpp \
	LCSSA.cpp \
	LibCallsShrinkWrap.cpp \
	TransformUtilsLocal.cpp \
	LoopPeel.cpp \
	LoopRotationUtils.cpp \
	LoopSimplify.cpp \
	LoopUnroll.cpp \
	LoopUnrollAndJam.cpp \
	LoopUnrollRuntime.cpp \
	LoopUtils.cpp \
	LoopVersioning.cpp \
	LowerAtomic.cpp \
	LowerGlobalDtors.cpp \
	LowerIFunc.cpp \
	LowerInvoke.cpp \
	LowerMemIntrinsics.cpp \
	LowerSwitch.cpp \
	MatrixUtils.cpp \
	MemoryOpRemark.cpp \
	MemoryTaggingSupport.cpp \
	Mem2Reg.cpp \
	MetaRenamer.cpp \
	MisExpect.cpp \
	ModuleUtils.cpp \
	NameAnonGlobals.cpp \
	PredicateInfo.cpp \
	PromoteMemoryToRegister.cpp \
	RelLookupTableConverter.cpp \
	ScalarEvolutionExpander.cpp \
	SCCPSolver.cpp \
	StripGCRelocates.cpp \
	SSAUpdater.cpp \
	SSAUpdaterBulk.cpp \
	SampleProfileInference.cpp \
	SampleProfileLoaderBaseUtil.cpp \
	SanitizerStats.cpp \
	SimplifyCFG.cpp \
	SimplifyIndVar.cpp \
	SimplifyLibCalls.cpp \
	SizeOpts.cpp \
	SplitModule.cpp \
	StripNonLineTableDebugInfo.cpp \
	SymbolRewriter.cpp \
	UnifyFunctionExitNodes.cpp \
	UnifyLoopExits.cpp \
	TransformUtilsUtils.cpp \
	ValueMapper.cpp \
	VNCoercion.cpp

TransformUtilsGuardUtils.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/Utils/GuardUtils.cpp $@

TransformUtilsLocal.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/Utils/Local.cpp $@

TransformUtilsUtils.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/Utils/Utils.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Utils

CLEANFILES+= TransformUtilsUtils.cpp TransformUtilsGuardUtils.cpp TransformUtilsLocal.cpp
