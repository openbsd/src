# $OpenBSD: Makefile,v 1.9 2019/06/23 22:07:39 patrick Exp $

LIB=	LLVMCore
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/IR

SRCS+=	AsmWriter.cpp \
	Attributes.cpp \
	AutoUpgrade.cpp \
	BasicBlock.cpp \
	Comdat.cpp \
	ConstantFold.cpp \
	ConstantRange.cpp \
	Constants.cpp \
	CoreCore.cpp \
	DIBuilder.cpp \
	DataLayout.cpp \
	DebugInfo.cpp \
	DebugInfoMetadata.cpp \
	DebugLoc.cpp \
	DiagnosticHandler.cpp \
	DiagnosticInfo.cpp \
	DiagnosticPrinter.cpp \
	DomTreeUpdater.cpp \
	Dominators.cpp \
	Function.cpp \
	GVMaterializer.cpp \
	Globals.cpp \
	IRBuilder.cpp \
	IRPrintingPasses.cpp \
	InlineAsm.cpp \
	Instruction.cpp \
	Instructions.cpp \
	IntrinsicInst.cpp \
	LLVMContext.cpp \
	LLVMContextImpl.cpp \
	LegacyPassManager.cpp \
	MDBuilder.cpp \
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
	ProfileSummary.cpp \
	SafepointIRVerifier.cpp \
	Statepoint.cpp \
	Type.cpp \
	TypeFinder.cpp \
	Use.cpp \
	User.cpp \
	Value.cpp \
	ValueSymbolTable.cpp \
	Verifier.cpp

CoreCore.cpp:
	ln -s ${LLVM_SRCS}/lib/IR/Core.cpp $@

.PATH:	${.CURDIR}/../../../llvm/lib/IR

Attributes.o: AttributesCompatFunc.inc

AttributesCompatFunc.inc: AttributesCompatFunc.td
	${.OBJDIR}/../llvm-tblgen/llvm-tblgen -gen-attrs \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

CLEANFILES+= CoreCore.cpp
