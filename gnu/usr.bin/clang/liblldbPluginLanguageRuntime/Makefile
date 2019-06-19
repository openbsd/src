# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbPluginLanguageRuntime
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	AppleObjCClassDescriptorV2.cpp \
	AppleObjCDeclVendor.cpp \
	AppleObjCRuntime.cpp \
	AppleObjCRuntimeV1.cpp \
	AppleObjCRuntimeV2.cpp \
	AppleObjCTrampolineHandler.cpp \
	AppleObjCTypeEncodingParser.cpp \
	AppleThreadPlanStepThroughObjCTrampoline.cpp \
	GoLanguageRuntime.cpp \
	ItaniumABILanguageRuntime.cpp \
	JavaLanguageRuntime.cpp \
	RenderScriptExpressionOpts.cpp \
	RenderScriptRuntime.cpp \
	RenderScriptScriptGroup.cpp \
	RenderScriptx86ABIFixups.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/LanguageRuntime/CPlusPlus/ItaniumABI
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/LanguageRuntime/Go
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/LanguageRuntime/Java
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/LanguageRuntime/ObjC/AppleObjCRuntime
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/LanguageRuntime/RenderScript/RenderScriptRuntime

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
