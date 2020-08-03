# $OpenBSD: Makefile,v 1.2 2020/08/03 14:45:30 patrick Exp $

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
	PipePosix.cpp \
	ProcessLauncherPosixFork.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Host/posix

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
