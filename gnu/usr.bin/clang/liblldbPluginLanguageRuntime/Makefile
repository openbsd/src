# $OpenBSD: Makefile,v 1.5 2025/06/11 17:46:33 robert Exp $

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

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/LanguageRuntime/CPlusPlus
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/LanguageRuntime/CPlusPlus/ItaniumABI
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/LanguageRuntime/ObjC
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/LanguageRuntime/ObjC/AppleObjCRuntime

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
