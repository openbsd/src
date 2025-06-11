# $OpenBSD: Makefile,v 1.18 2025/06/11 17:46:33 robert Exp $

.include <bsd.own.mk>

PROG=	lldb
BINDIR=	/usr/bin
LIBEXECDIR=/usr/libexec

SRCS=	Driver.cpp \
	Platform.cpp \
	Version.cpp

LDADD+=		-lcurses -ledit -lpanel -lkvm

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include

.PATH:	${.CURDIR}/../../../llvm/lldb/source
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Version
.PATH:	${.CURDIR}/../../../llvm/lldb/tools/driver
.PATH:	${.CURDIR}/../../../llvm/lldb/docs

LLVM_LIBDEPS=	clangAST \
		clangASTMatchers \
		clangAPINotes \
		clangAnalysis \
		clangBasic \
		clangBasicTargets \
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
		lldbPluginRegisterTypeBuilder \
		lldbPluginScriptInterpreter \
		lldbPluginStructuredData \
		lldbPluginSymbolFile \
		lldbPluginSymbolLocator \
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
CLEANFILES+=	Options.inc VCSVersion.inc

VCSVersion.inc:
	touch $@

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
