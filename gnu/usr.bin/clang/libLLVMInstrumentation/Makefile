# $OpenBSD: Makefile,v 1.4 2017/10/04 20:57:09 patrick Exp $

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
	MemorySanitizer.cpp \
	IndirectCallPromotion.cpp \
	Instrumentation.cpp \
	InstrProfiling.cpp \
	PGOInstrumentation.cpp \
	PGOMemOPSizeOpt.cpp \
	SanitizerCoverage.cpp \
	ThreadSanitizer.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/Instrumentation

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
