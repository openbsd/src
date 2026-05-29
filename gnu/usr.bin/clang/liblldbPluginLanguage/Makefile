# $OpenBSD: Makefile,v 1.7 2026/05/29 11:18:31 robert Exp $

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
	Generic.cpp \
	GenericBitset.cpp \
	GenericInitializerList.cpp \
	GenericList.cpp \
	GenericOptional.cpp \
	LibCxx.cpp \
	LibCxxAtomic.cpp \
	LibCxxMap.cpp \
	LibCxxQueue.cpp \
	LibCxxRangesRefView.cpp \
	LibCxxSliceArray.cpp \
	LibCxxProxyArray.cpp \
	LibCxxSpan.cpp \
	LibCxxTuple.cpp \
	LibCxxUnorderedMap.cpp \
	LibCxxVariant.cpp \
	LibCxxValarray.cpp \
	LibCxxVector.cpp \
	LibStdcpp.cpp \
	LibStdcppSpan.cpp \
	LibStdcppTuple.cpp \
	LibStdcppUniquePointer.cpp \
	MsvcStl.cpp \
	MsvcStlAtomic.cpp \
	MsvcStlDeque.cpp \
	MsvcStlSmartPointer.cpp \
	MsvcStlSpan.cpp \
	MsvcStlTree.cpp \
	MsvcStlTuple.cpp \
	MsvcStlUnordered.cpp \
	MsvcStlVariant.cpp \
	MsvcStlVector.cpp \
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
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Plugins

