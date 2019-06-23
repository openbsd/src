# $OpenBSD: Makefile,v 1.2 2019/06/23 22:07:43 patrick Exp $

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
	ItaniumABILanguageRuntime.cpp \
	RenderScriptExpressionOpts.cpp \
	RenderScriptRuntime.cpp \
	RenderScriptScriptGroup.cpp \
	RenderScriptx86ABIFixups.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/LanguageRuntime/CPlusPlus/ItaniumABI
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/LanguageRuntime/ObjC/AppleObjCRuntime
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/LanguageRuntime/RenderScript/RenderScriptRuntime

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
