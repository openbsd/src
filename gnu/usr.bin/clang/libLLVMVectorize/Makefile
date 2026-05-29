# $OpenBSD: Makefile,v 1.13 2026/05/29 11:18:31 robert Exp $

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
	VPlanConstruction.cpp \
	VPlanPredicator.cpp \
	VPlanRecipes.cpp \
	VPlanSLP.cpp \
	VPlanTransforms.cpp \
	VPlanUnroll.cpp \
	VPlanVerifier.cpp \
	VPlanUtils.cpp

# SandboxVectorizer
SRCS+=	DependencyGraph.cpp \
	InstrMaps.cpp \
	Interval.cpp \
	Legality.cpp \
	SandboxVectorizer.cpp \
	SandboxVectorizerPassBuilder.cpp \
	Scheduler.cpp \
	SeedCollector.cpp \
	VecUtils.cpp

# SandboxVectorizer/Passes
SRCS+=	BottomUpVec.cpp \
	PackReuse.cpp \
	RegionsFromBBs.cpp \
	RegionsFromMetadata.cpp \
	SeedCollection.cpp \
	TransactionAcceptOrRevert.cpp \
	TransactionSave.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Vectorize
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Vectorize/SandboxVectorizer
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Vectorize/SandboxVectorizer/Passes
