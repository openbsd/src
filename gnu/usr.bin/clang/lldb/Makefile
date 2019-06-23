# $OpenBSD: Makefile,v 1.6 2019/06/23 22:07:45 patrick Exp $

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

Driver.o: Options.inc

Options.inc: Options.td
	${.OBJDIR}/../llvm-tblgen/llvm-tblgen -gen-opt-parser-defs \
		-I ${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
