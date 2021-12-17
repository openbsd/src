# $OpenBSD: Makefile,v 1.11 2021/12/17 14:55:45 patrick Exp $

LIB=	LLVMInstrumentation
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

SRCS+=	AddressSanitizer.cpp \
	BoundsChecking.cpp \
	CGProfile.cpp \
	ControlHeightReduction.cpp \
	DataFlowSanitizer.cpp \
	GCOVProfiling.cpp \
	MemProfiler.cpp \
	MemorySanitizer.cpp \
	IndirectCallPromotion.cpp \
	Instrumentation.cpp \
	InstrOrderFile.cpp \
	InstrProfiling.cpp \
	PGOInstrumentation.cpp \
	PGOMemOPSizeOpt.cpp \
	PoisonChecking.cpp \
	SanitizerCoverage.cpp \
	ValueProfileCollector.cpp \
	ThreadSanitizer.cpp \
	HWAddressSanitizer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Instrumentation
