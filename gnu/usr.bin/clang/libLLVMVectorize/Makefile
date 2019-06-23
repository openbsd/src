# $OpenBSD: Makefile,v 1.8 2019/06/23 22:07:41 patrick Exp $

LIB=	LLVMVectorize
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

SRCS+=	LoadStoreVectorizer.cpp \
	LoopVectorizationLegality.cpp \
	LoopVectorize.cpp \
	SLPVectorizer.cpp \
	VPlan.cpp \
	VPlanHCFGBuilder.cpp \
	VPlanHCFGTransforms.cpp \
	VPlanSLP.cpp \
	VPlanVerifier.cpp \
	Vectorize.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/Vectorize
