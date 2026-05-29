# $OpenBSD: Makefile,v 1.17 2026/05/29 11:18:31 robert Exp $

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
	EmbedBitcodePass.cpp \
	ExpandVariadics.cpp \
	ExtractGV.cpp \
	FatLTOCleanup.cpp \
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
	Inliner.cpp \
	Internalize.cpp \
	LoopExtractor.cpp \
	LowerTypeTests.cpp \
	MemProfContextDisambiguation.cpp \
	MergeFunctions.cpp \
	ModuleInliner.cpp \
	OpenMPOpt.cpp \
	PartialInlining.cpp \
	SampleContextTracker.cpp \
	SampleProfile.cpp \
	SampleProfileMatcher.cpp \
	SampleProfileProbe.cpp \
	ipoSCCP.cpp \
	StripDeadPrototypes.cpp \
	StripSymbols.cpp \
	ThinLTOBitcodeWriter.cpp \
	WholeProgramDevirt.cpp

ipoSCCP.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/IPO/SCCP.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/IPO

CLEANFILES+= ipoSCCP.cpp
