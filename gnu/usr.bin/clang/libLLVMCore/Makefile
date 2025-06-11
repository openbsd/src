# $OpenBSD: Makefile,v 1.14 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMCore
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/IR \
		-I${LLVM_SRCS}/include/llvm/IR

SRCS+=	AbstractCallSite.cpp \
	AsmWriter.cpp \
	Assumptions.cpp \
	Attributes.cpp \
	AutoUpgrade.cpp \
	BasicBlock.cpp \
	BuiltinGCs.cpp \
	Comdat.cpp \
	ConstantFold.cpp \
	ConstantRange.cpp \
	ConstantRangeList.cpp \
	Constants.cpp \
	ConvergenceVerifier.cpp \
	CoreCore.cpp \
	CycleInfo.cpp \
	DIBuilder.cpp \
	DataLayout.cpp \
	DebugInfo.cpp \
	DebugInfoMetadata.cpp \
	DIExpressionOptimizer.cpp \
	DebugLoc.cpp \
	DebugProgramInstruction.cpp \
	DiagnosticHandler.cpp \
	DiagnosticInfo.cpp \
	DiagnosticPrinter.cpp \
	Dominators.cpp \
	EHPersonalities.cpp \
	FPEnv.cpp \
	Function.cpp \
	GCStrategy.cpp \
	GVMaterializer.cpp \
	Globals.cpp \
	IRBuilder.cpp \
	IRPrintingPasses.cpp \
	SSAContext.cpp \
	InlineAsm.cpp \
	Instruction.cpp \
	Instructions.cpp \
	IntrinsicInst.cpp \
	LLVMContext.cpp \
	LLVMContextImpl.cpp \
	LLVMRemarkStreamer.cpp \
	LegacyPassManager.cpp \
	MDBuilder.cpp \
	MemoryModelRelaxationAnnotations.cpp \
	Mangler.cpp \
	Metadata.cpp \
	Module.cpp \
	ModuleSummaryIndex.cpp \
	Operator.cpp \
	OptBisect.cpp \
	Pass.cpp \
	PassInstrumentation.cpp \
	PassManager.cpp \
	PassRegistry.cpp \
	PassTimingInfo.cpp \
	PrintPasses.cpp \
	ProfDataUtils.cpp \
	RuntimeLibcalls.cpp \
	SafepointIRVerifier.cpp \
	ProfileSummary.cpp \
	PseudoProbe.cpp \
	ReplaceConstant.cpp \
	Statepoint.cpp \
	CoreStructuralHash.cpp \
	Type.cpp \
	TypedPointerType.cpp \
	TypeFinder.cpp \
	Use.cpp \
	User.cpp \
	Value.cpp \
	ValueSymbolTable.cpp \
	VectorBuilder.cpp \
	VFABIDemangler.cpp \
	Verifier.cpp


CoreCore.cpp:
	ln -s ${LLVM_SRCS}/lib/IR/Core.cpp $@

CoreStructuralHash.cpp:
	ln -s ${LLVM_SRCS}/lib/IR/StructuralHash.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/IR

CLEANFILES+= CoreCore.cpp
