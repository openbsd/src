# $OpenBSD: Makefile,v 1.14 2021/12/17 14:55:46 patrick Exp $

LIB=	LLVMipo
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/IPO

SRCS+=	AlwaysInliner.cpp \
	Annotation2Metadata.cpp \
	ArgumentPromotion.cpp \
	Attributor.cpp \
	AttributorAttributes.cpp \
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
	FunctionSpecialization.cpp \
	GlobalDCE.cpp \
	GlobalOpt.cpp \
	GlobalSplit.cpp \
	HotColdSplitting.cpp \
	IPO.cpp \
	IROutliner.cpp \
	InferFunctionAttrs.cpp \
	InlineSimple.cpp \
	Inliner.cpp \
	Internalize.cpp \
	LoopExtractor.cpp \
	LowerTypeTests.cpp \
	MergeFunctions.cpp \
	OpenMPOpt.cpp \
	PartialInlining.cpp \
	PassManagerBuilder.cpp \
	PruneEH.cpp \
	SampleContextTracker.cpp \
	SampleProfile.cpp \
	SampleProfileProbe.cpp \
	ipoSCCP.cpp \
	StripDeadPrototypes.cpp \
	StripSymbols.cpp \
	SyntheticCountsPropagation.cpp \
	ThinLTOBitcodeWriter.cpp \
	WholeProgramDevirt.cpp

ipoSCCP.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/IPO/SCCP.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/IPO

CLEANFILES+= ipoSCCP.cpp
