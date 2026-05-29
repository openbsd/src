# $OpenBSD: Makefile,v 1.5 2026/05/29 11:18:31 robert Exp $

LIB=	lldbHostPOSIX
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ConnectionFileDescriptorPosix.cpp \
	DomainSocket.cpp \
	FileSystemPosix.cpp \
	HostInfoPosix.cpp \
	HostProcessPosix.cpp \
	HostThreadPosix.cpp \
	LockFilePosix.cpp \
	MainLoopPosix.cpp \
	PipePosix.cpp \
	ProcessLauncherPosixFork.cpp \
	Support.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Host/posix

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
