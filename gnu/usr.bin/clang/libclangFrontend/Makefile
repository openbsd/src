# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

LIB=	clangFrontend
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	ASTConsumers.cpp \
	ASTMerge.cpp \
	ASTUnit.cpp \
	CacheTokens.cpp \
	ChainedDiagnosticConsumer.cpp \
	ChainedIncludesSource.cpp \
	CodeGenOptions.cpp \
	CompilerInstance.cpp \
	CompilerInvocation.cpp \
	CreateInvocationFromCommandLine.cpp \
	DependencyFile.cpp \
	DependencyGraph.cpp \
	DiagnosticRenderer.cpp \
	FrontendAction.cpp \
	FrontendActions.cpp \
	FrontendOptions.cpp \
	HeaderIncludeGen.cpp \
	InitHeaderSearch.cpp \
	InitPreprocessor.cpp \
	LangStandards.cpp \
	LayoutOverrideSource.cpp \
	LogDiagnosticPrinter.cpp \
	ModuleDependencyCollector.cpp \
	MultiplexConsumer.cpp \
	PCHContainerOperations.cpp \
	PrintPreprocessedOutput.cpp \
	SerializedDiagnosticPrinter.cpp \
	SerializedDiagnosticReader.cpp \
	TestModuleFileExtension.cpp \
	TextDiagnostic.cpp \
	TextDiagnosticBuffer.cpp \
	TextDiagnosticPrinter.cpp \
	VerifyDiagnosticConsumer.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Frontend

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
