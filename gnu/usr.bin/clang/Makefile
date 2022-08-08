# $OpenBSD: Makefile,v 1.26 2022/08/08 09:25:13 fcambus Exp $

.include <bsd.own.mk>

.include "Makefile.arch"

# order!
SUBDIR=	include/llvm/Config
SUBDIR+=libLLVMSupport
SUBDIR+=libLLVMTableGen
SUBDIR+=llvm-tblgen
SUBDIR+=include/llvm/IR
SUBDIR+=include/llvm/Frontend/OpenACC
SUBDIR+=include/llvm/Frontend/OpenMP

.for arch in ${LLVM_ARCHS}
.if "${arch}" == ${LLVM_ARCH} || "${arch}" == "AMDGPU" || make(obj)
SUBDIR+=include/llvm/${arch}
.endif
.endfor

SUBDIR+=libLLVM
SUBDIR+=llvm-config

SUBDIR+=clang-tblgen
SUBDIR+=include/clang

SUBDIR+=libclangBasic
SUBDIR+=libclangLex
SUBDIR+=libclangParse
SUBDIR+=libclangAST
SUBDIR+=libclangSema
SUBDIR+=libclangCodeGen
SUBDIR+=libclangAnalysis
SUBDIR+=libclangEdit
SUBDIR+=libclangRewrite
SUBDIR+=libclangDriver
SUBDIR+=libclangSerialization
SUBDIR+=libclangFrontend
SUBDIR+=libclangRewriteFrontend
SUBDIR+=libclangFrontendTool

SUBDIR+=clang

SUBDIR+=include/lld/ELF

SUBDIR+=liblldCommon
SUBDIR+=liblldCore
SUBDIR+=liblldELF

SUBDIR+=lld


.if ${BUILD_LLDB:L} == "yes" || make(obj)
SUBDIR+=lldb-tblgen
SUBDIR+=include/lldb/Commands
SUBDIR+=include/lldb/Core
SUBDIR+=include/lldb/Interpreter
SUBDIR+=include/lldb/Plugins
SUBDIR+=include/lldb/Target

SUBDIR+=liblldbABI
SUBDIR+=liblldbAPI
SUBDIR+=liblldbBreakpoint
SUBDIR+=liblldbCommands
SUBDIR+=liblldbCore
SUBDIR+=liblldbDataFormatters
SUBDIR+=liblldbExpression
SUBDIR+=liblldbHostCommon
SUBDIR+=liblldbHostOpenBSD
SUBDIR+=liblldbHostPOSIX
SUBDIR+=liblldbInitialization
SUBDIR+=liblldbInterpreter
SUBDIR+=liblldbPluginArchitecture
SUBDIR+=liblldbPluginDisassembler
SUBDIR+=liblldbPluginDynamicLoader
SUBDIR+=liblldbPluginExpressionParser
SUBDIR+=liblldbPluginInstruction
SUBDIR+=liblldbPluginInstrumentationRuntime
SUBDIR+=liblldbPluginJITLoader
SUBDIR+=liblldbPluginLanguage
SUBDIR+=liblldbPluginLanguageRuntime
SUBDIR+=liblldbPluginMemoryHistory
SUBDIR+=liblldbPluginObjectContainer
SUBDIR+=liblldbPluginObjectFile
SUBDIR+=liblldbPluginPlatform
SUBDIR+=liblldbPluginProcess
SUBDIR+=liblldbPluginScriptInterpreter
SUBDIR+=liblldbPluginStructuredData
SUBDIR+=liblldbPluginSymbolFile
SUBDIR+=liblldbPluginSymbolVendor
SUBDIR+=liblldbPluginSystemRuntime
SUBDIR+=liblldbPluginTypeSystem
SUBDIR+=liblldbPluginUnwindAssembly
SUBDIR+=liblldbSymbol
SUBDIR+=liblldbTarget
SUBDIR+=liblldbUtility

SUBDIR+=lldb
SUBDIR+=lldb-server
.endif

SUBDIR+=include/llvm-objcopy
SUBDIR+=llvm-objcopy
SUBDIR+=include/llvm-objdump
SUBDIR+=llvm-objdump
SUBDIR+=include/llvm-readobj
SUBDIR+=llvm-readobj

SUBDIR+=llvm-profdata
SUBDIR+=llvm-cov

.if ${AR_VERSION:L} == "llvm" || make(obj)
SUBDIR+=libLLVMDlltoolDriver
SUBDIR+=libLLVMLibDriver
SUBDIR+=llvm-ar
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
