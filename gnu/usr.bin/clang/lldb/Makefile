# $OpenBSD: Makefile,v 1.5 2019/03/05 09:44:42 jsg Exp $

.include <bsd.own.mk>

NOPROG=	lldb
BINDIR=	/usr/bin
LIBEXECDIR=/usr/libexec
SRCS=	Driver.cpp \
	Platform.cpp \
	lldb.cpp

LDADD+=		-lcurses -ledit -lpanel

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/tools/driver
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/docs

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
