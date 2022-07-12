# $OpenBSD: Makefile,v 1.12 2022/07/12 21:06:04 jca Exp $

.include <bsd.own.mk>

PROG=	lldb
BINDIR=	/usr/bin
LIBEXECDIR=/usr/libexec

SRCS=	Driver.cpp \
	Platform.cpp \
	lldb.cpp

LDADD+=		-lcurses -ledit -lpanel

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}

.PATH:	${.CURDIR}/../../../llvm/lldb/source
.PATH:	${.CURDIR}/../../../llvm/lldb/tools/driver
.PATH:	${.CURDIR}/../../../llvm/lldb/docs

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

Driver.o: Options.inc

Options.inc: Options.td
	${.OBJDIR}/../llvm-tblgen/llvm-tblgen -gen-opt-parser-defs \
		-I ${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

BUILDFIRST=	VCSVersion.inc

VCSVersion.inc:
	touch $@

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
