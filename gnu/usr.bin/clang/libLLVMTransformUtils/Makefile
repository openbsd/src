# $OpenBSD: Makefile,v 1.14 2021/12/17 14:55:46 patrick Exp $

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
	HelloWorld.cpp \
	InlineFunction.cpp \
	InjectTLIMappings.cpp \
	InstructionNamer.cpp \
	IntegerDivision.cpp \
	LCSSA.cpp \
	LibCallsShrinkWrap.cpp \
	Local.cpp \
	LoopPeel.cpp \
	LoopRotationUtils.cpp \
	LoopSimplify.cpp \
	LoopUnroll.cpp \
	LoopUnrollAndJam.cpp \
	LoopUnrollRuntime.cpp \
	LoopUtils.cpp \
	LoopVersioning.cpp \
	LowerInvoke.cpp \
	LowerMemIntrinsics.cpp \
	LowerSwitch.cpp \
	MatrixUtils.cpp \
	MemoryOpRemark.cpp \
	Mem2Reg.cpp \
	MetaRenamer.cpp \
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

TransformUtilsUtils.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/Utils/Utils.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Utils

CLEANFILES+= TransformUtilsUtils.cpp
