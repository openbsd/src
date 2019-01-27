# $OpenBSD: Makefile,v 1.2 2019/01/27 16:59:42 patrick Exp $

LIB=	lldbUtility
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	ArchSpec.cpp \
	Args.cpp \
	Baton.cpp \
	CompletionRequest.cpp \
	Connection.cpp \
	ConstString.cpp \
	DataBufferHeap.cpp \
	DataBufferLLVM.cpp \
	DataEncoder.cpp \
	DataExtractor.cpp \
	Environment.cpp \
	FastDemangle.cpp \
	FileSpec.cpp \
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
