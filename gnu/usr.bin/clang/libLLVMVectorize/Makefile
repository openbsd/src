# $OpenBSD: Makefile,v 1.11 2023/11/11 18:35:38 robert Exp $

LIB=	LLVMVectorize
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms \
	-I${LLVM_SRCS}/include/llvm/Transforms/Vectorize

SRCS+=	LoadStoreVectorizer.cpp \
	LoopVectorizationLegality.cpp \
	LoopVectorize.cpp \
	SLPVectorizer.cpp \
	Vectorize.cpp \
	VectorCombine.cpp \
	VPlan.cpp \
	VPlanHCFGBuilder.cpp \
	VPlanRecipes.cpp \
	VPlanSLP.cpp \
	VPlanTransforms.cpp \
	VPlanVerifier.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Vectorize
