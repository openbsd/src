# $OpenBSD: Makefile,v 1.5 2019/06/23 22:07:42 patrick Exp $

LIB=	clangFrontend
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	ASTConsumers.cpp \
	ASTMerge.cpp \
	ASTUnit.cpp \
	ChainedDiagnosticConsumer.cpp \
	ChainedIncludesSource.cpp \
	CompilerInstance.cpp \
	CompilerInvocation.cpp \
	CreateInvocationFromCommandLine.cpp \
	DependencyFile.cpp \
	DependencyGraph.cpp \
	DiagnosticRenderer.cpp \
	FrontendAction.cpp \
	FrontendActions.cpp \
	FrontendOptions.cpp \
	FrontendTiming.cpp \
	HeaderIncludeGen.cpp \
	InitHeaderSearch.cpp \
	InitPreprocessor.cpp \
	LangStandards.cpp \
	LayoutOverrideSource.cpp \
	LogDiagnosticPrinter.cpp \
	ModuleDependencyCollector.cpp \
	MultiplexConsumer.cpp \
	PrecompiledPreamble.cpp \
	PrintPreprocessedOutput.cpp \
	SerializedDiagnosticPrinter.cpp \
	SerializedDiagnosticReader.cpp \
	TestModuleFileExtension.cpp \
	TextDiagnostic.cpp \
	TextDiagnosticBuffer.cpp \
	TextDiagnosticPrinter.cpp \
	VerifyDiagnosticConsumer.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Frontend

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
