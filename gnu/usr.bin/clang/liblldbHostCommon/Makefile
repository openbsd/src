# $OpenBSD: Makefile,v 1.7 2026/05/29 11:18:31 robert Exp $

LIB=	lldbHostCommon
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	DiagnosticsRendering.cpp \
	Editline.cpp \
	FileAction.cpp \
	FileCache.cpp \
	File.cpp \
	FileSystem.cpp \
	GetOptInc.cpp \
	Host.cpp \
	HostInfoBase.cpp \
	HostNativeThreadBase.cpp \
	HostProcess.cpp \
	HostThread.cpp \
	JSONTransport.cpp \
	LZMA.cpp \
	LockFileBase.cpp \
	MainLoopBase.cpp \
	MemoryMonitor.cpp \
	MonitoringProcessLauncher.cpp \
	NativeProcessProtocol.cpp \
	NativeRegisterContext.cpp \
	NativeThreadProtocol.cpp \
	NativeWatchpointList.cpp \
	OptionParser.cpp \
	PipeBase.cpp \
	ProcessLaunchInfo.cpp \
	ProcessRunLock.cpp \
	PseudoTerminal.cpp \
	StreamFile.cpp \
	SocketAddress.cpp \
	Socket.cpp \
	TCPSocket.cpp \
	Terminal.cpp \
	ThreadLauncher.cpp \
	UDPSocket.cpp \
	XML.cpp \
	ZipFileResolver.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Host/common

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
