# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:26 patrick Exp $

LIB=	LLVMObjCARCOpts
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

SRCS+=	ObjCARC.cpp \
	ObjCARCOpts.cpp \
	ObjCARCExpand.cpp \
	ObjCARCAPElim.cpp \
	ObjCARCContract.cpp \
	DependencyAnalysis.cpp \
	ProvenanceAnalysis.cpp \
	ProvenanceAnalysisEvaluator.cpp \
	PtrState.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/ObjCARC
