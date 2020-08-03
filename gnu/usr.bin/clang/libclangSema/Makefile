# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:29 patrick Exp $

LIB=	clangSema
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/clang/Sema \
		${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	AnalysisBasedWarnings.cpp \
	CodeCompleteConsumer.cpp \
	DeclSpec.cpp \
	DelayedDiagnostic.cpp \
	IdentifierResolver.cpp \
	JumpDiagnostics.cpp \
	MultiplexExternalSemaSource.cpp \
	ParsedAttr.cpp \
	Scope.cpp \
	ScopeInfo.cpp \
	Sema.cpp \
	SemaAccess.cpp \
	SemaAttr.cpp \
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
	SemaInit.cpp \
	SemaLambda.cpp \
	SemaLookup.cpp \
	SemaModule.cpp \
	SemaObjCProperty.cpp \
	SemaOpenMP.cpp \
	SemaOverload.cpp \
	SemaPseudoObject.cpp \
	SemaStmt.cpp \
	SemaStmtAsm.cpp \
	SemaStmtAttr.cpp \
	SemaTemplate.cpp \
	SemaTemplateDeduction.cpp \
	SemaTemplateInstantiate.cpp \
	SemaTemplateInstantiateDecl.cpp \
	SemaTemplateVariadic.cpp \
	SemaType.cpp \
	TypeLocBuilder.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Sema

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
