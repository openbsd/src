# $OpenBSD: Makefile,v 1.10 2026/05/29 11:18:31 robert Exp $

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
	DependencyFile.cpp \
	DependencyGraph.cpp \
	DiagnosticRenderer.cpp \
	FrontendAction.cpp \
	FrontendActions.cpp \
	FrontendOptions.cpp \
	HeaderIncludeGen.cpp \
	InitPreprocessor.cpp \
	LayoutOverrideSource.cpp \
	LogDiagnosticPrinter.cpp \
	ModuleDependencyCollector.cpp \
	MultiplexConsumer.cpp \
	PrecompiledPreamble.cpp \
	PrintPreprocessedOutput.cpp \
	SARIFDiagnostic.cpp \
	SARIFDiagnosticPrinter.cpp \
	SerializedDiagnosticPrinter.cpp \
	SerializedDiagnosticReader.cpp \
	StandaloneDiagnostic.cpp \
	TestModuleFileExtension.cpp \
	TextDiagnostic.cpp \
	TextDiagnosticBuffer.cpp \
	TextDiagnosticPrinter.cpp \
	VerifyDiagnosticConsumer.cpp \
	InterfaceStubFunctionsConsumer.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Frontend

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
