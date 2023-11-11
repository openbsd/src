# $OpenBSD: Makefile,v 1.4 2023/11/11 18:35:39 robert Exp $

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
	ProcessLauncherPosixFork.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Host/posix

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
