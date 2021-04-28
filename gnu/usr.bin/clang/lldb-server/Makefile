# $OpenBSD: Makefile,v 1.5 2021/04/28 12:55:42 patrick Exp $

.include <bsd.own.mk>

.if (${MACHINE} == "arm64") || (${MACHINE} == "amd64")
PROG=	lldb-server
.else
NOPROG=	lldb-server
.endif

BINDIR=	/usr/bin
NOMAN=

SRCS=	Acceptor.cpp \
	lldb.cpp \
	lldb-gdbserver.cpp \
	lldb-platform.cpp \
	lldb-server.cpp \
	LLDBServerUtilities.cpp \
	SystemInitializerLLGS.cpp

LDADD+=	-lcurses -ledit -lpanel


.PATH:	${.CURDIR}/../../../llvm/lldb/source
.PATH:	${.CURDIR}/../../../llvm/lldb/tools/lldb-server

LLVM_LIBDEPS=	clangAST \
    clangAnalysis \
    clangBasic \
    clangCodeGen \
    clangDriver \
    clangEdit \
    clangFrontend \
    clangLex \
    clangParse \
    clangRewrite \
    clangRewriteFrontend \
    clangSema \
    clangSerialization \
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
    lldbUtility


LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>

CPPFLAGS+=	${LLDB_INCLUDES} \
		${CLANG_INCLUDES}
