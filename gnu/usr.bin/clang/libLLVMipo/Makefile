# $OpenBSD: Makefile,v 1.12 2020/08/03 14:45:28 patrick Exp $

LIB=	LLVMipo
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/IPO

SRCS+=	ArgumentPromotion.cpp \
	AlwaysInliner.cpp \
	Attributor.cpp \
	BarrierNoopPass.cpp \
	BlockExtractor.cpp \
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
	HotColdSplitting.cpp \
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
	ipoSCCP.cpp \
	SampleProfile.cpp \
	StripDeadPrototypes.cpp \
	StripSymbols.cpp \
	SyntheticCountsPropagation.cpp \
	ThinLTOBitcodeWriter.cpp \
	WholeProgramDevirt.cpp

ipoSCCP.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/IPO/SCCP.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/IPO

CLEANFILES+= ipoSCCP.cpp
