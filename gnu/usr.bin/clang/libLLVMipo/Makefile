# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

LIB=	LLVMipo
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/IPO

.include <bsd.own.mk>
SRCS=	ArgumentPromotion.cpp \
	BarrierNoopPass.cpp \
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
	IPConstantPropagation.cpp \
	IPO.cpp \
	InferFunctionAttrs.cpp \
	InlineAlways.cpp \
	InlineSimple.cpp \
	Inliner.cpp \
	Internalize.cpp \
	LoopExtractor.cpp \
	LowerBitSets.cpp \
	MergeFunctions.cpp \
	PartialInlining.cpp \
	PassManagerBuilder.cpp \
	PruneEH.cpp \
	SampleProfile.cpp \
	StripDeadPrototypes.cpp \
	StripSymbols.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/IPO

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
