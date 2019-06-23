# $OpenBSD: Makefile,v 1.11 2019/06/23 22:07:41 patrick Exp $

LIB=	LLVMTransformUtils
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/Utils

SRCS+=	ASanStackFrameLayout.cpp \
	AddDiscriminators.cpp \
	BasicBlockUtils.cpp \
	BreakCriticalEdges.cpp \
	BuildLibCalls.cpp \
	BypassSlowDivision.cpp \
	CallPromotionUtils.cpp \
	CanonicalizeAliases.cpp \
	CloneFunction.cpp \
	CloneModule.cpp \
	CodeExtractor.cpp \
	CtorUtils.cpp \
	DemoteRegToStack.cpp \
	EntryExitInstrumenter.cpp \
	EscapeEnumerator.cpp \
	Evaluator.cpp \
	FlattenCFG.cpp \
	FunctionComparator.cpp \
	FunctionImportUtils.cpp \
	GlobalStatus.cpp \
	TransformUtilsGuardUtils.cpp \
	ImportedFunctionsInliningStatistics.cpp \
	InlineFunction.cpp \
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
	ModuleUtils.cpp \
	NameAnonGlobals.cpp \
	PredicateInfo.cpp \
	PromoteMemoryToRegister.cpp \
	StripGCRelocates.cpp \
	SSAUpdater.cpp \
	SanitizerStats.cpp \
	SimplifyCFG.cpp \
	SimplifyIndVar.cpp \
	SimplifyLibCalls.cpp \
	SplitModule.cpp \
	StripNonLineTableDebugInfo.cpp \
	SymbolRewriter.cpp \
	UnifyFunctionExitNodes.cpp \
	TransformUtilsUtils.cpp \
	VNCoercion.cpp \
	ValueMapper.cpp

TransformUtilsGuardUtils.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/Utils/GuardUtils.cpp $@

TransformUtilsUtils.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/Utils/Utils.cpp $@

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/Utils

CLEANFILES+= TransformUtilsUtils.cpp
