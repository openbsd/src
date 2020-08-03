# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:30 patrick Exp $

LIB=	lldbPluginLanguage
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	BlockPointer.cpp \
	CF.cpp \
	CPlusPlusLanguage.cpp \
	CPlusPlusNameParser.cpp \
	ClangHighlighter.cpp \
	Cocoa.cpp \
	CoreMedia.cpp \
	CxxStringTypes.cpp \
	LibCxx.cpp \
	LibCxxAtomic.cpp \
	LibCxxBitset.cpp \
	LibCxxInitializerList.cpp \
	LibCxxList.cpp \
	LibCxxMap.cpp \
	LibCxxOptional.cpp \
	LibCxxQueue.cpp \
	LibCxxTuple.cpp \
	LibCxxUnorderedMap.cpp \
	LibCxxVariant.cpp \
	LibCxxVector.cpp \
	LibStdcpp.cpp \
	LibStdcpp.h \
	LibStdcppTuple.cpp \
	LibStdcppUniquePointer.cpp \
	MSVCUndecoratedNameParser.cpp \
	NSArray.cpp \
	NSDictionary.cpp \
	NSError.cpp \
	NSException.cpp \
	NSIndexPath.cpp \
	NSSet.cpp \
	NSString.cpp \
	ObjCLanguage.cpp \
	ObjCPlusPlusLanguage.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Language/CPlusPlus
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Language/ClangCommon
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Language/ObjC
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Language/ObjCPlusPlus

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
