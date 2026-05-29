# $OpenBSD: Makefile,v 1.13 2026/05/29 11:18:32 robert Exp $

.include <bsd.own.mk>

PROG=	lldb-server
BINDIR=	/usr/bin
NOMAN=

SRCS=	lldb-gdbserver.cpp \
	lldb-platform.cpp \
	lldb-server.cpp \
	LLDBServerUtilities.cpp \
	SystemInitializerLLGS.cpp \
	Version.cpp

LDADD+=	-lcurses -ledit -lpanel


.PATH:	${.CURDIR}/../../../llvm/lldb/source
.PATH: ${.CURDIR}/../../../llvm/lldb/source/Version
.PATH:	${.CURDIR}/../../../llvm/lldb/tools/lldb-server

LLVM_LIBDEPS=	clangAPINotes \
    clangAST \
    clangASTMatchers \
    clangAnalysis \
    clangAnalysisLifetimeSafety \
    clangBasic \
    clangCodeGen \
    clangDriver \
    clangEdit \
    clangFrontend \
    clangLex \
    clangOptions \
    clangParse \
    clangRewrite \
    clangRewriteFrontend \
    clangSema \
    clangSerialization \
    clangSupport \
    lldbABI \
    lldbAPI \
    lldbBreakpoint \
    lldbCommands \
    lldbCore \
    lldbDataFormatters \
    lldbExpression \
    lldbHostCommon \
    lldbHostOpenBSD \
    lldbHostPOSIX \
    lldbInitialization \
    lldbInterpreter \
    lldbPluginArchitecture \
    lldbPluginDisassembler \
    lldbPluginDynamicLoader \
    lldbPluginExpressionParser \
    lldbPluginInstruction \
    lldbPluginInstrumentationRuntime \
    lldbPluginJITLoader \
    lldbPluginLanguage \
    lldbPluginLanguageRuntime \
    lldbPluginMemoryHistory \
    lldbPluginObjectContainer \
    lldbPluginObjectFile \
    lldbPluginPlatform \
    lldbPluginProcess \
    lldbPluginScriptInterpreter \
    lldbPluginStructuredData \
    lldbPluginSymbolFile \
    lldbPluginSymbolVendor \
    lldbPluginSystemRuntime \
    lldbPluginTypeSystem \
    lldbPluginUnwindAssembly \
    lldbSymbol \
    lldbTarget \
    lldbUtility \
    lldbValueObject

BUILDFIRST=	VCSVersion.inc LLGSOptions.inc PlatformOptions.inc
CLEANFILES+=	VCSVersion.inc LLGSOptions.inc PlatformOptions.inc

VCSVersion.inc:
	touch $@

LLGSOptions.inc: ${.CURDIR}/../../../llvm/lldb/tools/lldb-server/LLGSOptions.td
	${.OBJDIR}/../llvm-tblgen/llvm-tblgen -gen-opt-parser-defs \
		-I ${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

PlatformOptions.inc: ${.CURDIR}/../../../llvm/lldb/tools/lldb-server/PlatformOptions.td
	${.OBJDIR}/../llvm-tblgen/llvm-tblgen -gen-opt-parser-defs \
		-I ${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>

CPPFLAGS+=	${LLDB_INCLUDES} \
		${CLANG_INCLUDES}
