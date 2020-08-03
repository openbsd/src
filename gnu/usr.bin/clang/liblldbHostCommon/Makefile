# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:29 patrick Exp $

LIB=	lldbHostCommon
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	Editline.cpp \
	FileAction.cpp \
	FileCache.cpp \
	FileSystem.cpp \
	File.cpp \
	GetOptInc.cpp \
	Host.cpp \
	HostInfoBase.cpp \
	HostNativeThreadBase.cpp \
	HostProcess.cpp \
	HostThread.cpp \
	LockFileBase.cpp \
	LZMA.cpp \
	MainLoop.cpp \
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
	StringConvert.cpp \
	TaskPool.cpp \
	TCPSocket.cpp \
	Terminal.cpp \
	ThreadLauncher.cpp \
	UDPSocket.cpp \
	XML.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Host/common

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
