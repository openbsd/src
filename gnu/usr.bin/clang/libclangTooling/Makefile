# $OpenBSD: Makefile,v 1.1 2026/06/23 10:50:23 jca Exp $

LIB=	clangTooling
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${CLANG_SRCS}/lib/Tooling

.include <bsd.own.mk>

SRCS=	AllTUsExecution.cpp \
	ArgumentsAdjusters.cpp \
	CommonOptionsParser.cpp \
	CompilationDatabase.cpp \
	DependencyScanningTool.cpp \
	Execution.cpp \
	ExpandResponseFilesCompilationDatabase.cpp \
	FileMatchTrie.cpp \
	FixIt.cpp \
	GuessTargetAndModeCompilationDatabase.cpp \
	InterpolatingCompilationDatabase.cpp \
	JSONCompilationDatabase.cpp \
	LocateToolCompilationDatabase.cpp \
	Refactoring.cpp \
	RefactoringCallbacks.cpp \
	StandaloneExecution.cpp \
	Tooling.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Tooling

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
