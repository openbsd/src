# $OpenBSD: Makefile,v 1.6 2018/04/06 14:44:04 patrick Exp $

LIB=	LLVMipo
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/IPO

.include <bsd.own.mk>
SRCS=	ArgumentPromotion.cpp \
	AlwaysInliner.cpp \
	BarrierNoopPass.cpp \
	CalledValuePropagation.cpp \
	ConstantMerge.cpp \
	CrossDSOCFI.cpp \
	DeadArgumentElimination.cpp \
	ElimAvailExtern.cpp \
	ExtractGV.cpp \
	ForceFunctionAttrs.cpp \
	FunctionAttrs.cpp \
	FunctionImport.cpp \
	GlobalDCE.cpp \
	GlobalOpt.cpp \
	GlobalSplit.cpp \
	IPConstantPropagation.cpp \
	IPO.cpp \
	InferFunctionAttrs.cpp \
	InlineSimple.cpp \
	Inliner.cpp \
	Internalize.cpp \
	LoopExtractor.cpp \
	LowerTypeTests.cpp \
	MergeFunctions.cpp \
	PartialInlining.cpp \
	PassManagerBuilder.cpp \
	PruneEH.cpp \
	SampleProfile.cpp \
	StripDeadPrototypes.cpp \
	StripSymbols.cpp \
	ThinLTOBitcodeWriter.cpp \
	WholeProgramDevirt.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/IPO

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
