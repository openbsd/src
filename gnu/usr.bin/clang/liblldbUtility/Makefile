# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:31 patrick Exp $

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
	GDBRemote.cpp \
	IOObject.cpp \
	LLDBAssert.cpp \
	Listener.cpp \
	Log.cpp \
	Logging.cpp \
	NameMatches.cpp \
	ProcessInfo.cpp \
	RegisterValue.cpp \
	RegularExpression.cpp \
	Reproducer.cpp \
	ReproducerInstrumentation.cpp \
	Scalar.cpp \
	SelectHelper.cpp \
	SharingPtr.cpp \
	State.cpp \
	Status.cpp \
	Stream.cpp \
	StreamCallback.cpp \
	StreamString.cpp \
	StringExtractor.cpp \
	StringExtractorGDBRemote.cpp \
	StringLexer.cpp \
	StringList.cpp \
	StructuredData.cpp \
	TildeExpressionResolver.cpp \
	Timer.cpp \
	UUID.cpp \
	UriParser.cpp \
	UserID.cpp \
	UserIDResolver.cpp \
	VASprintf.cpp \
	VMRange.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Utility

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
