# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMObjCARCOpts
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

.include <bsd.own.mk>
SRCS=	ObjCARC.cpp \
	ObjCARCOpts.cpp \
	ObjCARCExpand.cpp \
	ObjCARCAPElim.cpp \
	ObjCARCContract.cpp \
	DependencyAnalysis.cpp \
	ProvenanceAnalysis.cpp \
	ProvenanceAnalysisEvaluator.cpp \
	PtrState.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/ObjCARC

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
