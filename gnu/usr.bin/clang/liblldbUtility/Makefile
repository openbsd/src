# $OpenBSD: Makefile,v 1.7 2023/11/11 18:35:40 robert Exp $

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
	Diagnostics.cpp \
	Environment.cpp \
	Event.cpp \
	FileSpec.cpp \
	GDBRemote.cpp \
	IOObject.cpp \
	LLDBAssert.cpp \
	LLDBLog.cpp \
	Listener.cpp \
	Log.cpp \
	NameMatches.cpp \
	ProcessInfo.cpp \
	RegisterValue.cpp \
	RegularExpression.cpp \
	Instrumentation.cpp \
	Scalar.cpp \
	SelectHelper.cpp \
	State.cpp \
	Status.cpp \
	Stream.cpp \
	StreamString.cpp \
	StringExtractor.cpp \
	StringExtractorGDBRemote.cpp \
	StringLexer.cpp \
	StringList.cpp \
	StructuredData.cpp \
	TildeExpressionResolver.cpp \
	Timer.cpp \
	TraceGDBRemotePackets.cpp \
	TraceIntelPTGDBRemotePackets.cpp \
	UnimplementedError.cpp \
	UUID.cpp \
	UriParser.cpp \
	UserID.cpp \
	UserIDResolver.cpp \
	VASprintf.cpp \
	VMRange.cpp \
	XcodeSDK.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Utility

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
