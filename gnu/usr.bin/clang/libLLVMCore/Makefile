# $OpenBSD: Makefile,v 1.4 2018/04/06 14:44:04 patrick Exp $

LIB=	LLVMCore
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/IR

.include <bsd.own.mk>
SRCS=	AsmWriter.cpp \
	Attributes.cpp \
	AutoUpgrade.cpp \
	BasicBlock.cpp \
	Comdat.cpp \
	ConstantFold.cpp \
	ConstantRange.cpp \
	Constants.cpp \
	Core.cpp \
	DIBuilder.cpp \
	DataLayout.cpp \
	DebugInfo.cpp \
	DebugInfoMetadata.cpp \
	DebugLoc.cpp \
	DiagnosticHandler.cpp \
	DiagnosticInfo.cpp \
	DiagnosticPrinter.cpp \
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
	PassManager.cpp \
	PassRegistry.cpp \
	ProfileSummary.cpp \
	Statepoint.cpp \
	Type.cpp \
	TypeFinder.cpp \
	Use.cpp \
	User.cpp \
	Value.cpp \
	ValueSymbolTable.cpp \
	ValueTypes.cpp \
	Verifier.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/IR

Attributes.o: AttributesCompatFunc.inc

AttributesCompatFunc.inc: AttributesCompatFunc.td
	${.OBJDIR}/../llvm-tblgen/llvm-tblgen -gen-attrs \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
