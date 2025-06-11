# $OpenBSD: Makefile,v 1.12 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMVectorize
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms \
	-I${LLVM_SRCS}/include/llvm/Transforms/Vectorize

SRCS+=	LoadStoreVectorizer.cpp \
	LoopIdiomVectorize.cpp \
	LoopVectorizationLegality.cpp \
	LoopVectorize.cpp \
	SLPVectorizer.cpp \
	Vectorize.cpp \
	VectorCombine.cpp \
	VPlan.cpp \
	VPlanAnalysis.cpp \
	VPlanHCFGBuilder.cpp \
	VPlanRecipes.cpp \
	VPlanSLP.cpp \
	VPlanTransforms.cpp \
	VPlanVerifier.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Vectorize
