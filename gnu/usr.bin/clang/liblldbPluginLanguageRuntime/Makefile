# $OpenBSD: Makefile,v 1.4 2021/04/28 12:55:41 patrick Exp $

LIB=	lldbPluginLanguageRuntime
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

# CPlusPlus
SRCS+= CPPLanguageRuntime.cpp \
	ItaniumABILanguageRuntime.cpp

# ObjC
SRCS+=	ObjCLanguageRuntime.cpp \
	AppleObjCRuntime.cpp \
	AppleObjCRuntimeV1.cpp \
	AppleObjCRuntimeV2.cpp \
	AppleObjCTrampolineHandler.cpp \
	AppleObjCDeclVendor.cpp \
	AppleThreadPlanStepThroughObjCTrampoline.cpp \
	AppleObjCClassDescriptorV2.cpp \
	AppleObjCTypeEncodingParser.cpp

# RenderScript
SRCS+=	RenderScriptRuntime.cpp \
	RenderScriptExpressionOpts.cpp \
	RenderScriptx86ABIFixups.cpp \
	RenderScriptScriptGroup.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/LanguageRuntime/CPlusPlus
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/LanguageRuntime/CPlusPlus/ItaniumABI
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/LanguageRuntime/ObjC
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/LanguageRuntime/ObjC/AppleObjCRuntime
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/LanguageRuntime/RenderScript/RenderScriptRuntime

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
