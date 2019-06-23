# $OpenBSD: Makefile,v 1.2 2019/06/23 22:07:43 patrick Exp $

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
