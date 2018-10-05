# $OpenBSD: Makefile,v 1.2 2018/10/05 03:47:59 deraadt Exp $

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

LLVM_LIBDEPS=	${BACKEND_ASMPARSER} \
		${BACKEND_ASMPRINTER} \
		${BACKEND_CODEGEN} \
		${BACKEND_DESC} \
		${BACKEND_DISASSEMBLER} \
		${BACKEND_INFO} \
		${BACKEND_UTILS} \
		LLVMAnalysis \
		LLVMAsmParser \
		LLVMAsmPrinter \
		LLVMBinaryFormat \
		LLVMBitReader \
		LLVMBitWriter \
		LLVMCodeGen \
		LLVMCore \
		LLVMCoroutines \
		LLVMCoverage \
		LLVMDebugInfoCodeView \
		LLVMDebugInfoDWARF \
		LLVMDebugInfoMSF \
		LLVMDebugInfoPDB \
		LLVMDemangle \
		LLVMExecutionEngine \
		LLVMGlobalISel \
		LLVMIRReader \
		LLVMInstCombine \
		LLVMInstrumentation \
		LLVMLTO \
		LLVMLinker \
		LLVMMC \
		LLVMMCDisassembler \
		LLVMMCParser \
		LLVMObjCARCOpts \
		LLVMObject \
		LLVMOption \
		LLVMPasses \
		LLVMProfileData \
		LLVMScalarOpts \
		LLVMSelectionDAG \
		LLVMSupport \
		LLVMTarget \
		LLVMTransformUtils \
		LLVMVectorize \
		LLVMipo \
		clangAST \
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
		lldbUtility \

.include <bsd.prog.mk>
