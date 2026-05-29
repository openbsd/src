# $OpenBSD: Makefile,v 1.15 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMInstrumentation
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

SRCS+=	AddressSanitizer.cpp \
	AllocToken.cpp \
	BoundsChecking.cpp \
	CGProfile.cpp \
	ControlHeightReduction.cpp \
	DataFlowSanitizer.cpp \
	GCOVProfiling.cpp \
	BlockCoverageInference.cpp \
	MemProfInstrumentation.cpp \
	MemProfUse.cpp \
	MemorySanitizer.cpp \
	NumericalStabilitySanitizer.cpp \
	IndirectCallPromotion.cpp \
	InstrProfiling.cpp \
	InstrumentationKCFI.cpp \
	LowerAllowCheckPass.cpp \
	PGOCtxProfFlattening.cpp \
	PGOCtxProfLowering.cpp \
	PGOForceFunctionAttrs.cpp \
	PGOInstrumentation.cpp \
	PGOMemOPSizeOpt.cpp \
	SanitizerCoverage.cpp \
	InstrumentationSanitizerBinaryMetadata.cpp \
	ValueProfileCollector.cpp \
	ThreadSanitizer.cpp \
	TypeSanitizer.cpp \
	HWAddressSanitizer.cpp \
	RealtimeSanitizer.cpp

InstrumentationKCFI.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/Instrumentation/KCFI.cpp $@

InstrumentationSanitizerBinaryMetadata.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/Instrumentation/SanitizerBinaryMetadata.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Instrumentation

CLEANFILES+= InstrumentationSanitizerBinaryMetadata.cpp
