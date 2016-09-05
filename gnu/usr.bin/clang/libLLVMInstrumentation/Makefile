# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMInstrumentation
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

.include <bsd.own.mk>
SRCS=	AddressSanitizer.cpp \
	BoundsChecking.cpp \
	DataFlowSanitizer.cpp \
	GCOVProfiling.cpp \
	MemorySanitizer.cpp \
	Instrumentation.cpp \
	InstrProfiling.cpp \
	PGOInstrumentation.cpp \
	SafeStack.cpp \
	SanitizerCoverage.cpp \
	ThreadSanitizer.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/Instrumentation

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
