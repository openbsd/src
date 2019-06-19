# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbPluginLanguage
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	BlockPointer.cpp \
	CF.cpp \
	CPlusPlusLanguage.cpp \
	CPlusPlusNameParser.cpp \
	Cocoa.cpp \
	CoreMedia.cpp \
	CxxStringTypes.cpp \
	GoFormatterFunctions.cpp \
	GoLanguage.cpp \
	JavaFormatterFunctions.cpp \
	JavaLanguage.cpp \
	LibCxx.cpp \
	LibCxxAtomic.cpp \
	LibCxxBitset.cpp \
	LibCxxInitializerList.cpp \
	LibCxxList.cpp \
	LibCxxMap.cpp \
	LibCxxQueue.cpp \
	LibCxxTuple.cpp \
	LibCxxUnorderedMap.cpp \
	LibCxxVector.cpp \
	LibStdcpp.cpp \
	LibStdcpp.h \
	LibStdcppTuple.cpp \
	LibStdcppUniquePointer.cpp \
	NSArray.cpp \
	NSDictionary.cpp \
	NSError.cpp \
	NSException.cpp \
	NSIndexPath.cpp \
	NSSet.cpp \
	NSString.cpp \
	OCamlLanguage.cpp \
	ObjCLanguage.cpp \
	ObjCPlusPlusLanguage.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Language/CPlusPlus
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Language/Go
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Language/Java
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Language/ObjC
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Language/ObjCPlusPlus
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Language/OCaml

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
