# $OpenBSD: Makefile,v 1.9 2023/11/11 18:35:38 robert Exp $

LIB=	clangAnalysis
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>

SRCS=	AnalysisDeclContext.cpp \
	BodyFarm.cpp \
	CalledOnceCheck.cpp \
	CFG.cpp \
	CFGReachabilityAnalysis.cpp \
	CFGStmtMap.cpp \
	CallGraph.cpp \
	CloneDetection.cpp \
	CocoaConventions.cpp \
	ConstructionContext.cpp \
	Consumed.cpp \
	CodeInjector.cpp \
	Dominators.cpp \
	ExprMutationAnalyzer.cpp \
	IssueHash.cpp \
	LiveVariables.cpp \
	MacroExpansionContext.cpp \
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
	UninitializedValues.cpp \
	UnsafeBufferUsage.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Analysis

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
