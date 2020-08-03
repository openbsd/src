# $OpenBSD: Makefile,v 1.6 2020/08/03 14:45:28 patrick Exp $

LIB=	clangAnalysis
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	AnalysisDeclContext.cpp \
	BodyFarm.cpp \
	CFG.cpp \
	CFGReachabilityAnalysis.cpp \
	CFGStmtMap.cpp \
	CallGraph.cpp \
	CocoaConventions.cpp \
	ConstructionContext.cpp \
	Consumed.cpp \
	CodeInjector.cpp \
	Dominators.cpp \
	ExprMutationAnalyzer.cpp \
	LiveVariables.cpp \
	ObjCNoReturn.cpp \
	PathDiagnostic.cpp \
	PostOrderCFGView.cpp \
	ProgramPoint.cpp \
	ReachableCode.cpp \
	RetainSummaryManager.cpp \
	ThreadSafety.cpp \
	ThreadSafetyCommon.cpp \
	ThreadSafetyLogical.cpp \
	ThreadSafetyTIL.cpp \
	UninitializedValues.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Analysis

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
