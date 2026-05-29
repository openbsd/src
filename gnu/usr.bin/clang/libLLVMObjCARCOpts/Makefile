# $OpenBSD: Makefile,v 1.5 2026/05/29 11:18:31 robert Exp $

LIB=	LLVMObjCARCOpts
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

SRCS+=	ObjCARC.cpp \
	ObjCARCOpts.cpp \
	ObjCARCExpand.cpp \
	ObjCARCContract.cpp \
	DependencyAnalysis.cpp \
	ProvenanceAnalysis.cpp \
	ProvenanceAnalysisEvaluator.cpp \
	PtrState.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/ObjCARC
