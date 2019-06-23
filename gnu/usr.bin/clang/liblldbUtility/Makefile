# $OpenBSD: Makefile,v 1.3 2019/06/23 22:07:45 patrick Exp $

LIB=	lldbUtility
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	ArchSpec.cpp \
	Args.cpp \
	Baton.cpp \
	Broadcaster.cpp \
	CompletionRequest.cpp \
	Connection.cpp \
	ConstString.cpp \
	DataBufferHeap.cpp \
	DataBufferLLVM.cpp \
	DataEncoder.cpp \
	DataExtractor.cpp \
	Environment.cpp \
	Event.cpp \
	FileSpec.cpp \
	IOObject.cpp \
	JSON.cpp \
	LLDBAssert.cpp \
	Listener.cpp \
	Log.cpp \
	Logging.cpp \
	NameMatches.cpp \
	RegisterValue.cpp \
	RegularExpression.cpp \
	Reproducer.cpp \
	Scalar.cpp \
	SelectHelper.cpp \
	SharingPtr.cpp \
	State.cpp \
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
