# $OpenBSD: Makefile,v 1.5 2023/11/11 18:35:39 robert Exp $

LIB=	lldbHostCommon
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	Editline.cpp \
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
	LockFileBase.cpp \
	LZMA.cpp \
	MainLoopBase.cpp \
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
	SocketAddress.cpp \
	Socket.cpp \
	TCPSocket.cpp \
	Terminal.cpp \
	ThreadLauncher.cpp \
	UDPSocket.cpp \
	XML.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Host/common

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
