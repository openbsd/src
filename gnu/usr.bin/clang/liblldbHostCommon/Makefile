# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbHostCommon
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	Editline.cpp \
	File.cpp \
	FileCache.cpp \
	FileSystem.cpp \
	GetOptInc.cpp \
	Host.cpp \
	HostInfoBase.cpp \
	HostNativeThreadBase.cpp \
	HostProcess.cpp \
	HostThread.cpp \
	LockFileBase.cpp \
	MainLoop.cpp \
	MonitoringProcessLauncher.cpp \
	NativeBreakpoint.cpp \
	NativeBreakpointList.cpp \
	NativeWatchpointList.cpp \
	NativeProcessProtocol.cpp \
	NativeRegisterContext.cpp \
	NativeThreadProtocol.cpp \
	OptionParser.cpp \
	PipeBase.cpp \
	ProcessRunLock.cpp \
	PseudoTerminal.cpp \
	Socket.cpp \
	SocketAddress.cpp \
	SoftwareBreakpoint.cpp \
	StringConvert.cpp \
	Symbols.cpp \
	TaskPool.cpp \
	TCPSocket.cpp \
	Terminal.cpp \
	ThreadLauncher.cpp \
	XML.cpp \
	UDPSocket.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Host/common

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
