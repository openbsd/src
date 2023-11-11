# $OpenBSD: Makefile,v 1.5 2023/11/11 18:35:39 robert Exp $

LIB=	lldbPluginLanguage
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

# CPlusPlus
SRCS=	BlockPointer.cpp \
	Coroutines.cpp \
	CPlusPlusLanguage.cpp \
	CPlusPlusNameParser.cpp \
	CxxStringTypes.cpp \
	GenericBitset.cpp \
	GenericOptional.cpp \
	LibCxx.cpp \
	LibCxxAtomic.cpp \
	LibCxxInitializerList.cpp \
	LibCxxList.cpp \
	LibCxxMap.cpp \
	LibCxxQueue.cpp \
	LibCxxRangesRefView.cpp \
	LibCxxSpan.cpp \
	LibCxxTuple.cpp \
	LibCxxUnorderedMap.cpp \
	LibCxxVariant.cpp \
	LibCxxVector.cpp \
	LibStdcpp.cpp \
	LibStdcppTuple.cpp \
	LibStdcppUniquePointer.cpp \
	MSVCUndecoratedNameParser.cpp

# ClangCommon
SRCS+=	ClangHighlighter.cpp

# ObjC
SRCS+=	ObjCLanguage.cpp \
	CF.cpp \
	CFBasicHash.cpp \
	Cocoa.cpp \
	CoreMedia.cpp \
	NSArray.cpp \
	NSDictionary.cpp \
	NSError.cpp \
	NSException.cpp \
	NSIndexPath.cpp \
	NSSet.cpp \
	NSString.cpp

# ObjCPlusPlus
SRCS+=	ObjCPlusPlusLanguage.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Language/ClangCommon
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Language/CPlusPlus
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Language/ObjC
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Language/ObjCPlusPlus

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
