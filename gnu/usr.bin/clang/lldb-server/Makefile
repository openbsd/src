# $OpenBSD: Makefile,v 1.1 2019/11/09 16:45:48 mortimer Exp $

.include <bsd.own.mk>

NOPROG=	lldb-server
BINDIR=	/usr/bin
NOMAN=

SRCS=	Acceptor.cpp \
	LLDBServerUtilities.cpp \
	SystemInitializerLLGS.cpp \
	lldb-platform.cpp \
	lldb-gdbserver.cpp \
	lldb-server.cpp \
	lldb.cpp \

LDADD+=	-lcurses -ledit -lpanel


.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/tools/lldb-server

LLVM_LIBDEPS=	clangAST \
    clangAnalysis \
    clangBasic \
    clangBasicTargets \
    clangCodeGen \
    clangDriver \
    clangDriverArch \
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
    lldbPluginOperatingSystem \
    lldbPluginPlatform \
    lldbPluginProcess \
    lldbPluginScriptInterpreter \
    lldbPluginStructuredData \
    lldbPluginSymbolFile \
    lldbPluginSymbolVendor \
    lldbPluginSystemRuntime \
    lldbPluginUnwindAssembly \
    lldbSymbol \
    lldbTarget \
    lldbUtility


LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>

CPPFLAGS+=	${LLDB_INCLUDES} \
		${CLANG_INCLUDES}
