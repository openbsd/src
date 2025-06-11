# $OpenBSD: Makefile,v 1.7 2025/06/11 17:46:33 robert Exp $

LIB=	clangSema
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/clang/Sema \
		${CLANG_INCLUDES}

.include <bsd.own.mk>

SRCS=	AnalysisBasedWarnings.cpp \
	CheckExprLifetime.cpp \
	CodeCompleteConsumer.cpp \
	DeclSpec.cpp \
	DelayedDiagnostic.cpp \
	HLSLExternalSemaSource.cpp \
	IdentifierResolver.cpp \
	JumpDiagnostics.cpp \
	MultiplexExternalSemaSource.cpp \
	ParsedAttr.cpp \
	Scope.cpp \
	ScopeInfo.cpp \
	Sema.cpp \
	SemaAMDGPU.cpp \
	SemaARM.cpp \
	SemaAVR.cpp \
	SemaAccess.cpp \
	SemaAttr.cpp \
	SemaAPINotes.cpp \
	SemaAvailability.cpp \
	SemaBPF.cpp \
	SemaBase.cpp \
	SemaBoundsSafety.cpp \
	SemaCXXScopeSpec.cpp \
	SemaCast.cpp \
	SemaChecking.cpp \
	SemaCodeComplete.cpp \
	SemaConcept.cpp \
	SemaConsumer.cpp \
	SemaCoroutine.cpp \
	SemaCUDA.cpp \
	SemaDecl.cpp \
	SemaDeclAttr.cpp \
	SemaDeclCXX.cpp \
	SemaDeclObjC.cpp \
	SemaExceptionSpec.cpp \
	SemaExpr.cpp \
	SemaExprCXX.cpp \
	SemaExprMember.cpp \
	SemaExprObjC.cpp \
	SemaFixItUtils.cpp \
	SemaHLSL.cpp \
	SemaHexagon.cpp \
	SemaInit.cpp \
	SemaLambda.cpp \
	SemaLookup.cpp \
	SemaLoongArch.cpp \
	SemaM68k.cpp \
	SemaMIPS.cpp \
	SemaMSP430.cpp \
	SemaModule.cpp \
	SemaNVPTX.cpp \
	SemaObjC.cpp \
	SemaObjCProperty.cpp \
	SemaOpenACC.cpp \
	SemaOpenCL.cpp \
	SemaOpenMP.cpp \
	SemaOverload.cpp \
	SemaPPC.cpp \
	SemaPseudoObject.cpp \
	SemaRISCV.cpp \
	SemaStmt.cpp \
	SemaStmtAsm.cpp \
	SemaStmtAttr.cpp \
	SemaSYCL.cpp \
	SemaSwift.cpp \
	SemaSystemZ.cpp \
	SemaTemplate.cpp \
	SemaTemplateDeduction.cpp \
	SemaTemplateDeductionGuide.cpp \
	SemaTemplateInstantiate.cpp \
	SemaTemplateInstantiateDecl.cpp \
	SemaTemplateVariadic.cpp \
	SemaType.cpp \
	SemaWasm.cpp \
	SemaX86.cpp \
	TypeLocBuilder.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Sema

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
