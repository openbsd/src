# $OpenBSD: Makefile,v 1.5 2018/04/06 14:44:04 patrick Exp $

LIB=	LLVMInstrumentation
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

.include <bsd.own.mk>
SRCS=	AddressSanitizer.cpp \
	BoundsChecking.cpp \
	DataFlowSanitizer.cpp \
	EfficiencySanitizer.cpp \
	GCOVProfiling.cpp \
	HWAddressSanitizer.cpp \
	IndirectCallPromotion.cpp \
	Instrumentation.cpp \
	InstrProfiling.cpp \
	MemorySanitizer.cpp \
	PGOInstrumentation.cpp \
	PGOMemOPSizeOpt.cpp \
	SanitizerCoverage.cpp \
	ThreadSanitizer.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/Instrumentation

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
