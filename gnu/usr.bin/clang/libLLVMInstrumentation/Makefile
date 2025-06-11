# $OpenBSD: Makefile,v 1.14 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMInstrumentation
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

SRCS+=	AddressSanitizer.cpp \
	BoundsChecking.cpp \
	CGProfile.cpp \
	ControlHeightReduction.cpp \
	DataFlowSanitizer.cpp \
	GCOVProfiling.cpp \
	BlockCoverageInference.cpp \
	MemProfiler.cpp \
	MemorySanitizer.cpp \
	NumericalStabilitySanitizer.cpp \
	IndirectCallPromotion.cpp \
	Instrumentation.cpp \
	InstrOrderFile.cpp \
	InstrProfiling.cpp \
	InstrumentationKCFI.cpp \
	LowerAllowCheckPass.cpp \
	PGOCtxProfLowering.cpp \
	PGOForceFunctionAttrs.cpp \
	PGOInstrumentation.cpp \
	PGOMemOPSizeOpt.cpp \
	PoisonChecking.cpp \
	SanitizerCoverage.cpp \
	InstrumentationSanitizerBinaryMetadata.cpp \
	ValueProfileCollector.cpp \
	ThreadSanitizer.cpp \
	HWAddressSanitizer.cpp

InstrumentationKCFI.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/Instrumentation/KCFI.cpp $@

InstrumentationSanitizerBinaryMetadata.cpp:
	ln -s ${LLVM_SRCS}/lib/Transforms/Instrumentation/SanitizerBinaryMetadata.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Instrumentation

CLEANFILES+= InstrumentationSanitizerBinaryMetadata.cpp
