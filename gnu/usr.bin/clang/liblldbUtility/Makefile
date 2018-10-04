# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbUtility
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	ArchSpec.cpp \
	Baton.cpp \
	Connection.cpp \
	ConstString.cpp \
	DataBufferHeap.cpp \
	DataBufferLLVM.cpp \
	DataEncoder.cpp \
	DataExtractor.cpp \
	FastDemangle.cpp \
	FileSpec.cpp \
	History.cpp \
	IOObject.cpp \
	JSON.cpp \
	LLDBAssert.cpp \
	Log.cpp \
	Logging.cpp \
	NameMatches.cpp \
	Range.cpp \
	RegularExpression.cpp \
	SelectHelper.cpp \
	SharingPtr.cpp \
	Status.cpp \
	Stream.cpp \
	StreamCallback.cpp \
	StreamGDBRemote.cpp \
	StreamString.cpp \
	StringExtractor.cpp \
	StringExtractorGDBRemote.cpp \
	StringLexer.cpp \
	StringList.cpp \
	StructuredData.cpp \
	TildeExpressionResolver.cpp \
	Timer.cpp \
	UserID.cpp \
	UriParser.cpp \
	UUID.cpp \
	VASprintf.cpp \
	VMRange.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Utility

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
