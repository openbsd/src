# $OpenBSD: Makefile,v 1.7 2022/07/12 21:06:04 jca Exp $

.include <bsd.own.mk>

PROG=	lldb-server
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

BUILDFIRST=	VCSVersion.inc LLGSOptions.inc

VCSVersion.inc:
	touch $@

LLGSOptions.inc: ${.CURDIR}/../../../llvm/lldb/tools/lldb-server/LLGSOptions.td
	${.OBJDIR}/../llvm-tblgen/llvm-tblgen -gen-opt-parser-defs \
		-I ${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>

CPPFLAGS+=	${LLDB_INCLUDES} \
		${CLANG_INCLUDES}
