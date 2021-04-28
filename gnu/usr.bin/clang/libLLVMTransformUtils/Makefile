# $OpenBSD: Makefile,v 1.13 2021/04/28 12:55:40 patrick Exp $

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
	InlineFunction.cpp \
	ImportedFunctionsInliningStatistics.cpp \
	InjectTLIMappings.cpp \
	InstructionNamer.cpp \
	IntegerDivision.cpp \
	LCSSA.cpp \
	LibCallsShrinkWrap.cpp \
	Local.cpp \
	LoopRotationUtils.cpp \
	LoopSimplify.cpp \
	LoopUnroll.cpp \
	LoopUnrollAndJam.cpp \
	LoopUnrollPeel.cpp \
	LoopUnrollRuntime.cpp \
	LoopUtils.cpp \
	LoopVersioning.cpp \
	LowerInvoke.cpp \
	LowerMemIntrinsics.cpp \
	LowerSwitch.cpp \
	Mem2Reg.cpp \
	MetaRenamer.cpp \
	MisExpect.cpp \
	ModuleUtils.cpp \
	NameAnonGlobals.cpp \
	PredicateInfo.cpp \
	PromoteMemoryToRegister.cpp \
	ScalarEvolutionExpander.cpp \
	StripGCRelocates.cpp \
	SSAUpdater.cpp \
	SSAUpdaterBulk.cpp \
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
	UniqueInternalLinkageNames.cpp \
	TransformUtilsUtils.cpp \
	ValueMapper.cpp \
	VNCoercion.cpp

TransformUtilsGuardUtils.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/Utils/GuardUtils.cpp $@

TransformUtilsUtils.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/Utils/Utils.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Utils

CLEANFILES+= TransformUtilsUtils.cpp
